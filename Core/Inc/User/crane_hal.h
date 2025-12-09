/*
 * crane_hal.h
 *
 *  Created on: Nov 26, 2025
 *      Author: ryang
 */

#ifndef CRANE_HAL_H_
#define CRANE_HAL_H_

#include "main.h"
#include "FreeRTOS.h"
#include "queue.h"

// Servo direction enum
typedef enum {
    DIRSTOP = 0,
    DIRUP,
    DIRDOWN
} dir_t;

extern uint16_t servo_pwm_forward;
extern uint16_t servo_pwm_backward;
extern uint16_t servo_pwm_stop;


// Servo command struct
typedef struct {
    TIM_HandleTypeDef* htim;
    dir_t servodir;
} servo_cmd_t;

// External queue for servo commands
extern QueueHandle_t servo_Queue;

// HAL initialization
void Crane_HAL_Init(void);

// Vertical servo control
void Crane_MoveVerticalUp(void);
void Crane_MoveVerticalDown(void);
void Crane_StopVertical(void);

// Platform servo control
void Crane_MovePlatformRight(void);
void Crane_MovePlatformLeft(void);
void Crane_StopPlatform(void);

#endif /* CRANE_HAL_H_ */
