/*
 * SensorTask.c
 *
 *  Created on: Nov 26, 2025
 *      Author: ryang
 */


#include "User/SensorTask.h"
#include "User/util.h"

#define SENSOR_TASK_PERIOD	20	// sample at 50Hz

QueueHandle_t sensorQueue = NULL;
static TaskHandle_t sensorTaskHandle = NULL;

#define HEIGHT_MAX_CM		1.0f 	// need to confirm in lab
#define HEIGHT_MAX_CM		20.0f 	// need to confirm in lab as well, we know highest platform is 15cm

// simulated state for now
static float simHeight = 10.0f;
static float simVelocity = 0.3f;

static void SensorTask(void *arg);

// init
void SensorTask_Init(void){

}

static void SensorTask(void *arg){

}
