/* uart_comm.h
 * UART command parsing and helpers
 */
#ifndef UART_COMM_H
#define UART_COMM_H

#include "main.h"
#include "globals.h"

#ifndef UART_RX_BUFFER_SIZE
#define UART_RX_BUFFER_SIZE 64
#endif

void Parse_UART_Command(void);
void Send_UART_Response(const char* msg);

#endif // UART_COMM_H
