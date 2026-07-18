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
void Screen_SendResult(const char *page, const char *title,
                       const char *items[], uint8_t count);
void Screen_RxByteCallback(uint8_t byte);

extern volatile uint8_t  g_cmd_received;
extern          char      g_cmd_buffer[RX_FRAME_MAX];

#endif
