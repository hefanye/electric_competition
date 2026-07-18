#include "screen_protocol.h"
#include "usart.h"

/*===========================================================================
 * RX 状态机
 *===========================================================================*/
static uint8_t  rx_frame[RX_FRAME_MAX];
static uint16_t rx_frame_len;
static uint8_t  rx_ff_cnt;

volatile uint8_t  g_cmd_received = 0;
          char    g_cmd_buffer[RX_FRAME_MAX];

/*===========================================================================
 * 底层发送
 *===========================================================================*/
static void SendBytes(const uint8_t *data, uint16_t len)
{
    uint8_t term[3] = {0xFF, 0xFF, 0xFF};
    HAL_UART_Transmit(&huart4, data, len, 200);
    HAL_UART_Transmit(&huart4, term, 3, 200);
}

void Screen_SendText(const char *ctrl_id, const char *text)
{
    uint8_t buf[96];
    uint16_t i = 0;
    while (i < sizeof(buf) - 20 && *ctrl_id) buf[i++] = *ctrl_id++;
    buf[i++] = '.'; buf[i++] = 't'; buf[i++] = 'x'; buf[i++] = 't'; buf[i++] = '=';
    buf[i++] = '"';
    while (i < sizeof(buf) - 5 && *text) buf[i++] = *text++;
    buf[i++] = '"';
    SendBytes(buf, i);
}

void Screen_SwitchPage(const char *page_name)
{
    uint8_t buf[24];
    uint16_t i = 0;
    buf[i++] = 'p'; buf[i++] = 'a'; buf[i++] = 'g'; buf[i++] = 'e'; buf[i++] = ' ';
    while (i < sizeof(buf) - 5 && *page_name) buf[i++] = *page_name++;
    SendBytes(buf, i);
}

/*===========================================================================
 * 初始化
 *===========================================================================*/
void Screen_Init(void)
{
    HAL_Delay(2000);
    Screen_SendText("tsta", "READY");
    UART_StartRx();
}

/*===========================================================================
 * RX 回调
 *===========================================================================*/
void Screen_RxByteCallback(uint8_t byte)
{
    if (byte == FRAME_TERM_BYTE) {
        rx_ff_cnt++;
        if (rx_ff_cnt >= FRAME_TERM_CNT) {
            if (rx_frame_len > 0) {
                rx_frame[rx_frame_len] = '\0';
                strcpy(g_cmd_buffer, (const char *)rx_frame);
                g_cmd_received = 1;
            }
            rx_frame_len = 0; rx_ff_cnt = 0;
        }
    } else {
        rx_ff_cnt = 0;
        if (rx_frame_len < sizeof(rx_frame) - 1)
            rx_frame[rx_frame_len++] = byte;
    }
}

/*===========================================================================
 * 令牌解析 & 事件派发
 *===========================================================================*/
static void HandleToken(const char *buf)
{
    /* 按 ":" 拆解 */
    char p0[16], p1[16], p2[16], p3[16];
    uint8_t cnt = 0;
    const char *s = buf;

    p0[0] = p1[0] = p2[0] = p3[0] = '\0';

    /* part0 */
    while (*s && *s != ':' && cnt < 15) p0[cnt++] = *s++;
    p0[cnt] = '\0';
    if (*s != ':') { cnt = 1; goto done; }
    s++; /* skip : */

    /* part1 */
    cnt = 0;
    while (*s && *s != ':' && cnt < 15) p1[cnt++] = *s++;
    p1[cnt] = '\0';
    if (*s != ':') { cnt = 2; goto done; }
    s++;

    /* part2 */
    cnt = 0;
    while (*s && *s != ':' && cnt < 15) p2[cnt++] = *s++;
    p2[cnt] = '\0';
    if (*s != ':') { cnt = 3; goto done; }
    s++;

    /* part3 */
    cnt = 0;
    while (*s && cnt < 15) p3[cnt++] = *s++;
    p3[cnt] = '\0';
    cnt = 4;

done:
    /* ========== 事件匹配 ========== */

    /* 返回主页 */
    if (strcmp(p0, "HOME") == 0 && strcmp(p1, "BACK") == 0) {
        HAL_Delay(500);
        Screen_SendText("tsta", "READY");
        return;
    }

    /* 导航到子页 */
    if (strcmp(p0, "CAL")  == 0 && strcmp(p1, "OPEN")  == 0) { Screen_SendText("tsta", "CAL_READY");   return; }
    if (strcmp(p0, "TEST") == 0 && strcmp(p1, "OPEN")  == 0) { Screen_SendText("tsta", "TEST_READY");  return; }
    if (strcmp(p0, "TEST") == 0 && strcmp(p1, "DUAL")  == 0) { Screen_SendText("tsta", "DUAL_READY");  return; }
    if (strcmp(p0, "TEST") == 0 && strcmp(p1, "SINGLE") == 0){ Screen_SendText("tsta", "SINGLE_READY");return; }

    /* ========== 测试模拟 ========== */

    if (strcmp(p0, "CAL") == 0 && strcmp(p1, "START") == 0) {
        /* 校准: CAL:START:<长度>:<类型> */
        Screen_SwitchPage("run"); HAL_Delay(500);
        Screen_SendText("tsta", "RUNNING");
        HAL_Delay(100);

        Screen_SwitchPage("result"); HAL_Delay(500);
        Screen_SendText("ttitle", "CAL_DONE");
        Screen_SendText("t0", p2);  /* 长度 */
        Screen_SendText("t1", p3);  /* 类型 */
        return;
    }

    if (strcmp(p0, "DUAL") == 0) {
        Screen_SwitchPage("run"); HAL_Delay(500);
        Screen_SendText("tsta", "RUNNING");
        if (strcmp(p1, "FULL") == 0) {
            HAL_Delay(200);
            Screen_SwitchPage("result"); HAL_Delay(500);
            Screen_SendText("ttitle", "DUAL_FULL");
            Screen_SendText("t0", "WIREMAP:OK");
            Screen_SendText("t1", "SHIELD:SFTP");
            Screen_SendText("t2", "R1=12.3R R2=12.5R");
            Screen_SendText("t3", "R3=12.4R R4=12.6R");
            Screen_SendText("t4", "LOSS=-3.2dB");
        }
        if (strcmp(p1, "WIREMAP") == 0) {
            HAL_Delay(100);
            Screen_SwitchPage("result"); HAL_Delay(500);
            Screen_SendText("ttitle", "WIREMAP");
        }
        if (strcmp(p1, "SHIELD") == 0) {
            HAL_Delay(100);
            Screen_SwitchPage("result"); HAL_Delay(500);
            Screen_SendText("ttitle", "SHIELD:SFTP");
        }
        if (strcmp(p1, "RES") == 0) {
            HAL_Delay(100);
            Screen_SwitchPage("result"); HAL_Delay(500);
            Screen_SendText("ttitle", "RESISTANCE");
            Screen_SendText("t0", "PAIR1=12.3R");
            Screen_SendText("t1", "PAIR2=12.5R");
            Screen_SendText("t2", "PAIR3=12.4R");
            Screen_SendText("t3", "PAIR4=12.6R");
        }
        if (strcmp(p1, "LOSS") == 0) {
            HAL_Delay(100);
            Screen_SwitchPage("result"); HAL_Delay(500);
            Screen_SendText("ttitle", "ATTENUATION");
            Screen_SendText("t0", "LOSS=-3.2dB");
        }
        return;
    }

    if (strcmp(p0, "SINGLE") == 0) {
        Screen_SwitchPage("run"); HAL_Delay(500);
        Screen_SendText("tsta", "RUNNING");

        if (strcmp(p1, "FULL") == 0) {
            HAL_Delay(100);
            Screen_SwitchPage("result"); HAL_Delay(500);
            Screen_SendText("ttitle", "SINGLE_FULL");
            Screen_SendText("t0", "LENGTH=105m");
            Screen_SendText("t1", "SHORT:NO_SHORT");
        }
        if (strcmp(p1, "LEN") == 0) {
            HAL_Delay(100);
            Screen_SwitchPage("result"); HAL_Delay(500);
            Screen_SendText("ttitle", "LENGTH");
            Screen_SendText("t0", "LENGTH=105m");
        }
        if (strcmp(p1, "SHORT") == 0) {
            HAL_Delay(100);
            Screen_SwitchPage("result"); HAL_Delay(500);
            Screen_SendText("ttitle", "NO_SHORT");
        }
        if (strcmp(p1, "LOC") == 0) {
            HAL_Delay(100);
            Screen_SwitchPage("result"); HAL_Delay(500);
            Screen_SendText("ttitle", "SHORT_LOC");
            Screen_SendText("t0", "SHORT@PAIR2");
            Screen_SendText("t1", "DIST=32.5m");
        }
        return;
    }
}

/*===========================================================================
 * 主循环处理
 *===========================================================================*/
void Screen_Process(void)
{
    if (!g_cmd_received) return;
    g_cmd_received = 0;
    HandleToken(g_cmd_buffer);
}
