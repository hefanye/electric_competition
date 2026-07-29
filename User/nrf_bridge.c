#include "nrf_bridge.h"
#include "nrf24l01.h"
#include "usart.h"
#include <string.h>
#include <stdio.h>

#define BRIDGE_BUF_SIZE   32U

static uint8_t  s_tx_buf[BRIDGE_BUF_SIZE];
static uint8_t  s_tx_len;
static uint8_t  s_line_ready;

void NRF_Bridge_Init(void)
{
    NRF24L01_Init();

    {
        uint8_t cfg = NRF24L01_ReadReg(NRF_REG_CONFIG);
        uint8_t sta = NRF24L01_ReadReg(NRF_REG_STATUS);
        char buf[48];
        (void)snprintf(buf, sizeof(buf), "\r\n[DIAG] CFG=0x%02X STA=0x%02X\r\n", cfg, sta);
        (void)HAL_UART_Transmit(&huart1, (uint8_t *)buf, (uint16_t)strlen(buf), 200U);
    }

    {
        const char *tag;
        if (NRF24L01_Check()) {
            tag = "\r\n[NRF DETECTED]\r\n";
        } else {
            tag = "\r\n[NRF NOT DETECTED]\r\n";
        }
        (void)HAL_UART_Transmit(&huart1, (uint8_t *)tag, (uint16_t)strlen(tag), 200U);
    }

    NRF24L01_Configure();
    {
        uint8_t cfg = NRF24L01_ReadReg(NRF_REG_CONFIG);
        const char *p;
        char buf[64];
        (void)snprintf(buf, sizeof(buf), "\r\n[CONFIGURE] CFG=0x%02X\r\n", cfg);
        (void)HAL_UART_Transmit(&huart1, (uint8_t *)buf, (uint16_t)strlen(buf), 200U);
        if (cfg & NRF_CFG_PWR_UP) {
            p = "[PWR_UP=1]\r\n";
        } else {
            p = "[PWR_UP=0] SPI WRITE FAIL?\r\n";
        }
        (void)HAL_UART_Transmit(&huart1, (uint8_t *)p, (uint16_t)strlen(p), 200U);
    }
    NRF24L01_TX_Mode();

    s_tx_len     = 0U;
    s_line_ready = 0U;

    (void)HAL_UART_AbortReceive_IT(&huart1);
    __HAL_UART_DISABLE_IT(&huart1, UART_IT_RXNE);

    {
        const char *msg = "\r\n=== NRF TX Bridge Ready (USART1 115200) ===\r\n"
                          "Type text + Enter to send via NRF24L01\r\n";
        (void)HAL_UART_Transmit(&huart1, (uint8_t *)msg, (uint16_t)strlen(msg), 200U);
    }
}

void NRF_Bridge_Process(void)
{
    if ((USART1->SR & USART_SR_RXNE) != 0U)
    {
        uint8_t byte = (uint8_t)(USART1->DR & 0xFFU);
        while ((USART1->SR & USART_SR_TXE) == 0U) { }
        USART1->DR = byte;

        if ((byte == '\r') || (byte == '\n'))
        {
            if (s_tx_len > 0U)
                s_line_ready = 1U;
        }
        else
        {
            if (s_tx_len < BRIDGE_BUF_SIZE)
                s_tx_buf[s_tx_len++] = byte;
            else
                s_line_ready = 1U;
        }
    }

    if (s_line_ready != 0U)
    {
        while (s_tx_len < BRIDGE_BUF_SIZE)
            s_tx_buf[s_tx_len++] = 0U;

        /* 盲发 5 次，不等 ACK（MISO 断开时 STATUS 读不回） */
        for (uint8_t i = 0; i < 5; i++)
        {
            NRF24L01_TxPacketNoWait(s_tx_buf, BRIDGE_BUF_SIZE);
            HAL_Delay(5);
        }

        const char *p = "\r\n[TX SENT]\r\n";
        (void)HAL_UART_Transmit(&huart1, (uint8_t *)p, (uint16_t)strlen(p), 100U);

        s_tx_len     = 0U;
        s_line_ready = 0U;
    }
}
