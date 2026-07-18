#include "screen_protocol.h"
#include "usart.h"

/*===========================================================================
 * TX 中断队列
 * 使用 HAL_UART_Transmit_IT, TXCpltCallback 自动发下一条
 *===========================================================================*/
#define TX_BUF_MAX 128
#define TX_Q_SIZE   8

static struct { uint8_t buf[TX_BUF_MAX]; uint16_t len; } tx_q[TX_Q_SIZE];
static volatile uint8_t  tx_head, tx_tail;
static volatile uint8_t  tx_busy;

static void tx_send(void)
{
    if (tx_busy) return;
    if (tx_head == tx_tail) return;
    tx_busy = 1;
    HAL_UART_Transmit_IT(&huart4, tx_q[tx_tail].buf, tx_q[tx_tail].len);
}

static void tx_enqueue(const uint8_t *data, uint16_t len)
{
    uint8_t term[3] = {0xFF, 0xFF, 0xFF};
    uint8_t n = (tx_head + 1) % TX_Q_SIZE;
    if (n == tx_tail) return;
    uint16_t i = 0;
    while (i < len && i < TX_BUF_MAX - 4) { tx_q[tx_head].buf[i] = data[i]; i++; }
    tx_q[tx_head].buf[i++] = 0xFF;
    tx_q[tx_head].buf[i++] = 0xFF;
    tx_q[tx_head].buf[i++] = 0xFF;
    tx_q[tx_head].len = i;
    tx_head = n;
    tx_send();
}

/* HAL 弱函数重定义 – TX 完成回调 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != UART4) return;
    tx_busy = 0;
    tx_tail = (tx_tail + 1) % TX_Q_SIZE;
    tx_send();
}

/*==============================================================
 * RX 状态机
 *==============================================================*/
static uint8_t  rx_frame[RX_FRAME_MAX];
static uint16_t rx_frame_len;
static uint8_t  rx_ff_cnt;

volatile uint8_t  g_cmd_received = 0;
          char    g_cmd_buffer[RX_FRAME_MAX];

/*==============================================================
 * 底层发送 API
 *==============================================================*/
static void SendBytes(const uint8_t *data, uint16_t len)
{
    tx_enqueue(data, len);
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

/*==============================================================
 * 初始化
 * 注: MCU 单独复位不影响串口屏, 需冷启动或拉屏幕 RESET 脚
 *==============================================================*/
void Screen_Init(void)
{
    UART_StartRx();
    HAL_Delay(5000);

    for (int i = 0; i < 3; i++) {
        Screen_SendText("tsta", "READY");
        HAL_Delay(800);
    }
}

/*==============================================================
 * RX 回调
 *==============================================================*/
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

/*==============================================================
 * 发送一组结果 (先 page → ttitle → t0..tN, 每步间隔 200ms)
 * 模拟场景用阻塞延时; 真实场景可改为状态机, 每出一个结果调一次
 *==============================================================*/
void Screen_SendResult(const char *page, const char *title,
                       const char *items[], uint8_t count)
{
    Screen_SwitchPage(page);  HAL_Delay(600);
    Screen_SendText("tsta", title);   HAL_Delay(200);

    for (uint8_t i = 0; i < count && i < 6; i++) {
        char ctrl[4] = {'t', (char)('0' + i), '\0'};
        Screen_SendText(ctrl, items[i]);
        HAL_Delay(200);
    }
}

/*==============================================================
 * 令牌解析 & 事件派发
 *==============================================================*/
static void HandleToken(const char *buf)
{
    char p0[16], p1[16], p2[16], p3[16];
    uint8_t cnt = 0;
    const char *s = buf;

    p0[0] = p1[0] = p2[0] = p3[0] = '\0';

    while (*s && *s != ':' && cnt < 15) p0[cnt++] = *s++;
    p0[cnt] = '\0';
    if (*s != ':') goto done;
    s++;

    cnt = 0;
    while (*s && *s != ':' && cnt < 15) p1[cnt++] = *s++;
    p1[cnt] = '\0';
    if (*s != ':') goto done;
    s++;

    cnt = 0;
    while (*s && *s != ':' && cnt < 15) p2[cnt++] = *s++;
    p2[cnt] = '\0';
    if (*s != ':') goto done;
    s++;

    cnt = 0;
    while (*s && cnt < 15) p3[cnt++] = *s++;
    p3[cnt] = '\0';

done:
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
    if (strcmp(p0, "TEST") == 0 && strcmp(p1, "SINGLE")== 0) { Screen_SendText("tsta", "SINGLE_READY");return; }

    /* 校准 */
    if (strcmp(p0, "CAL") == 0 && strcmp(p1, "START") == 0) {
        const char *items[] = { p2, p3 };
        Screen_SendResult("result", "校准完成", items, 2);
        return;
    }

    /* 双端测试 */
    if (strcmp(p0, "DUAL") == 0) {
        Screen_SwitchPage("run"); HAL_Delay(500);
        Screen_SendText("tsta", "RUNNING");

        if (strcmp(p1, "FULL") == 0) {
            HAL_Delay(200);
            const char *items[] = { "线序:正常", "线缆:SFTP",
                                    "R1=12.3Ω R2=12.5Ω",
                                    "R3=12.4Ω R4=12.6Ω", "衰减=-3.2dB" };
            Screen_SendResult("result", "完整双端检测", items, 5);
        }
        if (strcmp(p1, "WIREMAP") == 0) { HAL_Delay(100); Screen_SwitchPage("result"); HAL_Delay(500); Screen_SendText("tsta", "线序检测"); }
        if (strcmp(p1, "SHIELD")  == 0) { HAL_Delay(100); Screen_SwitchPage("result"); HAL_Delay(500); Screen_SendText("tsta", "线缆类型:SFTP"); }
        if (strcmp(p1, "RES") == 0) {
            HAL_Delay(100);
            const char *items[] = { "1对=12.3Ω", "2对=12.5Ω",
                                    "3对=12.4Ω", "4对=12.6Ω" };
            Screen_SendResult("result", "直流电阻检测", items, 4);
        }
        if (strcmp(p1, "LOSS") == 0) { HAL_Delay(100); Screen_SwitchPage("result"); HAL_Delay(500); Screen_SendText("tsta", "30MHz衰减检测"); HAL_Delay(200); Screen_SendText("t0", "衰减=-3.2dB"); }
        return;
    }

    /* 单端测试 */
    if (strcmp(p0, "SINGLE") == 0) {
        Screen_SwitchPage("run"); HAL_Delay(500);
        Screen_SendText("tsta", "RUNNING");

        if (strcmp(p1, "FULL") == 0) {
            HAL_Delay(100);
            const char *items[] = { "长度=105m", "短路:无短路" };
            Screen_SendResult("result", "完整单端检测", items, 2);
        }
        if (strcmp(p1, "LEN")   == 0) { HAL_Delay(100); Screen_SwitchPage("result"); HAL_Delay(500); Screen_SendText("tsta", "长度检测"); HAL_Delay(200); Screen_SendText("t0", "长度=105m"); }
        if (strcmp(p1, "SHORT") == 0) { HAL_Delay(100); Screen_SwitchPage("result"); HAL_Delay(500); Screen_SendText("tsta", "无短路"); }
        if (strcmp(p1, "LOC") == 0) {
            HAL_Delay(100);
            const char *items[] = { "短路@2对", "距离=32.5m" };
            Screen_SendResult("result", "短路位置检测", items, 2);
        }
        return;
    }
}

/*==============================================================
 * 主循环处理
 *==============================================================*/
void Screen_Process(void)
{
    if (!g_cmd_received) return;
    g_cmd_received = 0;
    HandleToken(g_cmd_buffer);
}
