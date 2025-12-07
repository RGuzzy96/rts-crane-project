/*
 * ControlTask.h
 *
 *  Created on: Nov 26, 2025
 *      Author: ryang
 */

#ifndef INC_USER_CONTROLTASK_H_
#define INC_USER_CONTROLTASK_H_

#include "FreeRTOS.h"
#include "queue.h"
#include "User/InputTask.h"

void ControlTask_Init(void);

typedef enum {
    MODE_MANUAL = 0,
    MODE_AUTO,
    MODE_CAL
} CraneMode;

void ControlTask_SetMode(CraneMode mode);


#endif /* INC_USER_CONTROLTASK_H_ */
