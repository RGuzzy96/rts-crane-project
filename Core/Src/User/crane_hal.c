/*
 * crane_jal.c
 *
 *  Created on: Nov 26, 2025
 *      Author: ryang
 */

#include "User/crane_hal.h"
#include "User/util.h"
#include "main.h"

static uint8_t verticalState = 0;	// 0 = stop, 1 = up, 2 = down
static uint8_t platformState = 0;	// 0 = stop, 1 = left, 2 = right

#define SERVO_MIN_US    1100	// pulse timing in micro seconds
#define SERVO_MAX_US    1900
#define SERVO_STOP_US   1500


// helper to write pwm pulse width
static void setVerticalServoPulse(uint16_t pulse_us)
{
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, pulse_us);
}

static void setHorizontalServoPulse(uint16_t pulse_us)
{
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, pulse_us);
}

void Crane_HAL_Init(void){
	print_str("Crane_HAL_Init: Starting TIM14 PWM on PA4\r\n");

	HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
	HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
	setVerticalServoPulse(SERVO_STOP_US);
	setHorizontalServoPulse(SERVO_STOP_US);
}

// need to implement the actual HAL GPIO handling for each of these for the crane
void Crane_MoveVerticalUp(void)
{
    if (verticalState != 1)
    {
        print_str("Crane: MOVING VERTICAL UP\r\n");

        // set gpios and start pwm for servo control
        setVerticalServoPulse(SERVO_MAX_US);

        verticalState = 1;
    }
}

void Crane_MoveVerticalDown(void)
{
    if (verticalState != 2)
    {
        print_str("Crane: MOVING VERTICAL DOWN\r\n");

        setVerticalServoPulse(SERVO_MIN_US);

        verticalState = 2;
    }
}

void Crane_StopVertical(void)
{
    if (verticalState != 0)
    {
        print_str("Crane: STOP VERTICAL\r\n");

        setVerticalServoPulse(SERVO_STOP_US);

        verticalState = 0;
    }
}

void Crane_MovePlatformLeft(void)
{
    if (platformState != 1)
    {
        print_str("Crane: ROTATING LEFT\r\n");

        setHorizontalServoPulse(SERVO_MIN_US);

        platformState = 1;
    }
}

void Crane_MovePlatformRight(void)
{
    if (platformState != 2)
    {
        print_str("Crane: ROTATING RIGHT\r\n");

        setHorizontalServoPulse(SERVO_MAX_US);

        platformState = 2;
    }
}

void Crane_StopPlatform(void)
{
    if (platformState != 0)
    {
        print_str("Crane: STOP PLATFORM\r\n");

        setHorizontalServoPulse((SERVO_STOP_US));

        platformState = 0;
    }
}

