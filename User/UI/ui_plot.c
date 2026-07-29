/**
 * @file ui_plot.c
 * @brief 采用 TJC/Nextion 原生 cle/add 指令向 Waveform 控件写入曲线。
 */
#include "ui_plot.h"

#include "screen_protocol.h"

#include <stdio.h>

typedef struct
{
    const uint8_t *samples;
    uint16_t count;
    uint16_t index;
    uint8_t component_id;
    uint8_t channel;
    uint8_t clear_pending;
    uint8_t active;
} ui_plot_state_t;

static ui_plot_state_t s_plot;

void UI_Plot_Start(uint8_t component_id, uint8_t channel,
                   const uint8_t *samples, uint16_t sample_count)
{
    s_plot.samples = samples;
    s_plot.count = sample_count;
    s_plot.index = 0U;
    s_plot.component_id = component_id;
    s_plot.channel = channel;
    s_plot.clear_pending = 1U;
    s_plot.active = (samples != NULL && sample_count > 0U) ? 1U : 0U;
}

void UI_Plot_Stop(void)
{
    s_plot.active = 0U;
}

void UI_Plot_Process(void)
{
    char command[32];
    uint8_t sent = 0U;

    if (s_plot.active == 0U) {
        return;
    }

    if (s_plot.clear_pending != 0U) {
        (void)snprintf(command, sizeof(command), "cle %u,%u",
                       (unsigned int)s_plot.component_id,
                       (unsigned int)s_plot.channel);
        if (Screen_TrySendCommand(command) != 0U) {
            s_plot.clear_pending = 0U;
        }
        return;
    }

    /* 每轮最多投递4个点，避免占满UART4的发送队列。 */
    while (s_plot.index < s_plot.count && sent < 4U) {
        (void)snprintf(command, sizeof(command), "add %u,%u,%u",
                       (unsigned int)s_plot.component_id,
                       (unsigned int)s_plot.channel,
                       (unsigned int)s_plot.samples[s_plot.index]);
        if (Screen_TrySendCommand(command) == 0U) {
            break;
        }
        ++s_plot.index;
        ++sent;
    }

    if (s_plot.index >= s_plot.count) {
        s_plot.active = 0U;
    }
}
