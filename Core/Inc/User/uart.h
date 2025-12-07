#ifndef UART_H
#define UART_H

#include <stdint.h>

void UART_Init(void);
void UART_StartCommandTask(void);

// Optional: last received command (for ModeManager)
extern char uartCommand[50];

#endif
