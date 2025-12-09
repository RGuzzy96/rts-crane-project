/*
 * InputTask.c
 *
 *  Created on: Nov 26, 2025
 *      Author: ryang
 */

#include "User/InputTask.h"
#include "User/util.h"
#include "User/ControlTask.h"

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
static void ControlTask_SendEvent(InputEvent evt);

// ---------------------------------------
// external functions
// ---------------------------------------
void InputTask_Init(void){
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

		uint8_t vertSwUp   = HAL_GPIO_ReadPin(SW_VERT_UP_GPIO_Port, SW_VERT_UP_Pin);
		uint8_t vertSwDown = HAL_GPIO_ReadPin(SW_VERT_DN_GPIO_Port, SW_VERT_DN_Pin);

		uint8_t platSwLeft  = HAL_GPIO_ReadPin(SW_PLAT_L_GPIO_Port, SW_PLAT_L_Pin);
		uint8_t platSwRight = HAL_GPIO_ReadPin(SW_PLAT_R_GPIO_Port, SW_PLAT_R_Pin);

		uint8_t limitSwLeft = HAL_GPIO_ReadPin(LIM_SW_LEFT_GPIO_Port, LIM_SW_LEFT_Pin);
		uint8_t limitSwRight = HAL_GPIO_ReadPin(LIM_SW_RIGHT_GPIO_Port, LIM_SW_RIGHT_Pin);

		// top and bottom limit switches omitted since functionality proven with left and right and crane track does not go to top anyway
//		uint8_t limitSwTop = HAL_GPIO_ReadPin();
//		uint8_t limitSwBottom = HAL_GPIO_ReadPin();

		TickType_t now = xTaskGetTickCount();

		// LEFT limit
		if (limitSwLeft) {
		    print_str("LIMIT SWITCH HIT: LEFT\r\n");
		    ControlTask_SendEvent(EVT_LIMIT_LEFT_HIT);
		}

		// RIGHT limit
		if (limitSwRight) {
		    print_str("LIMIT SWITCH HIT: RIGHT\r\n");
		    ControlTask_SendEvent(EVT_LIMIT_RIGHT_HIT);
		}
//
//		// TOP limit
//		if (limitSwTop) {
//		    print_str("LIMIT SWITCH HIT: TOP\r\n");
//		    ControlTask_SendEvent(EVT_LIMIT_TOP_HIT);
//		}
//
//		// BOTTOM limit
//		if (limitSwBottom) {
//		    print_str("LIMIT SWITCH HIT: BOTTOM\r\n");
//		    ControlTask_SendEvent(EVT_LIMIT_BOTTOM_HIT);
//		}

		// handle vertical button
		if (vertBtn != lastVertBtn && (now - vertBtnLastChange) > pdMS_TO_TICKS(DEBOUNCE_MS)) {
			print_str("Vert button pressed!\r\n");
			vertBtnLastChange = now;

			if (vertBtn){
				ControlTask_SendEvent(EVT_VERT_BUTTON_PRESSED);
			} else {
				ControlTask_SendEvent(EVT_VERT_BUTTON_RELEASED);
			}

			lastVertBtn = vertBtn;
		}

		// handle platform button
		if (platBtn != lastPlatBtn && (now - platBtnLastChange) > pdMS_TO_TICKS(DEBOUNCE_MS)) {
			print_str("Plat button pressed!\r\n");
			platBtnLastChange = now;

			if (platBtn){
				ControlTask_SendEvent(EVT_PLAT_BUTTON_PRESSED);
			} else {
				ControlTask_SendEvent(EVT_PLAT_BUTTON_RELEASED);
			}

			lastPlatBtn = platBtn;
		}

		// handle vertical switch
		if (vertSwUp) {
			ControlTask_SendEvent(EVT_VERT_SWITCH_UP);
		} else if (vertSwDown) {
			ControlTask_SendEvent(EVT_VERT_SWITCH_DOWN);
		}

		// handle platform switch
		if (platSwLeft) {
			ControlTask_SendEvent(EVT_PLAT_SWITCH_LEFT);
		} else if (platSwRight) {
			ControlTask_SendEvent(EVT_PLAT_SWITCH_RIGHT);
		}

		// handle reset button
//		if (resetBtn != lastResetBtn && (now - restBtnLastChange) > pdMS_TO_TICKS(DEBOUNCE_MS)){
//			resetBtnLastChange = now;
//
//			if (resetBtn){
//				ControlTask_SendEvent(EVT_RESET_BUTTON);
//			}
//
//			lastResetBtn = resetBtn;
//		}

		// delay
		vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(INPUT_TASK_PERIOD_MS));
	}
}









