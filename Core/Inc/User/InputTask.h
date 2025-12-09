/*
 * InputTask.h
 *
 *  Created on: Nov 26, 2025
 *      Author: ryang
 */

#ifndef INC_USER_INPUTTASK_H_
#define INC_USER_INPUTTASK_H_

#include "main.h"

#include "FreeRTOS.h"
#include "queue.h"

typedef enum {
	EVT_VERT_BUTTON_PRESSED,
	EVT_VERT_BUTTON_RELEASED,
	EVT_PLAT_BUTTON_PRESSED,
	EVT_PLAT_BUTTON_RELEASED,
	EVT_VERT_SWITCH_UP,
	EVT_VERT_SWITCH_DOWN,
	EVT_VERT_SWITCH_OFF,
	EVT_PLAT_SWITCH_LEFT,
	EVT_PLAT_SWITCH_RIGHT,
	EVT_PLAT_SWITCH_OFF,
	EVT_RESET_BUTTON,
	EVT_LIMIT_TOP_HIT,
	EVT_LIMIT_BOTTOM_HIT,
	EVT_LIMIT_LEFT_HIT,
	EVT_LIMIT_RIGHT_HIT
} InputEvent;

extern QueueHandle_t InputEventQuery;

void InputTask_Init(void);
void InputTask_Start(void);

#endif /* INC_USER_INPUTTASK_H_ */
