/*
 * InputTask.c
 *
 *  Created on: Nov 26, 2025
 *      Author: ryang
 */

#include "User/InputTask.h"
#include "User/util.h"

#define INPUT_TASK_PERIOD_MS	20	// 50Hz
#define DEBOUNCE_MS				40	// 2 cycles at 50Hz
#define QUEUE_LENGTH			20

QueueHandle_t inputEventQueue = NULL;

static TaskHandle_t inputTaskHandle = NULL;

// debounce tracking for button (debounce prevents spamming the button from executing handlers too frequently)
static uint8_t lastVertBtn = 0;
static uint8_t lastPlatBtn = 0;
static TickType_t vertBtnLastChange = 0;
static TickType_t platBtnLastChange = 0;

// state for reset button
static uint8_t lastResetBtn = 0;
static TickType_t resetBtnLastChange = 0;

static void InputTask(void *arg);
static void sendEvent(InputEvent evt);

// ---------------------------------------
// external functions
// ---------------------------------------
void InputTask_Init(void){
	// create queue
	inputEventQueue = xQueueCreate(QUEUE_LENGTH, sizeof(InputEvent));

	// create RTOS task for input task
	xTaskCreate(
		InputTask,
		"InputTask",
		512,
		NULL,
		tskIDLE_PRIORITY + 3,
		&inputTaskHandle
	);
}

// ---------------------------------------
// internal functions
// ---------------------------------------

// push event to queue
static void sendEvent(InputEvent evt){
	if (inputEventQueue != NULL){
		xQueueSend(inputEventQueue, &evt, 0);
	}
}


// ---------------------------------------
// input task main loop
// ---------------------------------------

static void InputTask(void *arg){
	TickType_t lastWake = xTaskGetTickCount();

	print_str("Input task started!\r\n");

	for (;;)
	{
		// read the gpios
		uint8_t vertBtn = HAL_GPIO_ReadPin(BUT_VERT_GPIO_Port, BUT_VERT_Pin);
		uint8_t platBtn = HAL_GPIO_ReadPin(BUT_PLAT_GPIO_Port, BUT_PLAT_Pin);

//		char vertStatus[50];
//		sprintf(vertStatus, "Vert button: %d\r\n", vertBtn);
//		print_str(vertStatus);

		uint8_t vertSwUp   = HAL_GPIO_ReadPin(SW_VERT_UP_GPIO_Port, SW_VERT_UP_Pin);
		uint8_t vertSwDown = HAL_GPIO_ReadPin(SW_VERT_DN_GPIO_Port, SW_VERT_DN_Pin);

		uint8_t platSwLeft  = HAL_GPIO_ReadPin(SW_PLAT_L_GPIO_Port, SW_PLAT_L_Pin);
		uint8_t platSwRight = HAL_GPIO_ReadPin(SW_PLAT_R_GPIO_Port, SW_PLAT_R_Pin);

		// uint8_t resetBtn    = HAL_GPIO_ReadPin(USER_Btn_GPIO_Port, USER_Btn_Pin);

		TickType_t now = xTaskGetTickCount();

		// handle vertical button
		if (vertBtn != lastVertBtn && (now - vertBtnLastChange) > pdMS_TO_TICKS(DEBOUNCE_MS)) {
			print_str("Vert button pressed!\r\n");
			vertBtnLastChange = now;

			if (vertBtn){
				sendEvent(EVT_VERT_BUTTON_PRESSED);
			} else {
				sendEvent(EVT_VERT_BUTTON_RELEASED);
			}

			lastVertBtn = vertBtn;
		}

		// handle platform button
		if (platBtn != lastPlatBtn && (now - platBtnLastChange) > pdMS_TO_TICKS(DEBOUNCE_MS)) {
			print_str("Plat button pressed!\r\n");
			platBtnLastChange = now;

			if (platBtn){
				sendEvent(EVT_PLAT_BUTTON_PRESSED);
			} else {
				sendEvent(EVT_PLAT_BUTTON_RELEASED);
			}

			lastPlatBtn = platBtn;
		}

		// handle vertical switch
		if (vertSwUp) {
			sendEvent(EVT_VERT_SWITCH_UP);
		} else if (vertSwDown) {
			sendEvent(EVT_VERT_SWITCH_DOWN);
		}

		// handle platform switch
		if (platSwLeft) {
			sendEvent(EVT_PLAT_SWITCH_LEFT);
		} else if (platSwRight) {
			sendEvent(EVT_PLAT_SWITCH_RIGHT);
		}

		// handle reset button
//		if (resetBtn != lastResetBtn && (now - restBtnLastChange) > pdMS_TO_TICKS(DEBOUNCE_MS)){
//			resetBtnLastChange = now;
//
//			if (resetBtn){
//				sendEvent(EVT_RESET_BUTTON);
//			}
//
//			lastResetBtn = resetBtn;
//		}

		// delay
		vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(INPUT_TASK_PERIOD_MS));
	}
}









