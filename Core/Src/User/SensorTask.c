#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "User/SensorTask.h"
#include "User/util.h"

#define SENSOR_TASK_PERIOD_MS   10 			// using 100hz period for task
#define HEIGHT_MIN_CM           1.0f		// clamp lower
#define HEIGHT_MAX_CM           20.0f		// clamp upper

// trigger ultrasonic on pa6
#define TRIG_PORT   GPIOA
#define TRIG_PIN    GPIO_PIN_6

extern TIM_HandleTypeDef htim3;

QueueHandle_t sensorQueue = NULL;
static TaskHandle_t sensorTaskHandle = NULL;

// input capture state machine
volatile uint32_t ic_start = 0;		// rising edge timestamp
volatile uint32_t ic_end   = 0;		// falling edge timestamp
volatile uint8_t  ic_state = 0;   	// 0=rising, 1=falling, 2=complete

// hal callback for tim3 capture (measurement of the echo pulse)
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim) // predefined function name by HAL library, recognizes name but uses our implementation
{
	// only process TIM3 ch1 events
    if (htim->Instance == TIM3 &&
        htim->Channel  == HAL_TIM_ACTIVE_CHANNEL_1)
    {
    	// check if we are looking for rising
        if (ic_state == 0)
        {
            ic_start = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1); // get rising edge timestamp

            // switch to falling edge for capture at end of pulse
            __HAL_TIM_SET_CAPTUREPOLARITY(htim,
                   TIM_CHANNEL_1,
                   TIM_INPUTCHANNELPOLARITY_FALLING);

            ic_state = 1; // set state to look for falling next
        }
        // check if we are looking for fallign edge
        else if (ic_state == 1)
        {
            ic_end = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1); // get falling edge ts

            // switch back to rising for next reading
            __HAL_TIM_SET_CAPTUREPOLARITY(htim,
                   TIM_CHANNEL_1,
                   TIM_INPUTCHANNELPOLARITY_RISING);

            ic_state = 2; // set to complete state
        }
    }
}

// trigger pulse (10 microseconds)
static void ultrasonic_trigger(void)
{
	// write gpio to start pulse
    HAL_GPIO_WritePin(TRIG_PORT, TRIG_PIN, GPIO_PIN_SET);

    // wait for 10 microseconds using TIM3 counter
    uint32_t start = __HAL_TIM_GET_COUNTER(&htim3);
    while ((__HAL_TIM_GET_COUNTER(&htim3) - start) < 10);

    // write gpio to end pulse
    HAL_GPIO_WritePin(TRIG_PORT, TRIG_PIN, GPIO_PIN_RESET);
}

// main function for ultrasonic read in cm
static float ultrasonic_read_cm(void)
{
    ic_state = 0; // set to looking for rising edge first

    // reset counter
    __HAL_TIM_SET_COUNTER(&htim3, 0);

    // capture rising edge
    __HAL_TIM_SET_CAPTUREPOLARITY(&htim3,
         TIM_CHANNEL_1,
         TIM_INPUTCHANNELPOLARITY_RISING);

    // start the IC and the interrupt so it will use our callback fn above
    HAL_TIM_IC_Start_IT(&htim3, TIM_CHANNEL_1);

    // send ping
    ultrasonic_trigger();

    // wait for both edges with timeout (stopped until callback changes ic state to completed)
    uint32_t timeout = 30000;
    while (ic_state != 2 && timeout--)
    {}

    if (ic_state != 2)
        return -1.0f; // timeout or no echo received

    // calculate pulse width
    uint32_t pulse =
        (ic_end >= ic_start)
          ? (ic_end - ic_start)
          : (65535 - ic_start + ic_end);

    // convert pulse width to cm
    float cm = (pulse * 0.0343f) / 2.0f;
    return cm;
}

// main sensor task
static void SensorTask(void *arg)
{
    CraneSensorData data;

    print_str("SensorTask started (TIM3 IC, PB4 ECHO) - 1ms updates\r\n");

    while (1)
    {
    	// get sensor reading of distance to ground in cm
        float d = ultrasonic_read_cm();

        // clamp distance
        if (d < HEIGHT_MIN_CM) d = HEIGHT_MIN_CM;
        if (d > HEIGHT_MAX_CM) d = HEIGHT_MAX_CM;

        // set crane data height in cm and normalized height (0-1)
        data.heightCm = d;
        data.heightNorm =
            (d - HEIGHT_MIN_CM) /
            (HEIGHT_MAX_CM - HEIGHT_MIN_CM);

        // put reading into sensor queue for other tasks to consume
        xQueueOverwrite(sensorQueue, &data);

        vTaskDelay(pdMS_TO_TICKS(SENSOR_TASK_PERIOD_MS)); // delay
    }
}

// initialization function
void SensorTask_Init(void)
{
    // create queue for sensor data to be pushed to
    sensorQueue = xQueueCreate(1, sizeof(CraneSensorData));

    // enable interrupts for input capture
    HAL_TIM_IC_Start_IT(&htim3, TIM_CHANNEL_1);

    xTaskCreate(SensorTask,
		"SensorTask",
		512,
		NULL,
		tskIDLE_PRIORITY + 1,
		&sensorTaskHandle);
}
