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

// Auto mode constants
#define AUTO_BASE_CM         6.0f   // first target height
#define AUTO_TOL_CM           0.5f   // tolerance around target

// Auto mode state machine
static uint8_t auto_step = 0;
static TickType_t auto_step_start = 0;

// Manual mode states
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

// Vertical axis state
static Direction vertSwitchDir = DIR_NONE;
static uint8_t vertButtonHeld = 0;
static Direction vertCurrentMotion = DIR_NONE;

// Platform axis state
static Direction platSwitchDir = DIR_NONE;
static uint8_t platButtonHeld = 0;
static Direction platCurrentMotion = DIR_NONE;

// Auto mode state tracking
static uint8_t autoStateEntry = 1;

// External sensor queue
extern QueueHandle_t sensorQueue;

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

static void ControlTask(void *arg);
static void updateVerticalMotion(void);
static void updatePlatformMotion(void);
static void updateAutoMode(void);

// ============================================================================
// PUBLIC API
// ============================================================================

void ControlTask_SendEvent(InputEvent evt)
{
    if (controlQueue) {
        xQueueSend(controlQueue, &evt, 0);
    }
}

void ControlTask_SetMode(CraneMode mode)
{
    currentMode = mode;

    // Full reset of motion state when mode changes
    vertButtonHeld = 0;
    platButtonHeld = 0;
    vertSwitchDir = DIR_NONE;
    platSwitchDir = DIR_NONE;
    vertCurrentMotion = DIR_NONE;
    platCurrentMotion = DIR_NONE;

    Crane_StopVertical();
    Crane_StopPlatform();

    if (mode == MODE_AUTO) {
        auto_step = 0;
        autoStateEntry = 1;
    }

    // Status message
    switch (mode) {
        case MODE_MANUAL:
            print_str("Mode: MANUAL\r\n");
            break;
        case MODE_AUTO:
            print_str("Mode: AUTO\r\n");
            break;
        case MODE_CAL:
            print_str("Mode: CAL\r\n");
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

// ============================================================================
// MANUAL MODE HELPERS
// ============================================================================

static void updateVerticalMotion(void)
{
    // Stop if button not held or no switch direction
    if (!vertButtonHeld || vertSwitchDir == DIR_NONE) {
        if (vertCurrentMotion != DIR_NONE) {
            Crane_StopVertical();
            vertCurrentMotion = DIR_NONE;
        }
        return;
    }

    // Move based on switch direction
    if (vertSwitchDir == DIR_UP && vertCurrentMotion != DIR_UP) {
        Crane_MoveVerticalUp();
        vertCurrentMotion = DIR_UP;
    } else if (vertSwitchDir == DIR_DOWN && vertCurrentMotion != DIR_DOWN) {
        Crane_MoveVerticalDown();
        vertCurrentMotion = DIR_DOWN;
    }
}

static void updatePlatformMotion(void)
{
    // Stop if button not held or no switch direction
    if (!platButtonHeld || platSwitchDir == DIR_NONE) {
        if (platCurrentMotion != DIR_NONE) {
            Crane_StopPlatform();
            platCurrentMotion = DIR_NONE;
        }
        return;
    }

    // Move based on switch direction
    if (platSwitchDir == DIR_LEFT && platCurrentMotion != DIR_LEFT) {
        Crane_MovePlatformLeft();
        platCurrentMotion = DIR_LEFT;
    } else if (platSwitchDir == DIR_RIGHT && platCurrentMotion != DIR_RIGHT) {
        Crane_MovePlatformRight();
        platCurrentMotion = DIR_RIGHT;
    }
}

// ============================================================================
// AUTO MODE STATE MACHINE (9 STEPS)
// ============================================================================

static void updateAutoMode(void)
{
    CraneSensorData s;
    if (xQueueReceive(sensorQueue, &s, 0) != pdPASS) {
        return;  // No fresh sensor reading
    }

    float h = s.heightCm;

    // CHECK IF MANUAL INPUT RECEIVED - RESET AUTO MODE
    InputEvent evt;
    if (xQueueReceive(controlQueue, &evt, 0) == pdPASS) {
        // Manual input detected - reset to MANUAL mode
        print_str("AUTO: Manual input detected! Resetting to MANUAL mode\r\n");
        Crane_StopVertical();
        Crane_StopPlatform();
        auto_step = 0;
        autoStateEntry = 1;
        ControlTask_SetMode(MODE_MANUAL);
        return;  // Exit AUTO mode immediately
    }


    switch (auto_step) {

    // Step 0: Raise to 10 cm (baseline)
    case 0:
    {
        if (autoStateEntry) {
            print_str("AUTO: Step0 -> 10 cm baseline\r\n");
            autoStateEntry = 0;
        }
        if (h < AUTO_BASE_CM - AUTO_TOL_CM) {
            Crane_MoveVerticalDown();  // FIXED: Was MoveVerticalDown
        } else if (h > AUTO_BASE_CM + AUTO_TOL_CM) {
            Crane_MoveVerticalUp();
        } else {
            Crane_StopVertical();
            print_str("AUTO: 10 cm reached, swing RIGHT 600ms\r\n");
            auto_step = 1;
            auto_step_start = xTaskGetTickCount();
            autoStateEntry = 1;
        }
        break;
    }

    // Step 1: Swing RIGHT 600 ms (pick up from right position)
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

    // Step 2: Go UP 2 cm (from 10 cm to ~12 cm)
    case 2:
    {
        float target = AUTO_BASE_CM + 2.0f;  // FIXED: Was 5.0f, should be 2.0f for 12cm
        if (autoStateEntry) {
            print_str("AUTO: Step2 -> UP 2cm (to 12cm)\r\n");
            autoStateEntry = 0;
        }
        if (h < target - AUTO_TOL_CM) {
            Crane_MoveVerticalDown();  // FIXED: Was MoveVerticalDown
        } else {
            Crane_StopVertical();
            print_str("AUTO: 12 cm reached, return to CENTER (LEFT 600ms)\r\n");
            auto_step = 3;
            auto_step_start = xTaskGetTickCount();
            autoStateEntry = 1;
        }
        break;
    }

    // Step 3: Back to CENTER - LEFT 600 ms (return to center)
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

    // Step 4: Go UP 5 cm (from 10 cm to ~15 cm)
    case 4:
    {
        float target = AUTO_BASE_CM + 10.0f;  // 15 cm
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

    // Step 5: Go LEFT 600 ms (move left at high position)
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

        // Step 7: Back to CENTER - RIGHT 600 ms
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

        // Step 8: Go DOWN fully to 3 cm (pickup position)
        case 8:
        {
            float target = 1.0f;
            if (autoStateEntry) {
                print_str("AUTO: Step8 -> DOWN to 3cm (PICKUP)\r\n");
                autoStateEntry = 0;
            }
            if (h > target + AUTO_TOL_CM) {
                Crane_MoveVerticalUp();
            } else {
                Crane_StopVertical();
                print_str("AUTO: Reached 3cm, AUTO sequence COMPLETE!\r\n");
                auto_step = 9;
                autoStateEntry = 1;
            }
            break;
        }

        // Step 9: Done
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


// ============================================================================
// MAIN CONTROL TASK
// ============================================================================

static void ControlTask(void *arg)
{
    print_str("ControlTask started!\r\n");
    TickType_t lastWake = xTaskGetTickCount();
    InputEvent evt;

    for (;;) {
        // Process input events
        while (xQueueReceive(controlQueue, &evt, 0)) {
            // Ignore button/switch events unless in MANUAL mode
            if (currentMode != MODE_MANUAL) {
                // Except RESET button always stops motors
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
                continue;  // Skip other events
            }

            // Handle MANUAL mode events
            switch (evt) {
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

        // Apply motion based on mode
        if (currentMode == MODE_MANUAL) {
            updateVerticalMotion();
            updatePlatformMotion();
        } else if (currentMode == MODE_AUTO) {
            updateAutoMode();
        } else if (currentMode == MODE_CAL) {
            updateCalMode();  // ADD THIS LINE
        }

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(CONTROL_TASK_PERIOD_MS));
    }
}
/*


// Simple calibration - just set vertical speed to 80% of max
void updateCalMode(void)
{
    CraneSensorData s;
    if (xQueueReceive(sensorQueue, &s, 0) != pdPASS) {
        return;
    }

    float h = s.heightCm;

    switch (auto_step) {

    // Step 0: Start - Raise slowly to test speed
    case 0:
        if (autoStateEntry) {
            print_str("\r\n=== CALIBRATION MODE ===\r\n");
            print_str("CAL: Operating at 80% speed (1.6 cm/sec)\r\n");
            print_str("CAL: Raising to 20cm...\r\n");
            autoStateEntry = 0;
        }
        if (h < 14.0f - AUTO_TOL_CM) {
            Crane_MoveVerticalDown();  // At 80% speed
        } else if (h > 14.0f + AUTO_TOL_CM) {
            Crane_MoveVerticalUp();
        } else {
            Crane_StopVertical();
            print_str("CAL: Reached 20cm. Test servo values.\r\n");
            print_str("CAL: Speed: 1.6 cm/sec (80% of max 2 cm/sec)\r\n");
            auto_step = 1;
            auto_step_start = xTaskGetTickCount();
            autoStateEntry = 1;
        }
        break;

    // Step 1: Lower back to 1cm
    case 1:
        if (autoStateEntry) {
            print_str("CAL: Lowering to 1cm...\r\n");
            autoStateEntry = 0;
        }
        if (h > 1.0f + AUTO_TOL_CM) {
            Crane_MoveVerticalUp();
        } else {
            Crane_StopVertical();
            print_str("CAL: Reached 1cm. Calibration COMPLETE!\r\n");
            print_str("CAL: Returning to MANUAL mode\r\n");
            auto_step = 2;
            autoStateEntry = 1;
        }
        break;

    // Step 2: Done
    case 2:
        Crane_StopVertical();
        Crane_StopPlatform();
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


void updateCalMode(void)
{
    CraneSensorData s;
    if (xQueueReceive(sensorQueue, &s, 0) != pdPASS) {
        return;
    }

    float h = s.heightCm;

    switch (auto_step) {

    // Step 0: Start - Raise slowly to test speed
    case 0:
        if (autoStateEntry) {
            print_str("\r\n=== CALIBRATION MODE ===\r\n");
            print_str("CAL: Operating at 80% speed (1.6 cm/sec)\r\n");
            print_str("CAL: Raising from 1cm to 20cm...\r\n");
            auto_step_start = xTaskGetTickCount();  // START TIMER
            autoStateEntry = 0;
        }
        if (h < 13.0f - AUTO_TOL_CM) {
            Crane_MoveVerticalDown();
        } else if (h > 13.0f + AUTO_TOL_CM) {
            Crane_MoveVerticalUp();
        } else {
            Crane_StopVertical();

            // CALCULATE SPEED
            TickType_t elapsed_ms = xTaskGetTickCount() - auto_step_start;
            float elapsed_sec = elapsed_ms / 1000.0f;
            float distance_cm = 19.0f;  // 20cm - 1cm
            float speed = distance_cm / elapsed_sec;

            char buf[100];
            sprintf(buf, "CAL: Reached 20cm\r\n");
            print_str(buf);
            sprintf(buf, "CAL: Time: %.2f sec, Distance: %.1f cm\r\n", elapsed_sec, distance_cm);
            print_str(buf);
            sprintf(buf, "CAL: Actual Speed: %.2f cm/sec (Target: 1.6 cm/sec)\r\n", speed);
            print_str(buf);

            if (speed > 2.0f) {
                print_str("CAL: WARNING - Speed exceeds max 2 cm/sec!\r\n");
            } else if (speed < 1.5f) {
                print_str("CAL: WARNING - Speed too slow (target 1.6 cm/sec)\r\n");
            } else {
                print_str("CAL: Speed OK!\r\n");
            }

            print_str("CAL: Lowering back to 1cm...\r\n");
            auto_step = 1;
            auto_step_start = xTaskGetTickCount();  // RESTART TIMER
            autoStateEntry = 1;
        }
        break;

    // Step 1: Lower back to 1cm
    case 1:
        if (autoStateEntry) {
            autoStateEntry = 0;
        }
        if (h > 1.0f + AUTO_TOL_CM) {
            Crane_MoveVerticalUp();
        } else {
            Crane_StopVertical();

            // CALCULATE SPEED FOR DOWNWARD MOVEMENT
            TickType_t elapsed_ms = xTaskGetTickCount() - auto_step_start;
            float elapsed_sec = elapsed_ms / 1000.0f;
            float distance_cm = 19.0f;
            float speed = distance_cm / elapsed_sec;

            char buf[100];
            sprintf(buf, "CAL: Reached 1cm (down)\r\n");
            print_str(buf);
            sprintf(buf, "CAL: Down Speed: %.2f cm/sec\r\n", speed);
            print_str(buf);

            print_str("CAL: Calibration COMPLETE!\r\n");
            print_str("CAL: Returning to MANUAL mode\r\n");
            auto_step = 2;
            autoStateEntry = 1;
        }
        break;

    // Step 2: Done
    case 2:
        Crane_StopVertical();
        Crane_StopPlatform();
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
*/
void updateCalMode(void)
{
    CraneSensorData s;
    if (xQueueReceive(sensorQueue, &s, 0) != pdPASS) {
        return;
    }

    float h = s.heightCm;

    switch (auto_step) {

    // Step 0: Test 1700 PWM (2cm -> 4cm)
    case 0:
        if (autoStateEntry) {
            print_str("\r\n=== CALIBRATION MODE ===\r\n");
            print_str("CAL: Stage 1 - PWM 1700 (2cm -> 4cm)\r\n");
            servo_pwm_forward = 1800;
            auto_step_start = xTaskGetTickCount();
            autoStateEntry = 0;
        }
        if (h < 4.0f - AUTO_TOL_CM) {
            Crane_MoveVerticalDown();
        } else if (h > 4.0f + AUTO_TOL_CM) {
            Crane_MoveVerticalUp();
        } else {
            Crane_StopVertical();
            TickType_t elapsed_ms = xTaskGetTickCount() - auto_step_start;
            float elapsed_sec = elapsed_ms / 1000.0f;
            float speed = 2.0f / elapsed_sec;
            char buf[100];
            sprintf(buf, "CAL: PWM 1700 -> Speed: %.2f cm/sec\r\n", speed);
            print_str(buf);

            print_str("CAL: Stage 2 - PWM 1600 (4cm -> 8cm)\r\n");
            servo_pwm_forward = 1700;
            auto_step = 1;
            auto_step_start = xTaskGetTickCount();
            autoStateEntry = 1;
        }
        break;

    // Step 1: Test 1600 PWM (4cm -> 8cm)
    case 1:
        if (h < 8.0f - AUTO_TOL_CM) {
            Crane_MoveVerticalDown();
        } else if (h > 8.0f + AUTO_TOL_CM) {
            Crane_MoveVerticalUp();
        } else {
            Crane_StopVertical();
            TickType_t elapsed_ms = xTaskGetTickCount() - auto_step_start;
            float elapsed_sec = elapsed_ms / 1000.0f;
            float speed = 4.0f / elapsed_sec;
            char buf[100];
            sprintf(buf, "CAL: PWM 1600 -> Speed: %.2f cm/sec\r\n", speed);
            print_str(buf);

            print_str("CAL: Stage 3 - PWM 1570 (8cm -> 13cm)\r\n");
            servo_pwm_forward = 1570;
            auto_step = 2;
            auto_step_start = xTaskGetTickCount();
            autoStateEntry = 1;
        }
        break;

    // Step 2: Test 1570 PWM - 80% (8cm -> 13cm)
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
            sprintf(buf, "CAL: PWM 1570 (80%%) -> Speed: %.2f cm/sec (Target: 1.6)\r\n", speed);
            print_str(buf);

            if (speed >= 1.5f && speed <= 1.7f) {
                print_str("CAL: ✓ 80% Speed OK!\r\n");
            } else if (speed < 1.5f) {
                print_str("CAL: ✗ Too slow - increase PWM\r\n");
            } else {
                print_str("CAL: ✗ Too fast - decrease PWM\r\n");
            }

            print_str("CAL: Stage 4 - Lowering back at 80% (13cm -> 2cm)\r\n");
            auto_step = 3;
            auto_step_start = xTaskGetTickCount();
            autoStateEntry = 1;
        }
        break;



    // Step 3: Lower back at 80% speed (13cm -> 2cm)
    case 3:

    	if (autoStateEntry) {
    	   servo_pwm_backward = 1430;  // Set DOWN speed (different from UP)
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

            print_str("CAL: Calibration COMPLETE!\r\n");
            auto_step = 4;
            autoStateEntry = 1;
        }
        break;

    // Step 4: Done
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
