/*
 * SensorTask.h
 *
 *  Created on: Nov 26, 2025
 *      Author: ryang
 */

#ifndef INC_USER_SENSORTASK_H_
#define INC_USER_SENSORTASK_H_

#include <stdint.h>
#include "FreeRTOS.h"
#include "queue.h"

typedef struct {
	float heightCm;			// raw height converted to centimeters (not sure yet what the measurement comes in as)
	float heightNorm;		// normalized 0-1
} CraneSensorData;

void SensorTask_Init(void);

// external queue so other tasks can consume sensor data
extern QueueHandle_t sensorQueue;

#endif /* INC_USER_SENSORTASK_H_ */
