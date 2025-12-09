#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "string.h"
#include "User/util.h"
#include "User/uart.h"
#include "User/ControlTask.h"


// extern from STM32 HAL
extern UART_HandleTypeDef huart2;

char uartCommand[50];
static int cmdIndex = 0;

static TaskHandle_t uartTaskHandle = NULL;

//helper function for comparing strings for match
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
    print_str("UART: Type 'manual', 'auto', or 'cal'\r\n"); // print input options to user

    char ch;

    while (1)
    {
        // read 1 character received over uart
        if (HAL_UART_Receive(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY) == HAL_OK)
        {
            // echo what was input
            HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);

            // check if enter was pressed
            if (ch == '\r' || ch == '\n')
            {
                uartCommand[cmdIndex] = '\0'; // terminate string

                uartCommand[cmdIndex] = '\0'; // terminate string

                // reprint what user typed
                print_str("\r\nCommand received: ");
                print_str(uartCommand);
                print_str("\r\n");

                // check what was input to see if it aligns with our modes
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
                // if input not aligned with modes, print error msg
                else if (strlen(uartCommand) > 0)
                {
                    print_str("Unknown command\r\n");
                }

                // Reset input buffer
                memset(uartCommand, 0, sizeof(uartCommand));
                cmdIndex = 0;


                // reset the buffer
                memset(uartCommand, 0, sizeof(uartCommand));
                cmdIndex = 0;
            }
            else
            {
                // if enter was not pressed, push the char into the command buffer
                if (cmdIndex < sizeof(uartCommand) - 1)
                {
                    uartCommand[cmdIndex++] = ch;
                }
            }
        }
    }
}

void UART_StartCommandTask(void)
{
    xTaskCreate(UART_CommandTask, "UART_CommandTask", 512, NULL,
                tskIDLE_PRIORITY + 1, &uartTaskHandle);
}
