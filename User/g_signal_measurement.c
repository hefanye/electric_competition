#include "g_signal_measurement.h"

#include "ui_controller.h"
#include "ui_display.h"
#include "ui_hmi_map.h"
#include "spectrum_analysis.h"
#include "g_signal_pc_debug.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define G_SIGNAL_DISCARD_SAMPLES  8U
#define G_SIGNAL_DMA_SAMPLES      (G_SIGNAL_FRAME_SAMPLES + G_SIGNAL_DISCARD_SAMPLES)
#define G_SIGNAL_BIN_COUNT        (G_SIGNAL_FRAME_SAMPLES / 2U + 1U)

static TIM_HandleTypeDef *s_htim;
static volatile uint8_t s_busy;
static volatile uint8_t s_frame_ready;
static volatile uint8_t s_dma_fault;
static ui_requirement_t s_requirement;
static ui_view_t s_view;

static uint16_t s_raw[G_SIGNAL_DMA_SAMPLES];
static sp_complex_f32_t s_fft[G_SIGNAL_FRAME_SAMPLES];
static float s_magnitude[G_SIGNAL_BIN_COUNT];
static uint8_t s_wave_one[UI_HMI_WAVE_PLOT_POINTS];
static uint8_t s_wave_three[UI_HMI_WAVE_PLOT_POINTS];
static uint8_t s_spectrum[UI_HMI_SPECTRUM_PLOT_POINTS];

static uint16_t reverse12(uint16_t value)
{
    uint16_t result = 0U;
    uint8_t bit;
    for (bit = 0U; bit < 12U; ++bit) {
        result = (uint16_t)((result << 1U) | (value & 1U));
        value >>= 1U;
    }
    return result;
}

static float code_to_input_voltage(uint16_t code)
{
    /* Module manual: D = 2048 - Vin * 2048 / 5.  Divide by front-end gain. */
    return ((2048.0f - (float)code) * 5.0f / 2048.0f) / G_SIGNAL_INPUT_GAIN;
}

static uint8_t scale_to_u8(float value, float minimum, float maximum)
{
    float scaled;
    if (maximum <= minimum + 1.0e-12f) {
        return 128U;
    }
    scaled = (value - minimum) * 255.0f / (maximum - minimum);
    if (scaled < 0.0f) scaled = 0.0f;
    if (scaled > 255.0f) scaled = 255.0f;
    return (uint8_t)(scaled + 0.5f);
}

static float sample_linear(float index)
{
    uint32_t i0 = (uint32_t)index % G_SIGNAL_FRAME_SAMPLES;
    uint32_t i1 = (i0 + 1U) % G_SIGNAL_FRAME_SAMPLES;
    float fraction = index - floorf(index);
    float v0 = code_to_input_voltage(reverse12(s_raw[G_SIGNAL_DISCARD_SAMPLES + i0] & 0x0FFFU));
    float v1 = code_to_input_voltage(reverse12(s_raw[G_SIGNAL_DISCARD_SAMPLES + i1] & 0x0FFFU));

    return v0 + (v1 - v0) * fraction;
}

static void build_wave(uint8_t *destination, uint8_t periods, float frequency_hz)
{
    uint32_t i;
    uint32_t crossing = 0U;
    float minimum = 1.0e30f;
    float maximum = -1.0e30f;
    float step;

    if (frequency_hz < 1.0f) frequency_hz = 1000.0f;
    for (i = 1U; i < G_SIGNAL_FRAME_SAMPLES; ++i) {
        if (sample_linear((float)(i - 1U)) <= 0.0f && sample_linear((float)i) > 0.0f) {
            crossing = i;
            break;
        }
    }
    step = ((float)periods * (float)G_SIGNAL_SAMPLE_RATE_HZ / frequency_hz) /
           (float)(UI_HMI_WAVE_PLOT_POINTS - 1U);
    for (i = 0U; i < UI_HMI_WAVE_PLOT_POINTS; ++i) {
        float value = sample_linear((float)crossing + step * (float)i);
        if (value < minimum) minimum = value;
        if (value > maximum) maximum = value;
    }
    for (i = 0U; i < UI_HMI_WAVE_PLOT_POINTS; ++i) {
        float value = sample_linear((float)crossing + step * (float)i);
        destination[i] = scale_to_u8(value, minimum, maximum);
    }
}

static void build_spectrum(float *maximum_amplitude)
{
    uint32_t point;
    float peak = 1.0e-12f;
    for (point = 1U; point < G_SIGNAL_BIN_COUNT; ++point) {
        if (s_magnitude[point] > peak) peak = s_magnitude[point];
    }
    for (point = 0U; point < UI_HMI_SPECTRUM_PLOT_POINTS; ++point) {
        uint32_t first = 1U + point * (G_SIGNAL_BIN_COUNT - 1U) /
                         UI_HMI_SPECTRUM_PLOT_POINTS;
        uint32_t last = 1U + (point + 1U) * (G_SIGNAL_BIN_COUNT - 1U) /
                        UI_HMI_SPECTRUM_PLOT_POINTS;
        float local = 0.0f;
        uint32_t bin;
        for (bin = first; bin <= last && bin < G_SIGNAL_BIN_COUNT; ++bin) {
            if (s_magnitude[bin] > local) local = s_magnitude[bin];
        }
        s_spectrum[point] = scale_to_u8(local, 0.0f, peak);
    }
    *maximum_amplitude = peak;
}

static void make_top3(ui_spectrum_measurement_t *result)
{
    uint32_t rank;
    uint32_t selected[3] = { 0U, 0U, 0U };
    for (rank = 0U; rank < 3U; ++rank) {
        uint32_t bin;
        uint32_t best = 1U;
        for (bin = 2U; bin + 1U < G_SIGNAL_BIN_COUNT; ++bin) {
            uint32_t previous;
            uint8_t rejected = 0U;
            for (previous = 0U; previous < rank; ++previous) {
                if (bin + 2U >= selected[previous] && bin <= selected[previous] + 2U) {
                    rejected = 1U;
                }
            }
            if (rejected == 0U && s_magnitude[bin] > s_magnitude[best]) best = bin;
        }
        selected[rank] = best;
        result->frequency_mHz[rank] = (uint32_t)((float)best *
            (float)G_SIGNAL_SAMPLE_RATE_HZ * 1000.0f / (float)G_SIGNAL_FRAME_SAMPLES + 0.5f);
        result->amplitude_mV[rank] = (uint32_t)(s_magnitude[best] * 1000.0f + 0.5f);
    }
}

static void analyse_frame(void)
{
    uint32_t i;
    float minimum = 1.0e30f, maximum = -1.0e30f, sum2 = 0.0f;
    float coherent_gain;
    spectrum_peak_t fundamental;
    ui_wave_measurement_t wave;
    ui_spectrum_measurement_t spectrum;
    float ignored_peak;

    for (i = 0U; i < G_SIGNAL_FRAME_SAMPLES; ++i) {
        float value = code_to_input_voltage(reverse12((uint16_t)(s_raw[i + G_SIGNAL_DISCARD_SAMPLES] & 0x0FFFU)));
        if (value < minimum) minimum = value;
        if (value > maximum) maximum = value;
        sum2 += value * value;
    }

    /* Reuse the complex FFT buffer as the windowed input buffer. This saves
     * 32 KiB SRAM compared with keeping a second 8192-point float array. */
    coherent_gain = 1.0f;
    (void)sp_window_coherent_gain_f32(SP_WINDOW_HANN,
                                      G_SIGNAL_FRAME_SAMPLES,
                                      &coherent_gain);
    if (coherent_gain <= 0.0f) coherent_gain = 1.0f;
    for (i = 0U; i < G_SIGNAL_FRAME_SAMPLES; ++i) {
        float value = code_to_input_voltage(reverse12((uint16_t)(s_raw[i + G_SIGNAL_DISCARD_SAMPLES] & 0x0FFFU)));
        float coefficient = 1.0f;
        (void)sp_window_coefficient_f32(SP_WINDOW_HANN,
                                        i,
                                        G_SIGNAL_FRAME_SAMPLES,
                                        &coefficient);
        s_fft[i].real = value * coefficient;
        s_fft[i].imag = 0.0f;
    }
    (void)fft_transform_inplace_f32(s_fft, G_SIGNAL_FRAME_SAMPLES, 0U);
    for (i = 0U; i < G_SIGNAL_BIN_COUNT; ++i) {
        float amplitude = sqrtf(s_fft[i].real * s_fft[i].real + s_fft[i].imag * s_fft[i].imag) /
                          ((float)G_SIGNAL_FRAME_SAMPLES * coherent_gain);
        if ((i != 0U) && (i != (G_SIGNAL_FRAME_SAMPLES / 2U))) amplitude *= 2.0f;
        s_magnitude[i] = amplitude;
    }
    (void)spectrum_find_peak_f32(s_magnitude, G_SIGNAL_BIN_COUNT,
                                 (float)G_SIGNAL_SAMPLE_RATE_HZ, G_SIGNAL_FRAME_SAMPLES,
                                 1U, G_SIGNAL_BIN_COUNT - 2U, &fundamental);
    wave.upp_mV = (uint32_t)((maximum - minimum) * 1000.0f + 0.5f);
    wave.urms_mV = (uint32_t)(sqrtf(sum2 / (float)G_SIGNAL_FRAME_SAMPLES) * 1000.0f + 0.5f);
    wave.fundamental_mHz = (uint32_t)(fundamental.frequency_hz * 1000.0f + 0.5f);
    build_wave(s_wave_one, 1U, fundamental.frequency_hz);
    build_wave(s_wave_three, 3U, fundamental.frequency_hz);
    build_spectrum(&ignored_peak);
    make_top3(&spectrum);

    UI_Controller_SetWaveMeasurement(&wave);
    UI_Controller_SetWaveSamplesForView(UI_VIEW_WAVE_1PERIOD, s_wave_one, UI_HMI_WAVE_PLOT_POINTS);
    UI_Controller_SetWaveSamplesForView(UI_VIEW_WAVE_3PERIOD, s_wave_three, UI_HMI_WAVE_PLOT_POINTS);
    UI_Controller_SetSpectrumMeasurement(&spectrum);
    UI_Controller_SetSpectrumSamples(s_spectrum, UI_HMI_SPECTRUM_PLOT_POINTS);
    /* PC 串口调试输出：将测量结果+波形+频谱通过 USART1 发到 PC（不依赖串口屏） */
    GSignal_PCDebug_Print(&wave, &spectrum, s_wave_one, s_spectrum);
    if (s_view == UI_VIEW_SPECTRUM) UI_Controller_RefreshSpectrum();
    else UI_Controller_RefreshWave(s_view);
    UI_Display_SetStatus("READY");
}

HAL_StatusTypeDef GSignal_Init(TIM_HandleTypeDef *htim)
{
    if (htim == NULL || htim->Instance != TIM1) return HAL_ERROR;
    s_htim = htim;
    s_busy = 0U; s_frame_ready = 0U; s_dma_fault = 0U;
    HAL_NVIC_SetPriority(DMA2_Stream5_IRQn, 1U, 0U);
    HAL_NVIC_EnableIRQ(DMA2_Stream5_IRQn);
    return HAL_OK;
}

void GSignal_Request(ui_requirement_t requirement, ui_view_t view)
{
    if (s_htim == NULL || s_busy != 0U) return;
    s_requirement = requirement;
    s_view = view;
    s_frame_ready = 0U; s_dma_fault = 0U; s_busy = 1U;
    /* 诊断：通知 PC 收到采集请求 */
    {
        extern UART_HandleTypeDef huart1;
        const char *view_name = "?";
        if (view == UI_VIEW_WAVE_1PERIOD) view_name = "WAVE1";
        else if (view == UI_VIEW_WAVE_3PERIOD) view_name = "WAVE3";
        else if (view == UI_VIEW_SPECTRUM) view_name = "SPECTRUM";
        char msg[64];
        (void)snprintf(msg, sizeof(msg), "REQ: %s\r\n", view_name);
        (void)HAL_UART_Transmit(&huart1, (uint8_t *)msg, (uint16_t)strlen(msg), 100);
    }
    UI_Display_SetStatus("CAPTURE");
    DMA2_Stream5->CR &= ~DMA_SxCR_EN;
    while ((DMA2_Stream5->CR & DMA_SxCR_EN) != 0U) { }
    DMA2->HIFCR = DMA_HIFCR_CFEIF5 | DMA_HIFCR_CDMEIF5 | DMA_HIFCR_CTEIF5 |
                  DMA_HIFCR_CHTIF5 | DMA_HIFCR_CTCIF5;
    DMA2_Stream5->PAR = (uint32_t)&GPIOE->IDR;
    DMA2_Stream5->M0AR = (uint32_t)s_raw;
    DMA2_Stream5->NDTR = G_SIGNAL_DMA_SAMPLES;
    DMA2_Stream5->CR = (6U << DMA_SxCR_CHSEL_Pos) | DMA_SxCR_PL_1 |
                       DMA_SxCR_MINC | DMA_SxCR_PSIZE_0 | DMA_SxCR_MSIZE_0 |
                       DMA_SxCR_TCIE | DMA_SxCR_TEIE;
    DMA2_Stream5->CR |= DMA_SxCR_EN;
    __HAL_TIM_SET_COUNTER(s_htim, 0U);
    (void)HAL_TIM_PWM_Start(s_htim, TIM_CHANNEL_1);
    /* TIM1 高级定时器必须使能 BDTR.MOE 才能输出 PWM 到 PA8（ACLK） */
    TIM1->BDTR |= TIM_BDTR_MOE;
    /* 每次 Request 都要重新使能 TIM1 计数（DMA 完成中断里会清 CEN）。
     * HAL_TIM_PWM_Start 不会设置 CR1.CEN，必须手动 ENABLE。 */
    __HAL_TIM_ENABLE(s_htim);
    TIM1->DIER |= TIM_DIER_UDE;
}

void GSignal_Process(void)
{
    if (s_dma_fault != 0U) { s_dma_fault = 0U; s_busy = 0U; UI_Display_SetStatus("DMA_ERR"); }
    if (s_frame_ready != 0U) {
        s_frame_ready = 0U;
        analyse_frame();
        s_busy = 0U;
    }
}

uint8_t GSignal_IsBusy(void) { return s_busy; }

void GSignal_DMA2_Stream5_IRQHandler(void)
{
    uint32_t status = DMA2->HISR;
    if ((status & (DMA_HISR_TEIF5 | DMA_HISR_DMEIF5 | DMA_HISR_FEIF5)) != 0U) {
        DMA2->HIFCR = DMA_HIFCR_CFEIF5 | DMA_HIFCR_CDMEIF5 | DMA_HIFCR_CTEIF5 |
                      DMA_HIFCR_CHTIF5 | DMA_HIFCR_CTCIF5;
        TIM1->DIER &= ~TIM_DIER_UDE;
        TIM1->CR1 &= ~TIM_CR1_CEN;
        s_dma_fault = 1U;
    } else if ((status & DMA_HISR_TCIF5) != 0U) {
        DMA2->HIFCR = DMA_HIFCR_CTCIF5;
        TIM1->DIER &= ~TIM_DIER_UDE;
        TIM1->CR1 &= ~TIM_CR1_CEN;
        s_frame_ready = 1U;
    }
}
