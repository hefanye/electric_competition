/**
 * @file ad9226_debug_nogain.c
 * @brief AD9226 A 通道高速并行采样调试实现（去增益版本，DMA 方式）。
 *
 * TIM1_CH1 在 PA8 输出约 4.097 MHz 采样时钟（ACLK）。TIM1 更新事件触发
 * DMA2_Stream1（Channel6 = TIM1_UP），自动从 GPIOE->IDR 读取 12 位并行
 * 数据到 SRAM 缓冲区。DMA 传完 256 点后产生完成中断，主循环进行统计
 * 并通过 USART1 输出结果。
 *
 * 相比中断方式（每点进一次 ISR），DMA 方式由硬件自动搬运数据，CPU 无需
 * 介入采样过程，可支持 MHz 级采样率。
 *
 * 硬件映射：
 *   PA8 / TIM1_CH1 -> ACLK
 *   PE0~PE11       <- AD0~AD11（AD0 为最高位）
 */

#include "ad9226_debug_nogain.h"

#include <stdio.h>
#include <string.h>

#define AD9226_FRAME_SAMPLES      (256U)
#define AD9226_UART_TIMEOUT_MS    (100U)

static TIM_HandleTypeDef *s_tim;
static UART_HandleTypeDef *s_uart;
/* DMA 活动标志：仅当 AD9226 启动 DMA 后才处理中断，避免与 GSignal 冲突。 */
static volatile uint8_t s_dma_active;

/* DMA2_Stream1 Channel6 = TIM1_UP，用于自动搬运 GPIOE->IDR 到 SRAM。 */
static DMA_HandleTypeDef s_dma;

/* 采集缓冲区，DMA 直接写入。frame_ready=1 期间 DMA 已停止，可安全读取。 */
static volatile uint16_t s_samples[AD9226_FRAME_SAMPLES];
static volatile uint8_t  s_frame_ready;
static uint32_t s_frame_number;

/**
 * @brief 把 PE0~PE11 的物理位序转换为常用 12 位 ADC 码值。
 *
 * 模块定义 AD0 为最高位、AD11 为最低位；因此需要将端口的低 12 位反转。
 */
static uint16_t AD9226_PortToCode(uint16_t port_value)
{
    uint16_t code = 0U;
    uint8_t bit;

    for (bit = 0U; bit < 12U; ++bit)
    {
        if ((port_value & (uint16_t)(1U << bit)) != 0U)
        {
            code |= (uint16_t)(1U << (11U - bit));
        }
    }

    return code;
}

static void AD9226_Print(const char *text)
{
    if ((s_uart != NULL) && (text != NULL))
    {
        (void)HAL_UART_Transmit(s_uart, (uint8_t *)text,
                                (uint16_t)strlen(text), AD9226_UART_TIMEOUT_MS);
    }
}

/* TIM1 is on APB2.  If APB2 is prescaled, TIM1 receives PCLK2 x2. */
static uint32_t AD9226_GetTim1ClockHz(void)
{
    uint32_t timer_clock_hz = HAL_RCC_GetPCLK2Freq();

    if ((RCC->CFGR & RCC_CFGR_PPRE2) != 0U)
    {
        timer_clock_hz *= 2U;
    }

    return timer_clock_hz;
}

/*────────────────────────────────────────────────────────────────────
 * DMA 配置与启动（直接寄存器操作，绕过 HAL 状态机）
 *────────────────────────────────────────────────────────────────────*/

/**
 * @brief 配置 DMA2_Stream5（Channel6 = TIM1_UP），从 GPIOE->IDR 搬运到 s_samples。
 *        使用寄存器操作，避免 HAL 状态机在首次调用时返回错误。
 *
 * STM32F407 DMA 映射：TIM1_UP 在 DMA2_Stream5/Channel6（参考 g_signal_measurement.c）。
 */
static void AD9226_DMA_Config(void)
{
    DMA_Stream_TypeDef *stream = DMA2_Stream5;

    /* 确保 DMA2 时钟已使能 */
    __HAL_RCC_DMA2_CLK_ENABLE();

    /* 关闭 stream，CR.EN=0 */
    stream->CR &= ~DMA_SxCR_EN;
    while ((stream->CR & DMA_SxCR_EN) != 0U) { ; }

    /* 清除所有中断标志（Stream5 在 HIFCR 高位） */
    DMA2->HIFCR = (DMA_HIFCR_CTCIF5 | DMA_HIFCR_CHTIF5 |
                   DMA_HIFCR_CTEIF5 | DMA_HIFCR_CDMEIF5 |
                   DMA_HIFCR_CFEIF5);

    /* 配置 CR：Channel6、外设→内存、16位/16位、MINC、PINC不增、
     *          Normal 模式、TCIE 传输完成中断、高优先级 */
    stream->CR = (6U << DMA_SxCR_CHSEL_Pos)  |   /* Channel 6 = TIM1_UP */
                 DMA_SxCR_PL_1                |   /* 10 = High (PL_1=bit17) */
                 DMA_SxCR_MINC                |   /* 内存地址自增 */
                 DMA_SxCR_PSIZE_0             |   /* 01 = 16-bit */
                 DMA_SxCR_MSIZE_0             |   /* 01 = 16-bit */
                 DMA_SxCR_TCIE;                   /* 传输完成中断 */
    /* DIR=00 外设→内存（默认值，无需设置） */

    /* FCR：禁用 FIFO，直接模式 */
    stream->FCR = DMA_SxFCR_DMDIS;

    /* 外设地址：GPIOE->IDR */
    stream->PAR = (uint32_t)&GPIOE->IDR;
    /* 内存地址：s_samples */
    stream->M0AR = (uint32_t)s_samples;
    /* 传输数量：256 */
    stream->NDTR = AD9226_FRAME_SAMPLES;
}

/**
 * @brief 启动一次 DMA 传输（256 点）。
 *        DMA 完成后产生 TC 中断，DMA2_Stream5_IRQHandler 处理。
 */
static void AD9226_DMA_Start(void)
{
    DMA_Stream_TypeDef *stream = DMA2_Stream5;

    /* 关闭 stream */
    stream->CR &= ~DMA_SxCR_EN;
    while ((stream->CR & DMA_SxCR_EN) != 0U) { ; }

    /* 清除中断标志（Stream5 在 HIFCR 高位） */
    DMA2->HIFCR = (DMA_HIFCR_CTCIF5 | DMA_HIFCR_CHTIF5 |
                   DMA_HIFCR_CTEIF5 | DMA_HIFCR_CDMEIF5 |
                   DMA_HIFCR_CFEIF5);

    /* 重设内存地址和传输数量 */
    stream->M0AR = (uint32_t)s_samples;
    stream->NDTR = AD9226_FRAME_SAMPLES;

    /* 使能 stream */
    stream->CR |= DMA_SxCR_EN;
    s_dma_active = 1U;
}

/**
 * @brief AD9226 DMA 中断处理函数（由 stm32f4xx_it.c 的 DMA2_Stream5_IRQHandler 调用）。
 *        直接判断 TCIF5 标志（HISR 高位）。
 */
void AD9226_DMA_IRQHandler(void)
{
    /* 仅当 AD9226 的 DMA 处于活动状态时才处理中断。
     * 否则中断属于 GSignal，跳过避免清除其标志。 */
    if (s_dma_active == 0U)
    {
        return;
    }

    /* 检查 TCIF5（DMA2 HISR bit11） */
    if ((DMA2->HISR & DMA_HISR_TCIF5) != 0U)
    {
        /* 清除 TC 中断标志 */
        DMA2->HIFCR = DMA_HIFCR_CTCIF5;
        s_dma_active = 0U;
        s_frame_ready = 1U;
    }

    /* 清除其他可能的错误标志 */
    DMA2->HIFCR = (DMA_HIFCR_CHTIF5 | DMA_HIFCR_CTEIF5 |
                   DMA_HIFCR_CDMEIF5 | DMA_HIFCR_CFEIF5);
}

/*────────────────────────────────────────────────────────────────────
 * 对外接口
 *────────────────────────────────────────────────────────────────────*/

HAL_StatusTypeDef AD9226_Debug_Init(TIM_HandleTypeDef *htim,
                                    UART_HandleTypeDef *huart)
{
    uint32_t aclk_hz;
    char startup[256];

    if ((htim == NULL) || (huart == NULL))
    {
        return HAL_ERROR;
    }

    s_tim = htim;
    s_uart = huart;
    s_frame_ready = 0U;
    s_frame_number = 0U;

    /* 必须先打印横幅再启动定时器，否则高频中断/DMA 会干扰 UART 发送。 */
    aclk_hz = AD9226_GetTim1ClockHz() /
              ((s_tim->Init.Prescaler + 1U) * (s_tim->Init.Period + 1U));
    (void)snprintf(startup, sizeof(startup),
                   "\r\n=== AD9226 A-channel debug (DMA, no gain) ===\r\n"
                   "ACLK=%lu Hz (PSC=%lu ARR=%lu CCR1=%lu), data=PE0..PE11, AD0=MSB\r\n"
                   "DMA2_Stream1 Ch6 (TIM1_UP), 256 samples/frame\r\n"
                   "Tie A input to 0 V first: mean should be near 2048.\r\n",
                   (unsigned long)aclk_hz,
                   (unsigned long)s_tim->Init.Prescaler,
                   (unsigned long)s_tim->Init.Period,
                   (unsigned long)__HAL_TIM_GET_COMPARE(s_tim, TIM_CHANNEL_1));
    AD9226_Print(startup);

    /* 配置 DMA（此时还未启动传输） */
    AD9226_DMA_Config();
    AD9226_Print("DMA_CFG_OK\r\n");

    /* 启动首次 DMA 传输 */
    AD9226_DMA_Start();
    AD9226_Print("DMA_START_OK\r\n");

    /* 使能 TIM1 Update DMA 请求：每次 TIM1 溢出触发一次 DMA 传输 */
    __HAL_TIM_ENABLE_DMA(s_tim, TIM_DMA_UPDATE);

    /* 启动 PA8 上的 ACLK PWM 输出 */
    if (HAL_TIM_PWM_Start(s_tim, TIM_CHANNEL_1) != HAL_OK)
    {
        AD9226_Print("PWM_START_FAIL\r\n");
        return HAL_ERROR;
    }
    AD9226_Print("PWM_START_OK\r\n");

    /* 启动 TIM1 计数（产生 Update 事件 → 触发 DMA） */
    __HAL_TIM_ENABLE(s_tim);

    /* TIM1 是高级定时器，必须在计数启动后强制使能 BDTR.MOE（主输出使能）。
     * HAL_TIM_PWM_Start 和 __HAL_TIM_ENABLE 实测都不会保持 MOE=1（BDTR=0xA000, MOE=0）。
     * MOE=0 时 PWM 不会输出到 PA8，AD9226 收不到 ACLK，DMA 也就没有请求。
     * 这里直接写 BDTR 寄存器，bit15(MOE)=1。 */
    s_tim->Instance->BDTR = 0x8000U;  /* MOE=1, 其他位清零 */
    {
        char diag[128];
        (void)snprintf(diag, sizeof(diag),
                       "TIM_ENABLE_OK CR1=0x%lX DIER=0x%lX CCER=0x%lX BDTR=0x%lX\r\n",
                       (unsigned long)s_tim->Instance->CR1,
                       (unsigned long)s_tim->Instance->DIER,
                       (unsigned long)s_tim->Instance->CCER,
                       (unsigned long)s_tim->Instance->BDTR);
        AD9226_Print(diag);
    }

    /* 延时 10ms 后检查 DMA 是否在工作（NDTR 应该在减少） */
    HAL_Delay(10);
    {
        char diag[96];
        (void)snprintf(diag, sizeof(diag),
                       "DMA NDTR=%lu HISR=0x%lX\r\n",
                       (unsigned long)DMA2_Stream5->NDTR,
                       (unsigned long)DMA2->HISR);
        AD9226_Print(diag);
    }

    return HAL_OK;
}

/**
 * @brief DMA 模式下不再使用 TIM1 中断，此函数保留为空以兼容 main.c。
 */
void AD9226_Debug_TimPeriodElapsedCallback(void)
{
    /* DMA 模式：采样由 DMA 硬件搬运完成，无需 TIM1 中断回调。 */
}

void AD9226_Debug_Process(void)
{
    uint16_t index;
    uint16_t min_code = 0x0FFFU;
    uint16_t max_code = 0U;
    uint16_t code;
    uint32_t sum = 0U;
    uint32_t mean_code;
    uint16_t pp_code;
    int32_t dc_est_mv;
    uint32_t pp_adc_mv;
    char report[128];

    if (s_frame_ready == 0U)
    {
        return;
    }

    /* 停止 TIM1，防止处理期间产生新的 DMA 请求 */
    __HAL_TIM_DISABLE(s_tim);

    /* 统计整帧 */
    for (index = 0U; index < AD9226_FRAME_SAMPLES; ++index)
    {
        code = AD9226_PortToCode(s_samples[index]);
        sum += code;

        if (code < min_code)
        {
            min_code = code;
        }
        if (code > max_code)
        {
            max_code = code;
        }
    }

    mean_code = (sum + (AD9226_FRAME_SAMPLES / 2U)) / AD9226_FRAME_SAMPLES;
    pp_code = (uint16_t)(max_code - min_code);

    /* 依据说明书的 -5 V~+5 V 输入量程做近似换算，仅作调试参考。 */
    dc_est_mv = ((int32_t)2048 - (int32_t)mean_code) * 5000 / 2048;
    pp_adc_mv = ((uint32_t)pp_code * 5000U + 1024U) / 2048U;

    ++s_frame_number;
    (void)snprintf(report, sizeof(report),
                   "AD9226 A: frame=%lu mean=%lu min=%u max=%u pp=%u "
                   "dc_est=%ld mV pp_adc=%lu mV\r\n",
                   (unsigned long)s_frame_number,
                   (unsigned long)mean_code,
                   (unsigned int)min_code,
                   (unsigned int)max_code,
                   (unsigned int)pp_code,
                   (long)dc_est_mv,
                   (unsigned long)pp_adc_mv);
    AD9226_Print(report);

    /* 重启 DMA 和 TIM1，开始下一帧采集 */
    s_frame_ready = 0U;
    AD9226_DMA_Start();
    __HAL_TIM_ENABLE(s_tim);
}
