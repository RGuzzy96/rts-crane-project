/*
 * InputTask.h
 *
 *  Created on: Nov 26, 2025
 *      Author: ryang
 */

#ifndef INC_USER_INPUTTASK_H_
#define INC_USER_INPUTTASK_H_

#include "queue.h"

typedef enum {
	EVT_VERT_BUTTON_PRESSED,
	EVT_VERT_BUTTON_RELEASED,
	EVT_PLAT_BUTTON_PRESSED,
	EVT_PLAT_BUTTON_RELEASED,
	EVT_VERT_SWITCH_UP,
	EVT_VERT_SWITCH_DOWN,
	EVT_PLAT_SWITCH_LEFT,
	EVT_PLAT_SWITCH_RIGHT,
	EVT_RESET_BUTTON,
} InputEvent;

extern QueueHandle_t InputEventQuery;

void InputTask_Init(void);
void InputTask_Start(void);

#endif /* INC_USER_INPUTTASK_H_ */
