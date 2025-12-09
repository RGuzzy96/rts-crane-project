/*
 * crane_hal.c - Buddy's servo pattern integrated (same filename!)
 */
#include "User/crane_hal.h"
#include "User/util.h"
#include "main.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

QueueHandle_t servo_Queue = NULL;
static dir_t last_dir_vertical = DIRSTOP;
static dir_t last_dir_platform = DIRSTOP;

// Global PWM values - adjustable via calibration
uint16_t servo_pwm_forward = 1570;   // Default 80% speed value
uint16_t servo_pwm_backward = 1440;
uint16_t servo_pwm_stop = 1500;


static void start_servo_fwd(TIM_HandleTypeDef *servo) {
    if (servo == &htim1) { // Vertical CH1
        __HAL_TIM_SET_COMPARE(servo, TIM_CHANNEL_1, servo_pwm_forward);  // USE VARIABLE
        print_str("Crane: MOVING VERTICAL UP\r\n");
    } else { // Platform CH2
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, servo_pwm_forward);
        print_str("Crane: ROTATING RIGHT\r\n");
    }
}

static void start_servo_bck(TIM_HandleTypeDef *servo) {
    if (servo == &htim1) { // Vertical CH1
        __HAL_TIM_SET_COMPARE(servo, TIM_CHANNEL_1, servo_pwm_backward);
        print_str("Crane: MOVING VERTICAL DOWN\r\n");
    } else { // Platform CH2
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, servo_pwm_backward);
        print_str("Crane: ROTATING LEFT\r\n");
    }
}

static void stop_servo(TIM_HandleTypeDef *servo){
    if (servo == &htim1) {  // Vertical CH1
        __HAL_TIM_SET_COMPARE(servo, TIM_CHANNEL_1, 1500);
        print_str("Crane: STOP VERTICAL\r\n");
    } else {  // Platform CH2
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, 1500);
        print_str("Crane: STOP PLATFORM\r\n");
    }
}

// Your EXISTING API (ControlTask calls unchanged!)
void Crane_MoveVerticalUp(void) {
    servo_cmd_t cmd = {&htim1, DIRUP};
    if (servo_Queue) xQueueSend(servo_Queue, &cmd, 0);
}

void Crane_MoveVerticalDown(void) {
    servo_cmd_t cmd = {&htim1, DIRDOWN};
    if (servo_Queue) xQueueSend(servo_Queue, &cmd, 0);
}

void Crane_StopVertical(void) {
    servo_cmd_t cmd = {&htim1, DIRSTOP};
    if (servo_Queue) xQueueSend(servo_Queue, &cmd, 0);
}

void Crane_MovePlatformRight(void) {
    servo_cmd_t cmd = {NULL, DIRUP};
    if (servo_Queue) xQueueSend(servo_Queue, &cmd, 0);
}

void Crane_MovePlatformLeft(void) {
    servo_cmd_t cmd = {NULL, DIRDOWN};
    if (servo_Queue) xQueueSend(servo_Queue, &cmd, 0);
}

void Crane_StopPlatform(void) {
    servo_cmd_t cmd = {NULL, DIRSTOP};
    if (servo_Queue) xQueueSend(servo_Queue, &cmd, 0);
}

static void servo_controller_task(void) {
    servo_cmd_t current_cmd;

    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);  // Vertical
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);  // Platform

    servo_Queue = xQueueCreate(10, sizeof(servo_cmd_t));
    print_str("Servo controller started\r\n");

    while (xQueueReceive(servo_Queue, &current_cmd, portMAX_DELAY) == pdPASS) {
        // VERTICAL (CH1)
        if (current_cmd.htim == &htim1) {
            if (last_dir_vertical == DIRSTOP) {
                if (current_cmd.servodir == DIRUP) start_servo_fwd(&htim1);
                else if (current_cmd.servodir == DIRDOWN) start_servo_bck(&htim1);
                last_dir_vertical = current_cmd.servodir;
            } else if ((last_dir_vertical == DIRUP && current_cmd.servodir == DIRDOWN) ||
                       (last_dir_vertical == DIRDOWN && current_cmd.servodir == DIRUP)) {
                stop_servo(&htim1);
                last_dir_vertical = DIRSTOP;
            } else if (current_cmd.servodir == DIRSTOP) {
                stop_servo(&htim1);
                last_dir_vertical = DIRSTOP;
            }
        }
        // PLATFORM (CH2)
        else {
            if (last_dir_platform == DIRSTOP) {
                if (current_cmd.servodir == DIRUP) start_servo_fwd(NULL);
                else if (current_cmd.servodir == DIRDOWN) start_servo_bck(NULL);
                last_dir_platform = current_cmd.servodir;
            } else if ((last_dir_platform == DIRUP && current_cmd.servodir == DIRDOWN) ||
                       (last_dir_platform == DIRDOWN && current_cmd.servodir == DIRUP)) {
                stop_servo(NULL);
                last_dir_platform = DIRSTOP;
            } else if (current_cmd.servodir == DIRSTOP) {
                stop_servo(NULL);
                last_dir_platform = DIRSTOP;
            }
        }
    }
}

void Crane_HAL_Init(void) {
    print_str("Crane HAL: Starting servo task...\r\n");
    xTaskCreate(servo_controller_task, "ServoTask", 256, NULL, tskIDLE_PRIORITY + 1, NULL);
}


