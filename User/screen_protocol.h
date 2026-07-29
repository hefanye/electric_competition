#ifndef __SCREEN_PROTOCOL_H
#define __SCREEN_PROTOCOL_H

#include "main.h"
#include <string.h>

#define FRAME_TERM_CNT      3
#define FRAME_TERM_BYTE     0xFF
#define RX_FRAME_MAX        64

void Screen_Init(void);
void Screen_Process(void);
void Screen_SendText(const char *ctrl_id, const char *text);
void Screen_SwitchPage(const char *page_name);
/* Send one raw HMI command. Terminator is added internally.
 * Returns 1 only if UART4 accepted it into its TX queue. */
uint8_t Screen_TrySendCommand(const char *command);
void Screen_SendResult(const char *page, const char *title,
                       const char *items[], uint8_t count);
void Screen_RxByteCallback(uint8_t byte);
/* Called from the common HAL UART error callback.  It restores UART4 RX
 * after an overrun/noise error so a bad frame cannot permanently stop HMI
 * communication. */
void Screen_UartErrorCallback(void);

extern volatile uint8_t  g_cmd_received;
extern          char      g_cmd_buffer[RX_FRAME_MAX];

#endif
