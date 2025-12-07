/*
 * crane_jal.c
 *
 *  Created on: Nov 26, 2025
 *      Author: ryang
 */

#include "User/crane_hal.h"
#include "User/util.h"

static uint8_t verticalState = 0;	// 0 = stop, 1 = up, 2 = down
static uint8_t platformState = 0;	// 0 = stop, 1 = left, 2 = right

void Crane_HAL_Init(void){
	// nothing right now while we don't have the crane to actually interface with
}

// need to implement the actual HAL GPIO handling for each of these for the crane
void Crane_MoveVerticalUp(void)
{
    if (verticalState != 1)
    {
        print_str("Crane: MOVING VERTICAL UP\r\n");

        // set gpios and start pwm for servo control

        verticalState = 1;
    }
}

void Crane_MoveVerticalDown(void)
{
    if (verticalState != 2)
    {
        print_str("Crane: MOVING VERTICAL DOWN\r\n");
        verticalState = 2;
    }
}

void Crane_StopVertical(void)
{
    if (verticalState != 0)
    {
        print_str("Crane: STOP VERTICAL\r\n");
        verticalState = 0;
    }
}

void Crane_MovePlatformLeft(void)
{
    if (platformState != 1)
    {
        print_str("Crane: ROTATING LEFT\r\n");
        platformState = 1;
    }
}

void Crane_MovePlatformRight(void)
{
    if (platformState != 2)
    {
        print_str("Crane: ROTATING RIGHT\r\n");
        platformState = 2;
    }
}

void Crane_StopPlatform(void)
{
    if (platformState != 0)
    {
        print_str("Crane: STOP PLATFORM\r\n");
        platformState = 0;
    }
}

