/*
 * main_user.c
 *
 *  Created on: Aug 8, 2022
 *      Author: Andre Hendricks
 */

#include <stdio.h>
#include <string.h>


//STM32 generated header files
#include "main.h"

//User generated header files
#include "User/main_user.h"
#include "User/util.h"
#include "User/InputTask.h"
#include "User/ControlTask.h"
#include "User/crane_hal.h"
#include "User/uart.h"
#include "User/SensorTask.h"



//Required FreeRTOS header files
#include "FreeRTOS.h"
#include "task.h"

char main_string[50];
uint32_t main_counter = 0;
static TaskHandle_t commandTaskHandle;
static char cmdBuffer[50];
static int cmdIndex = 0;

// simple task used for logging
static void main_task(void *param){

    CraneSensorData s;

	while(1){
		 if (xQueueReceive(sensorQueue, &s, 0) == pdPASS)
		        {
		            char buf[64];
		            sprintf(buf, "Distance: %.2f cm   Normalized: %.2f\r\n", s.heightCm, s.heightNorm);
		            print_str(buf);
		        }

		vTaskDelay(10000/portTICK_RATE_MS);
	}
}

void main_user(){
	util_init();

	xTaskCreate(main_task,"Main Task", configMINIMAL_STACK_SIZE + 100, NULL, tskIDLE_PRIORITY + 2, NULL);

	// initialize tasks
	Crane_HAL_Init();
	InputTask_Init();
	ControlTask_Init();
	UART_StartCommandTask();
	SensorTask_Init();

	// start scheduler
	vTaskStartScheduler();

	while(1);
}
