/*
 * ControlTask.c
 *
 *  Created on: Nov 26, 2025
 *      Author: ryang
 */

#include "User/ControlTask.h"
#include "User/util.h"
#include "User/crane_hal.h"

#define CONTROL_TASK_PERIOD_MS	20

static QueueHandle_t controlQueue;
static TaskHandle_t controlTaskHandle;

static void ControlTask(void *arg);

typedef enum {
	DIR_NONE = 0,
	DIR_UP,
	DIR_DOWN,
	DIR_LEFT,
	DIR_RIGHT
} Direction;

// state for the vertical axis
static Direction vertSwitchDir = DIR_NONE;
static uint8_t vertButtonHeld = 0;
static Direction vertCurrentMotion = DIR_NONE;

// state for the platform axis
static Direction platSwitchDir = DIR_NONE;
static uint8_t platButtonHeld = 0;
static Direction platCurrentMotion = DIR_NONE;

// receive events from InputTask
void ControlTask_SendEvent(InputEvent evt){
	if (controlQueue) {
		xQueueSend(controlQueue, &evt, 0);
	}
}

void ControlTask_Init(void){
	controlQueue = xQueueCreate(20, sizeof(InputEvent));

	xTaskCreate(
		ControlTask,
		"ControlTask",
		512,
		NULL,
		tskIDLE_PRIORITY + 2,
		&controlTaskHandle
	);
}

// helper for updating the control for vertical motion
static void updateVerticalMotion(void){
	// check if we are not holding the vertical button, or if the switch is set for no vertical direction
	if(!vertButtonHeld || vertSwitchDir == DIR_NONE){
		// stop vertical motion if there is any
		if(vertCurrentMotion != DIR_NONE){
			Crane_StopVertical();
			vertCurrentMotion = DIR_NONE;
		}
		return;
	}

	// check if the button is held and there is a direction chosen with the switch
	if(vertSwitchDir == DIR_UP && vertCurrentMotion != DIR_UP){
		Crane_MoveVerticalUp();
		vertCurrentMotion = DIR_UP;
	} else if(vertSwitchDir == DIR_DOWN && vertCurrentMotion != DIR_DOWN){
		Crane_MoveVerticalDown();
		vertCurrentMotion = DIR_DOWN;
	}
}

// helper for updating the control for platform motion
static void updatePlatformMotion(void){
	// check if we are not holding the platform button, or if the switch is set for no platform direction
	if(!platButtonHeld || platSwitchDir == DIR_NONE){
		// stop platform motion if there is any
		if(platCurrentMotion != DIR_NONE){
			Crane_StopPlatform();
			platCurrentMotion = DIR_NONE;
		}
		return;
	}

	// check if the button is held and there is a direction chosen with the switch
	if(platSwitchDir == DIR_LEFT && platCurrentMotion != DIR_LEFT){
		Crane_MovePlatformLeft();
		platCurrentMotion = DIR_LEFT;
	} else if(platSwitchDir == DIR_RIGHT && platCurrentMotion != DIR_RIGHT){
		Crane_MovePlatformRight();
		platCurrentMotion = DIR_RIGHT;
	}
}

static void ControlTask(void *arg){
	print_str("ControlTask started!\r\n");

	TickType_t lastWake = xTaskGetTickCount();

	InputEvent evt;

	for (;;){
		while (xQueueReceive(controlQueue, &evt, 0)){
			switch (evt){
			case EVT_VERT_BUTTON_PRESSED:
				print_str("Control: Vertical BUTTON pressed\r\n");
				vertButtonHeld = 1;
				break;

			case EVT_VERT_BUTTON_RELEASED:
				print_str("Control: Vertical BUTTON released\r\n");
				vertButtonHeld = 0;
				break;

			case EVT_PLAT_BUTTON_PRESSED:
				print_str("Control: Platform BUTTON pressed\r\n");
				platButtonHeld = 1;
				break;

			case EVT_PLAT_BUTTON_RELEASED:
				print_str("Control: Platform BUTTON released\r\n");
				platButtonHeld = 0;
				break;

			case EVT_VERT_SWITCH_UP:
				vertSwitchDir = DIR_UP;
				break;

			case EVT_VERT_SWITCH_DOWN:
				vertSwitchDir = DIR_DOWN;
				break;

			case EVT_VERT_SWITCH_OFF:
				vertSwitchDir = DIR_NONE;
				break;

			case EVT_PLAT_SWITCH_LEFT:
				print_str("Control: Platform LEFT\r\n");
				platSwitchDir = DIR_LEFT;
				break;

			case EVT_PLAT_SWITCH_RIGHT:
				print_str("Control: Platform RIGHT\r\n");
				platSwitchDir = DIR_RIGHT;
				break;

			case EVT_PLAT_SWITCH_OFF:
				print_str("Control: Platform NONE\r\n");
				platSwitchDir = DIR_NONE;
				break;

			case EVT_RESET_BUTTON:
				print_str("Control: RESET\r\n");
				vertSwitchDir = DIR_NONE;
				vertButtonHeld = 0;
				platSwitchDir = DIR_NONE;
				platButtonHeld = 0;
				Crane_StopVertical();
				Crane_StopPlatform();
				vertCurrentMotion = DIR_NONE;
				platCurrentMotion = DIR_NONE;
				break;

			default:
				print_str("Control: Unknown event\r\n");
				break;
			}
		}

		// apply motion on each cycle
		updateVerticalMotion();
		updatePlatformMotion();

		vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(CONTROL_TASK_PERIOD_MS));
	}
}
