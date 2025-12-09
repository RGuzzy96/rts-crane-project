/*
 * ControlTask.c
 *
 *  Created on: Nov 26, 2025
 *      Author: ryang
 */

#include "User/ControlTask.h"
#include "User/util.h"
#include "User/crane_hal.h"
#include "User/SensorTask.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <stdio.h>

#define CONTROL_TASK_PERIOD_MS 20

// auto mode constants
#define AUTO_BASE_CM         6.0f   // first target height
#define AUTO_TOL_CM           0.5f   // tolerance around target

// state machine for auto mode
static uint8_t auto_step = 0;
static TickType_t auto_step_start = 0;

// manual mode states
typedef enum {
    DIR_NONE = 0,
    DIR_UP,
    DIR_DOWN,
    DIR_LEFT,
    DIR_RIGHT
} Direction;

static QueueHandle_t controlQueue;
static TaskHandle_t controlTaskHandle;

static CraneMode currentMode = MODE_MANUAL;

// vertical axis state
static Direction vertSwitchDir = DIR_NONE;
static uint8_t vertButtonHeld = 0;
static Direction vertCurrentMotion = DIR_NONE;

// platform axis state
static Direction platSwitchDir = DIR_NONE;
static uint8_t platButtonHeld = 0;
static Direction platCurrentMotion = DIR_NONE;

// auto mode state tracking
static uint8_t autoStateEntry = 1;

// external sensor queue for sensor readings
extern QueueHandle_t sensorQueue;

static void ControlTask(void *arg);
static void updateVerticalMotion(void);
static void updatePlatformMotion(void);
static void updateAutoMode(void);

// helper for sending events to control queue
void ControlTask_SendEvent(InputEvent evt)
{
    if (controlQueue) {
        xQueueSend(controlQueue, &evt, 0);
    }
}

// helper for setting mode for control operations
void ControlTask_SetMode(CraneMode mode)
{
    currentMode = mode; // change mode

    // full reset of motion state when mode changes
    vertButtonHeld = 0;
    platButtonHeld = 0;
    vertSwitchDir = DIR_NONE;
    platSwitchDir = DIR_NONE;
    vertCurrentMotion = DIR_NONE;
    platCurrentMotion = DIR_NONE;

    // stop movement of crane
    Crane_StopVertical();
    Crane_StopPlatform();

    // reset auto states
    if (mode == MODE_AUTO) {
        auto_step = 0;
        autoStateEntry = 1;
    }

    // status message for mode change
    switch (mode) {
        case MODE_MANUAL:
            print_str("Mode: MANUAL\r\n");
            break;
        case MODE_AUTO:
            print_str("Mode: AUTO\r\n");
            break;
        case MODE_CAL:
            print_str("Mode: CAL\r\n");
        case MODE_BLOCKED:
        	print_str("Mode: BLOCKED\r\n");
            break;
    }
}

void ControlTask_Init(void)
{
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

// manual mode vertical motion helper
static void updateVerticalMotion(void)
{
    // stop if button not held or no switch direction
	// (this was implemented with no switch direction as an option when simulated at home, but maintained as would be useful if
		// further developed with 3 state switch)
    if (!vertButtonHeld || vertSwitchDir == DIR_NONE) {
        if (vertCurrentMotion != DIR_NONE) {
            Crane_StopVertical();
            vertCurrentMotion = DIR_NONE;
        }
        return;
    }

    // move based on switch direction
    if (vertSwitchDir == DIR_UP && vertCurrentMotion != DIR_UP) {
        Crane_MoveVerticalUp();
        vertCurrentMotion = DIR_UP;
    } else if (vertSwitchDir == DIR_DOWN && vertCurrentMotion != DIR_DOWN) {
        Crane_MoveVerticalDown();
        vertCurrentMotion = DIR_DOWN;
    }
}

// manual mode platform movement helper
static void updatePlatformMotion(void)
{
    // stop if button not held or no switch direction
	// (this was implemented with no switch direction as an option when simulated at home, but maintained as would be useful if
	// further developed with 3 state switch)
    if (!platButtonHeld || platSwitchDir == DIR_NONE) {
        if (platCurrentMotion != DIR_NONE) {
            Crane_StopPlatform();
            platCurrentMotion = DIR_NONE;
        }
        return;
    }

    // move based on switch direction
    if (platSwitchDir == DIR_LEFT && platCurrentMotion != DIR_LEFT) {
        Crane_MovePlatformLeft();
        platCurrentMotion = DIR_LEFT;
    } else if (platSwitchDir == DIR_RIGHT && platCurrentMotion != DIR_RIGHT) {
        Crane_MovePlatformRight();
        platCurrentMotion = DIR_RIGHT;
    }
}


// auto mode state machine
static void updateAutoMode(void)
{
    CraneSensorData s;
    if (xQueueReceive(sensorQueue, &s, 0) != pdPASS) {
        return;  // return if no fresh sensor reading
    }

    float h = s.heightCm;

    // check if manual input received, if so, switch to manual mode
    InputEvent evt;
    if (xQueueReceive(controlQueue, &evt, 0) == pdPASS) {
        print_str("AUTO: Manual input detected! Resetting to MANUAL mode\r\n");
        Crane_StopVertical();
        Crane_StopPlatform();
        auto_step = 0;
        autoStateEntry = 1;
        ControlTask_SetMode(MODE_MANUAL);
        return;  // exit auto mode state machine
    }


    switch (auto_step) {

    // step 0: raise to first platform
    case 0:
    {
        if (autoStateEntry) {
            print_str("AUTO: Step0 -> first platform baseline\r\n");
            autoStateEntry = 0;
        }

        // check if below the first platform (within tolerance)
        if (h < AUTO_BASE_CM - AUTO_TOL_CM) {
        	// move up
            Crane_MoveVerticalDown(); // our function definitions got flipped at the crane_hal level
        } else if (h > AUTO_BASE_CM + AUTO_TOL_CM) {
        	// otherwise, move down
            Crane_MoveVerticalUp();
        } else {
        	// stop if within tolerance
            Crane_StopVertical();
            print_str("AUTO: first platform reached, swing RIGHT 600ms\r\n");
            auto_step = 1;
            auto_step_start = xTaskGetTickCount();
            autoStateEntry = 1;
        }
        break;
    }

    // step 1: rotate platform right for 600ms
    case 1:
        if (autoStateEntry) {
            print_str("AUTO: Step1 -> RIGHT 600ms\r\n");
            Crane_MovePlatformRight();
            auto_step_start = xTaskGetTickCount();
            autoStateEntry = 0;
        }
        if (xTaskGetTickCount() - auto_step_start >= pdMS_TO_TICKS(600)) {
            Crane_StopPlatform();
            print_str("AUTO: Right 600ms done, now UP +2cm\r\n");
            auto_step = 2;
            autoStateEntry = 1;
        }
        break;

    // step 2 go up 2cm to pick up freight
    case 2:
    {
        float target = AUTO_BASE_CM + 2.0f;
        if (autoStateEntry) {
            print_str("AUTO: Step2 -> UP 2cm (to 12cm)\r\n");
            autoStateEntry = 0;
        }
        if (h < target - AUTO_TOL_CM) {
            Crane_MoveVerticalDown();  // again, function name is flipped
        } else {
            Crane_StopVertical();
            print_str("AUTO: 12 cm reached, return to CENTER (LEFT 600ms)\r\n");
            auto_step = 3;
            auto_step_start = xTaskGetTickCount();
            autoStateEntry = 1;
        }
        break;
    }

    // step 3: retrun to center, rotating left for 600ms
    case 3:
        if (autoStateEntry) {
            print_str("AUTO: Step3 -> LEFT 600ms (back to center)\r\n");
            Crane_MovePlatformLeft();
            auto_step_start = xTaskGetTickCount();
            autoStateEntry = 0;
        }
        if (xTaskGetTickCount() - auto_step_start >= pdMS_TO_TICKS(600)) {
            Crane_StopPlatform();
            print_str("AUTO: Centered, now UP +5cm\r\n");
            auto_step = 4;
            autoStateEntry = 1;
        }
        break;

    // step 4: go up to 14.5cm
    case 4:
    {
        float target = AUTO_BASE_CM + 8.75f;  // 14.5 cm
        if (autoStateEntry) {
            print_str("AUTO: Step4 -> UP 5cm (to 15cm)\r\n");
            autoStateEntry = 0;
        }
        if (h < target - AUTO_TOL_CM) {
            Crane_MoveVerticalDown();
        } else {
            Crane_StopVertical();
            print_str("AUTO: 15 cm reached, LEFT 600ms\r\n");
            auto_step = 5;
            auto_step_start = xTaskGetTickCount();
            autoStateEntry = 1;
        }
        break;
    }

    // step 5: rotate left for 600 ms (to hover over top platform)
    case 5:
        if (autoStateEntry) {
            print_str("AUTO: Step5 -> LEFT 600ms\r\n");
            Crane_MovePlatformLeft();
            auto_step_start = xTaskGetTickCount();
            autoStateEntry = 0;
        }
        if (xTaskGetTickCount() - auto_step_start >= pdMS_TO_TICKS(600)) {
            Crane_StopPlatform();
            print_str("AUTO: Left 600ms done, DOWN 5cm\r\n");
            auto_step = 6;
            autoStateEntry = 1;
        }
        break;

	// Step 6: Go DOWN 2 cm (from 15 cm to ~13 cm)
	case 6:
	{
		float target = 10.0f;  // 15cm - 2cm = 13cm
		if (autoStateEntry) {
			print_str("AUTO: Step6 -> DOWN 2cm (to 13cm)\r\n");
			autoStateEntry = 0;
		}
		if (h > target + AUTO_TOL_CM) {
			Crane_MoveVerticalUp();
		} else {
			Crane_StopVertical();
			print_str("AUTO: Reached 13cm, return to CENTER (RIGHT 600ms)\r\n");
			auto_step = 7;
			auto_step_start = xTaskGetTickCount();
			autoStateEntry = 1;
		}
		break;
	}

	// step 7: rotate back to center (600ms again)
	case 7:
		if (autoStateEntry) {
			print_str("AUTO: Step7 -> RIGHT 600ms (back to center)\r\n");
			Crane_MovePlatformRight();
			auto_step_start = xTaskGetTickCount();
			autoStateEntry = 0;
		}
		if (xTaskGetTickCount() - auto_step_start >= pdMS_TO_TICKS(600)) {
			Crane_StopPlatform();
			print_str("AUTO: Centered, DOWN fully to 3cm (PICKUP)\r\n");
			auto_step = 8;
			autoStateEntry = 1;
		}
		break;

	// step 8: go down fully to 2cm (we get limited by hardware at around 2)
	case 8:
	{
		float target = 2.0f;
		if (autoStateEntry) {
			print_str("AUTO: Step8 -> DOWN to 2cm (PICKUP)\r\n");
			autoStateEntry = 0;
		}
		if (h > target + AUTO_TOL_CM) {
			Crane_MoveVerticalUp();
		} else {
			Crane_StopVertical();
			print_str("AUTO: Reached 2cm, AUTO sequence COMPLETE!\r\n");
			auto_step = 9;
			autoStateEntry = 1;
		}
		break;
	}

	//step 9: finish, stop movement and reset states
	case 9:
		Crane_StopVertical();
		Crane_StopPlatform();
		print_str("AUTO: Full sequence complete. Returning to MANUAL\r\n");
		ControlTask_SetMode(MODE_MANUAL);
		auto_step = 0;
		autoStateEntry = 1;
		break;


    default:
        auto_step = 0;
        autoStateEntry = 1;
        break;
    }
}

// main control task
static void ControlTask(void *arg)
{
    print_str("ControlTask started!\r\n");
    TickType_t lastWake = xTaskGetTickCount();
    InputEvent evt;

    for (;;) {
        // process input events entering the control queue
        while (xQueueReceive(controlQueue, &evt, 0)) {
            // ignore button/switch events unless we're in manual mode
            if (currentMode != MODE_MANUAL) {
                // reset button should still work
                if (evt == EVT_RESET_BUTTON) {
                    Crane_StopVertical();
                    Crane_StopPlatform();
                    vertButtonHeld = 0;
                    platButtonHeld = 0;
                    vertSwitchDir = DIR_NONE;
                    platSwitchDir = DIR_NONE;
                    vertCurrentMotion = DIR_NONE;
                    platCurrentMotion = DIR_NONE;
                }
                continue;  // skip other events
            }

            //handle manual mode events
            switch (evt) {
				case EVT_LIMIT_TOP_HIT:
					Crane_StopVertical();
					ControlTask_SetMode(MODE_BLOCKED);
					break;

				case EVT_LIMIT_BOTTOM_HIT:
					Crane_StopVertical();
					ControlTask_SetMode(MODE_BLOCKED);
					break;

				case EVT_LIMIT_LEFT_HIT:
					Crane_StopPlatform();
					ControlTask_SetMode(MODE_BLOCKED);
					break;

				case EVT_LIMIT_RIGHT_HIT:
					Crane_StopPlatform();
					ControlTask_SetMode(MODE_BLOCKED);
					break;

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
                    platSwitchDir = DIR_LEFT;
                    break;
                case EVT_PLAT_SWITCH_RIGHT:
                    platSwitchDir = DIR_RIGHT;
                    break;
                case EVT_PLAT_SWITCH_OFF:
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
                    break;
            }
        }

        // apply motion based on mode
        if (currentMode == MODE_MANUAL) {
            updateVerticalMotion();
            updatePlatformMotion();
        } else if (currentMode == MODE_AUTO) {
            updateAutoMode();
        } else if (currentMode == MODE_CAL) {
            updateCalMode();
        }

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(CONTROL_TASK_PERIOD_MS));
    }
}

// control for calibration mode
void updateCalMode(void)
{
    CraneSensorData s;
    if (xQueueReceive(sensorQueue, &s, 0) != pdPASS) {
        return; // ignore if no valid reading
    }

    float h = s.heightCm;

    switch (auto_step) {

    // step 0: test first speed upwards
    case 0:
        if (autoStateEntry) {
            servo_pwm_backward = 1320; // hardcoded starting point
            auto_step_start = xTaskGetTickCount();
            autoStateEntry = 0;
        }

        // check if nelow first target height of 4cm
        if (h < 4.0f - AUTO_TOL_CM) {
            Crane_MoveVerticalDown(); // move up (flipped function name)
        } else if (h > 4.0f + AUTO_TOL_CM) {
            Crane_MoveVerticalUp(); // move down if above
        } else {
        	// stop if we have reached here and calculate results from first test
            Crane_StopVertical();
            TickType_t elapsed_ms = xTaskGetTickCount() - auto_step_start;
            float elapsed_sec = elapsed_ms / 1000.0f;
            float speed = 4.0f / elapsed_sec;
            char buf[100];
            sprintf(buf, "CAL: PWM %d -> Speed: %.2f cm/sec\r\n", servo_pwm_backward, speed);
            print_str(buf);

            servo_pwm_backward = 1400; // hardcoded secondary value, but ideally this should be selected based off of how far off our speed was
            auto_step = 1;
            auto_step_start = xTaskGetTickCount();
            autoStateEntry = 1;
        }
        break;

    // step 1: test second vertical speed (going up to 8cm)
    case 1:
        if (h < 8.0f - AUTO_TOL_CM) {
            Crane_MoveVerticalDown();
        } else if (h > 8.0f + AUTO_TOL_CM) {
            Crane_MoveVerticalUp();
        } else {
        	// stop once we reach height within tolerance and calculate results
            Crane_StopVertical();
            TickType_t elapsed_ms = xTaskGetTickCount() - auto_step_start;
            float elapsed_sec = elapsed_ms / 1000.0f;
            float speed = 4.0f / elapsed_sec;
            char buf[100];
            sprintf(buf, "CAL: PWM %d -> Speed: %.2f cm/sec\r\n", servo_pwm_backward, speed);
            print_str(buf);

            // this is where we would now take the new results and find something either in between or further away from the firts option
            // it would repeat this loop until we end up close to 2cm/s
            // however, due to timeconstraints, a hardcoded value was used again

            servo_pwm_backward = 1440; // hardcoded
            auto_step = 2;
            auto_step_start = xTaskGetTickCount();
            autoStateEntry = 1;
        }
        break;

    // step 2: next test for upwards speed
    case 2:
        if (h < 13.0f - AUTO_TOL_CM) {
            Crane_MoveVerticalDown();
        } else if (h > 13.0f + AUTO_TOL_CM) {
            Crane_MoveVerticalUp();
        } else {
            Crane_StopVertical();
            TickType_t elapsed_ms = xTaskGetTickCount() - auto_step_start;
            float elapsed_sec = elapsed_ms / 1000.0f;
            float speed = 5.0f / elapsed_sec;
            char buf[100];
            sprintf(buf, "CAL: PWM %d (80%%) -> Speed: %.2f cm/sec\r\n", servo_pwm_backward, speed);
            print_str(buf);

            // this is the comparison that would be made to see if we land within 80% of speed reqs
            if (speed >= 1.5f && speed <= 1.7f) {
                print_str("CAL: ✓ 80% Speed OK!\r\n");
            } else if (speed < 1.5f) {
                print_str("CAL: ✗ Too slow - increase PWM\r\n");
            } else {
                print_str("CAL: ✗ Too fast - decrease PWM\r\n");
            }

            // instead of logging, we would calculate the final speed

            auto_step = 3;
            auto_step_start = xTaskGetTickCount();
            autoStateEntry = 1;
        }
        break;

    // step 3: calculate downward speed
    case 3:

    	if (autoStateEntry) {
    	   servo_pwm_forward = 1550; // hardcoded starting point
    	   autoStateEntry = 0;
    	}
        if (h > 2.0f + AUTO_TOL_CM) {
            Crane_MoveVerticalUp();
        } else {
            Crane_StopVertical();
            TickType_t elapsed_ms = xTaskGetTickCount() - auto_step_start;
            float elapsed_sec = elapsed_ms / 1000.0f;
            float speed = 11.0f / elapsed_sec;
            char buf[100];
            sprintf(buf, "CAL: Down Speed: %.2f cm/sec\r\n", speed);
            print_str(buf);

            // we calculate speed here but currently do nothing with it
            // ideally, would use same procedure outlined in upwards handling to narrow in on the proper pwm value to
            // achieve the safe speed

            auto_step = 4;
            autoStateEntry = 1;
        }
        break;

    // complete calibrationand set back to manual
    case 4:
        Crane_StopVertical();
        ControlTask_SetMode(MODE_MANUAL);
        auto_step = 0;
        autoStateEntry = 1;
        break;

    default:
        auto_step = 0;
        autoStateEntry = 1;
        break;
    }
}
