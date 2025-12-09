#ifndef UART_H
#define UART_H

#include <stdint.h>

void UART_StartCommandTask(void);

extern char uartCommand[50];

#endif
