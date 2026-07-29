/**
 * @file ui_plot.h
 * @brief 串口屏 Waveform 控件的非阻塞绘图发送器。
 */
#ifndef UI_PLOT_H
#define UI_PLOT_H

#include <stdint.h>

/** Start drawing samples to a Waveform component. */
void UI_Plot_Start(uint8_t component_id, uint8_t channel,
                   const uint8_t *samples, uint16_t sample_count);

/** Stop the active drawing task. */
void UI_Plot_Stop(void);

/** Send one or more plotting commands when the UART queue has space. */
void UI_Plot_Process(void);

#endif
