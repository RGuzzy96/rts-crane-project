#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "string.h"
#include "User/util.h"
#include "User/uart.h"
#include "User/ControlTask.h"


// Extern from STM32 HAL
extern UART_HandleTypeDef huart2;

char uartCommand[50];
static int cmdIndex = 0;

static TaskHandle_t uartTaskHandle = NULL;

//helper function
int stricmp(const char *a, const char *b)
{
    while (*a && *b)
    {
        char ca = (*a >= 'A' && *a <= 'Z') ? *a + 32 : *a;
        char cb = (*b >= 'A' && *b <= 'Z') ? *b + 32 : *b;
        if (ca != cb) return ca - cb;
        a++; b++;
    }
    return *a - *b;
}


static void UART_CommandTask(void *param)
{
    print_str("UART: Type 'manual', 'auto', or 'cal'\r\n");

    char ch;

    while (1)
    {
        // Read 1 character from PuTTY
        if (HAL_UART_Receive(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY) == HAL_OK)
        {
            // Echo
            HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);

            // ENTER pressed?
            if (ch == '\r' || ch == '\n')
            {
                uartCommand[cmdIndex] = '\0'; // terminate string

                uartCommand[cmdIndex] = '\0'; // terminate string

                // Print what user typed
                print_str("\r\nCommand received: ");
                print_str(uartCommand);
                print_str("\r\n");

                if (stricmp(uartCommand, "manual") == 0)
                {
                    ControlTask_SetMode(MODE_MANUAL);
                    print_str("Manual mode selected\r\n");
                }
                else if (stricmp(uartCommand, "auto") == 0)
                {
                    ControlTask_SetMode(MODE_AUTO);
                    print_str("Auto mode selected\r\n");
                }
                else if (stricmp(uartCommand, "cal") == 0)
                {
                    ControlTask_SetMode(MODE_CAL);
                    print_str("Calibration mode selected\r\n");
                }

                else if (strlen(uartCommand) > 0)
                {
                    print_str("Unknown command\r\n");
                }

                // Reset input buffer
                memset(uartCommand, 0, sizeof(uartCommand));
                cmdIndex = 0;


                // Reset buffer
                memset(uartCommand, 0, sizeof(uartCommand));
                cmdIndex = 0;
            }
            else
            {
                // Push into command buffer
                if (cmdIndex < sizeof(uartCommand) - 1)
                {
                    uartCommand[cmdIndex++] = ch;
                }
            }
        }
    }
}

void UART_Init(void)
{
    // Nothing needed yet (HAL UART is initialized in main.c)
}

void UART_StartCommandTask(void)
{
    xTaskCreate(UART_CommandTask, "UART_CommandTask", 512, NULL,
                tskIDLE_PRIORITY + 1, &uartTaskHandle);
}
