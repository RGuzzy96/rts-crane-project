#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "User/SensorTask.h"
#include "User/util.h"

// ---------------- CONFIG ------------------

#define SENSOR_TASK_PERIOD_MS   10        // FIXED: Was 20ms, now 1ms (1000 Hz sampling)
#define HEIGHT_MIN_CM           1.0f
#define HEIGHT_MAX_CM           20.0f

// TRIG on PA6
#define TRIG_PORT   GPIOA
#define TRIG_PIN    GPIO_PIN_6

extern TIM_HandleTypeDef htim3;

// ------------------------------------------

QueueHandle_t sensorQueue = NULL;
static TaskHandle_t sensorTaskHandle = NULL;

// Input capture state machine
volatile uint32_t ic_start = 0;
volatile uint32_t ic_end   = 0;
volatile uint8_t  ic_state = 0;   // 0=rising, 1=falling, 2=complete

// ------------------------------------------
// HAL Callback for TIM3 Input Capture
// ------------------------------------------
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM3 &&
        htim->Channel  == HAL_TIM_ACTIVE_CHANNEL_1)
    {
        if (ic_state == 0)
        {
            ic_start = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);

            // Switch to falling edge
            __HAL_TIM_SET_CAPTUREPOLARITY(htim,
                   TIM_CHANNEL_1,
                   TIM_INPUTCHANNELPOLARITY_FALLING);

            ic_state = 1;
        }
        else if (ic_state == 1)
        {
            ic_end = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);

            // Switch back to rising for next reading
            __HAL_TIM_SET_CAPTUREPOLARITY(htim,
                   TIM_CHANNEL_1,
                   TIM_INPUTCHANNELPOLARITY_RISING);

            ic_state = 2; // done
        }
    }
}

// ------------------------------------------
// TRIGGER PULSE
// ------------------------------------------
static void ultrasonic_trigger(void)
{
    HAL_GPIO_WritePin(TRIG_PORT, TRIG_PIN, GPIO_PIN_SET);

    uint32_t start = __HAL_TIM_GET_COUNTER(&htim3);
    while ((__HAL_TIM_GET_COUNTER(&htim3) - start) < 10); // 10µs

    HAL_GPIO_WritePin(TRIG_PORT, TRIG_PIN, GPIO_PIN_RESET);
}

// ------------------------------------------
// MAIN ULTRASONIC READ FUNCTION
// ------------------------------------------
static float ultrasonic_read_cm(void)
{
    ic_state = 0;

    // Reset counter
    __HAL_TIM_SET_COUNTER(&htim3, 0);

    // Start with rising edge
    __HAL_TIM_SET_CAPTUREPOLARITY(&htim3,
         TIM_CHANNEL_1,
         TIM_INPUTCHANNELPOLARITY_RISING);

    // Start IC + interrupt
    HAL_TIM_IC_Start_IT(&htim3, TIM_CHANNEL_1);

    // Send 10µs pulse
    ultrasonic_trigger();

    // Wait for rising + falling edges captured
    uint32_t timeout = 30000;
    while (ic_state != 2 && timeout--)
    {
        // optional vTaskDelay(1);
    }

    if (ic_state != 2)
        return -1.0f; // timeout or no echo

    // Pulse width
    uint32_t pulse =
        (ic_end >= ic_start)
          ? (ic_end - ic_start)
          : (65535 - ic_start + ic_end);

    float cm = (pulse * 0.0343f) / 2.0f;
    return cm;
}

// ------------------------------------------
// SENSOR TASK
// ------------------------------------------
static void SensorTask(void *arg)
{
    CraneSensorData data;

    print_str("SensorTask started (TIM3 IC, PB4 ECHO) - 1ms updates\r\n");

    while (1)
    {
        float d = ultrasonic_read_cm();

        // Clamp distance
        if (d < HEIGHT_MIN_CM) d = HEIGHT_MIN_CM;
        if (d > HEIGHT_MAX_CM) d = HEIGHT_MAX_CM;

        data.heightCm = d;
        data.heightNorm =
            (d - HEIGHT_MIN_CM) /
            (HEIGHT_MAX_CM - HEIGHT_MIN_CM);

        xQueueOverwrite(sensorQueue, &data);

        vTaskDelay(pdMS_TO_TICKS(SENSOR_TASK_PERIOD_MS));
    }
}

// ------------------------------------------
// INITIALIZATION FUNCTION
// ------------------------------------------
void SensorTask_Init(void)
{
    // Create queue
    sensorQueue = xQueueCreate(1, sizeof(CraneSensorData));

    // Start TIM3 Input Capture interrupts
    HAL_TIM_IC_Start_IT(&htim3, TIM_CHANNEL_1);

    // Create the FreeRTOS task
    xTaskCreate(SensorTask,
                "SensorTask",
                512,
                NULL,
                tskIDLE_PRIORITY + 1,
                &sensorTaskHandle);
}
