/**
 * @file g_signal_u.c
 * @brief 第三题抗干扰模式实现：10.5MHz 采样 + 频域陷波 + 多正弦拟合。
 *
 * 与 g_signal_measurement.c 完全解耦，独立缓冲与状态。
 * 仅复用 g_signal_measurement.h 导出的底层算法函数。
 *
 * 抗干扰原理：
 *   信号频率 50kHz~500kHz，干扰 >=1MHz。10.5MHz 采样下 Nyquist=5.25MHz，
 *   干扰不混叠。FFT 后将 >=800kHz 分量置零（频域陷波）。
 *   Upp/Urms 由最小二乘拟合得出，拟合基函数 cos/sin 与干扰频率正交，
 *   干扰能量不污染拟合结果。
 */
#include "g_signal_u.h"

#include "g_signal_measurement.h"
#include "ui_controller.h"
#include "ui_display.h"
#include "ui_hmi_map.h"
#include "spectrum_analysis.h"
#include "g_signal_pc_debug.h"
#include "usart.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* ===== U 模式独立状态 ===== */
static TIM_HandleTypeDef *s_htim_u;
static volatile uint8_t s_busy_u;
static volatile uint8_t s_frame_ready_u;
static volatile uint8_t s_dma_fault_u;
static ui_requirement_t s_requirement_u;
static ui_view_t s_view_u;

/* U 模式采样率固定 10.5MHz */
#define G_SIGNALU_SAMPLE_RATE_HZ   G_SIGNAL_SAMPLE_RATE_10M_HZ
/* DMA 采样点数 = 帧点数 + 丢弃点数（和一二题共用 DISCARD 宏） */
#define G_SIGNALU_DMA_SAMPLES      (G_SIGNALU_FRAME_SAMPLES + G_SIGNAL_DISCARD_SAMPLES)

/* ===== U 模式缓冲区 =====
 * s_raw_u 是 DMA 目标，独立缓冲（不同采样点数）。
 * s_fft/s_magnitude 共享 g_signal_measurement.c 的缓冲（两模块互斥采集，
 * 不会同时访问）。U 模块用前 2048/1025 个元素做 2048 点 FFT，节省 20KB SRAM。
 * G_SIGNALU_FRAME_SAMPLES(2048) <= G_SIGNAL_FRAME_SAMPLES(8192)，不会越界。 */
static uint16_t s_raw_u[G_SIGNALU_DMA_SAMPLES];
/* IFFT 后的干净时域电压（陷波去干扰后的重建信号，供 fit_voltage_u 拟合） */
static float s_clean_voltage_u[G_SIGNALU_FRAME_SAMPLES];
/* s_fft/s_magnitude 来自 g_signal_measurement.c（extern 声明在头文件） */
static uint8_t s_wave_one_u[UI_HMI_WAVE_PLOT_POINTS];
static uint8_t s_wave_three_u[UI_HMI_WAVE_PLOT_POINTS];
static uint8_t s_spectrum_u[UI_HMI_SPECTRUM_PLOT_POINTS];

/* 8 帧环形缓冲（和一二题策略一致：指数平均 + 突变检测） */
#define AVG_FRAME_COUNT_U  8U
static float s_upp_history_u[AVG_FRAME_COUNT_U];
static float s_urms_history_u[AVG_FRAME_COUNT_U];
static float s_freq_history_u[AVG_FRAME_COUNT_U];
static uint8_t s_avg_index_u;
static uint8_t s_avg_filled_u;

/* ===== 工具函数（U 模块独立实现，不依赖 g_signal_measurement.c 的 static） ===== */

/* U 模式专用：对干净时域电压序列做最小二乘拟合。
 * 与 fit_multisine_and_upp 区别：输入直接是 float 电压（已陷波 IFFT 后），
 * 无需 remap_ad9226/code_to_input_voltage，彻底消除干扰对 a1/b1 的污染。
 * 法方程、求解逻辑与 fit_multisine_and_upp 完全一致，复用 solve_linear_system。 */
static float fit_voltage_u(const float *voltage, uint32_t length,
                           float sample_rate,
                           const float freq_in[3],
                           uint32_t harmonics_count,
                           float harmonics_freq[3],
                           float harmonics_amp[3],
                           float beta_out[FIT_MATRIX_DIM])
{
    static float s_fit_matrix_u[FIT_MATRIX_DIM * FIT_MATRIX_DIM];
    static float s_fit_vector_u[FIT_MATRIX_DIM];
    float beta[FIT_MATRIX_DIM];
    uint32_t dim = FIT_MATRIX_DIM;
    uint32_t n, k, row, col;
    float omega[FIT_MAX_HARMONICS];
    float freq[FIT_MAX_HARMONICS];
    float fund_upp_mV = 0.0f;

    if (harmonics_count < 1U) harmonics_count = 1U;
    if (harmonics_count > FIT_MAX_HARMONICS) harmonics_count = FIT_MAX_HARMONICS;

    for (k = 0U; k < FIT_MAX_HARMONICS; ++k) {
        freq[k] = (k < harmonics_count) ? freq_in[k] : 0.0f;
        omega[k] = 2.0f * 3.14159265358979f * freq[k] / sample_rate;
    }

    for (row = 0U; row < dim; ++row) {
        s_fit_vector_u[row] = 0.0f;
        for (col = 0U; col < dim; ++col) {
            s_fit_matrix_u[row * dim + col] = 0.0f;
        }
    }

    for (n = 0U; n < length; ++n) {
        float row_vec[FIT_MATRIX_DIM];
        float x = voltage[n];
        row_vec[0] = 1.0f;
        for (k = 0U; k < FIT_MAX_HARMONICS; ++k) {
            row_vec[1U + 2U * k] = 0.0f;
            row_vec[2U + 2U * k] = 0.0f;
        }
        for (k = 0U; k < harmonics_count; ++k) {
            float phi = omega[k] * (float)n;
            row_vec[1U + 2U * k] = cosf(phi);
            row_vec[2U + 2U * k] = sinf(phi);
        }
        for (row = 0U; row < dim; ++row) {
            if (row > 2U * harmonics_count) continue;
            s_fit_vector_u[row] += row_vec[row] * x;
            for (col = row; col < dim; ++col) {
                if (col > 2U * harmonics_count) continue;
                float prod = row_vec[row] * row_vec[col];
                s_fit_matrix_u[row * dim + col] += prod;
                if (row != col) {
                    s_fit_matrix_u[col * dim + row] += prod;
                }
            }
        }
    }

    for (k = harmonics_count; k < FIT_MAX_HARMONICS; ++k) {
        s_fit_matrix_u[(1U + 2U * k) * dim + (1U + 2U * k)] = 1.0f;
        s_fit_matrix_u[(2U + 2U * k) * dim + (2U + 2U * k)] = 1.0f;
    }

    if (solve_linear_system(s_fit_matrix_u, s_fit_vector_u, beta, dim) == 0U) {
        if (beta_out) {
            for (k = 0U; k < FIT_MATRIX_DIM; ++k) beta_out[k] = 0.0f;
        }
        for (k = 0U; k < 3U; ++k) {
            if (harmonics_freq) harmonics_freq[k] = (k < harmonics_count) ? freq[k % FIT_MAX_HARMONICS] : 0.0f;
            if (harmonics_amp) harmonics_amp[k] = 0.0f;
        }
        return 0.0f;
    }

    for (k = 0U; k < FIT_MAX_HARMONICS; ++k) {
        float ak = beta[1U + 2U * k];
        float bk = beta[2U + 2U * k];
        float peak = sqrtf(ak * ak + bk * bk);
        if (harmonics_amp) harmonics_amp[k] = (k < harmonics_count) ? peak : 0.0f;
        if (harmonics_freq) harmonics_freq[k] = (k < harmonics_count) ? freq[k] : 0.0f;
    }

    /* 数值搜索一个基波周期内的极值求 Upp */
    {
        uint32_t fine_n;
        uint32_t fine_points = 4096U;
        float model_max = -1.0e30f;
        float model_min = 1.0e30f;
        float fund_freq_local = freq[0];
        if (fund_freq_local < 1.0f) fund_freq_local = 1000.0f;
        for (fine_n = 0U; fine_n < fine_points; ++fine_n) {
            float t = (float)fine_n / (float)fine_points;
            float value = beta[0];
            for (k = 0U; k < harmonics_count; ++k) {
                float harmonic_ratio = freq[k] / fund_freq_local;
                float phase = 2.0f * 3.14159265358979f * harmonic_ratio * t;
                value += beta[1U + 2U * k] * cosf(phase);
                value += beta[2U + 2U * k] * sinf(phase);
            }
            if (value < model_min) model_min = value;
            if (value > model_max) model_max = value;
        }
        fund_upp_mV = (model_max - model_min) * 1000.0f;
    }
    if (beta_out) {
        for (k = 0U; k < FIT_MATRIX_DIM; ++k) beta_out[k] = beta[k];
    }
    return fund_upp_mV;
}

static uint8_t scale_to_u8_u(float value, float minimum, float maximum)
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

/* 在 U 模块自己的 s_magnitude 上找 Top3 真峰。
 * 和 g_signal_measurement.c 的 find_top3_peaks 逻辑一致，
 * 但操作独立缓冲，搜索范围限制在 bin4 ~ 陷波门限（信号<=500k，干扰>=1M）。
 * 返回真峰个数（1~3）。 */
static uint32_t find_top3_peaks_u(float freq_out[3], float amp_out[3])
{
    const float bin_freq = (float)G_SIGNALU_SAMPLE_RATE_HZ /
                           (float)G_SIGNALU_FRAME_SAMPLES;
    uint32_t rank, bin, r2;
    uint32_t selected[3] = { 0U, 0U, 0U };
    float raw_amp[3] = { 0.0f, 0.0f, 0.0f };
    float raw_freq[3] = { 0.0f, 0.0f, 0.0f };
    uint32_t real_count = 0U;
    /* 搜索上限：陷波门限 bin（>=此 bin 的频率分量将被置零，无需搜索） */
    uint32_t search_max = G_SIGNALU_NOTCH_BIN;

    for (rank = 0U; rank < 3U; ++rank) {
        uint32_t best = 4U;  /* 跳过 DC 附近 bin 1~3 */
        for (bin = 4U; bin < search_max; ++bin) {
            uint32_t prev;
            uint8_t rejected = 0U;
            for (prev = 0U; prev < rank; ++prev) {
                int32_t diff = (int32_t)bin - (int32_t)selected[prev];
                if (diff < 0) diff = -diff;
                if (diff < 3) rejected = 1U;  /* ±3bin≈15kHz 去重（2048点 bin 更宽） */
            }
            if (rejected == 0U && s_magnitude[bin] > s_magnitude[best]) best = bin;
        }
        selected[rank] = best;
    }
    /* 抛物线插值精化频率和幅值 */
    for (rank = 0U; rank < 3U; ++rank) {
        uint32_t b = selected[rank];
        float amp = s_magnitude[b];
        if (b > 0U && b + 1U < G_SIGNALU_BIN_COUNT) {
            float left = s_magnitude[b - 1U];
            float center = s_magnitude[b];
            float right = s_magnitude[b + 1U];
            float denom = left - 2.0f * center + right;
            float delta = 0.0f;
            if (fabsf(denom) > 1.0e-20f) {
                delta = 0.5f * (left - right) / denom;
                if (delta < -0.5f) delta = -0.5f;
                else if (delta > 0.5f) delta = 0.5f;
                amp = center - 0.25f * (left - right) * delta;
            }
            raw_freq[rank] = ((float)b + delta) * bin_freq;
        } else {
            raw_freq[rank] = (float)b * bin_freq;
        }
        raw_amp[rank] = amp;
    }
    /* 按幅度降序排列 */
    for (rank = 0U; rank < 2U; ++rank) {
        uint32_t max_idx = rank;
        for (r2 = rank + 1U; r2 < 3U; ++r2) {
            if (raw_amp[r2] > raw_amp[max_idx]) max_idx = r2;
        }
        if (max_idx != rank) {
            float tf = raw_freq[rank]; raw_freq[rank] = raw_freq[max_idx]; raw_freq[max_idx] = tf;
            float ta = raw_amp[rank];  raw_amp[rank]  = raw_amp[max_idx];  raw_amp[max_idx]  = ta;
        }
    }
    /* 相对门限过滤：幅度 < 主峰×2% 视为噪声假峰 */
    {
        float threshold = raw_amp[0] * 0.02f;
        for (rank = 0U; rank < 3U; ++rank) {
            if (raw_amp[rank] >= threshold) {
                freq_out[rank] = raw_freq[rank];
                amp_out[rank] = raw_amp[rank];
                ++real_count;
            } else {
                break;
            }
        }
    }
    return real_count;
}

/* 用拟合参数重建波形（和一二题 build_wave 逻辑一致，全频段数学级光滑）。
 * 模型 x(t) = d + Σ [ak·cos(2π·fk/f0·t) + bk·sin(2π·fk/f0·t)] */
static void build_wave_u(uint8_t *destination, uint8_t periods,
                         const float *beta, const float *freq,
                         uint32_t harmonics_count, float frequency_hz)
{
    uint32_t i, k;
    float minimum = 1.0e30f;
    float maximum = -1.0e30f;
    static float s_values_u[UI_HMI_WAVE_PLOT_POINTS];
    float dc = beta[0];

    if (frequency_hz < 1.0f) frequency_hz = 1000.0f;

    {
        float fund_freq = freq[0];
        if (fund_freq < 1.0f) fund_freq = 1000.0f;
        for (i = 0U; i < UI_HMI_WAVE_PLOT_POINTS; ++i) {
            float t = (float)i / (float)(UI_HMI_WAVE_PLOT_POINTS - 1U) * (float)periods;
            float value = dc;
            for (k = 0U; k < harmonics_count && k < FIT_MAX_HARMONICS; ++k) {
                float harmonic_ratio = freq[k] / fund_freq;
                float phase = 2.0f * 3.14159265358979f * harmonic_ratio * t;
                value += beta[1U + 2U * k] * cosf(phase);
                value += beta[2U + 2U * k] * sinf(phase);
            }
            s_values_u[i] = value;
            if (value < minimum) minimum = value;
            if (value > maximum) maximum = value;
        }
    }
    for (i = 0U; i < UI_HMI_WAVE_PLOT_POINTS; ++i) {
        destination[i] = scale_to_u8_u(s_values_u[i], minimum, maximum);
    }
}

/* 构建频谱显示数据（陷波后：>=800kHz 置零，频谱图不含干扰峰）。
 * 动态频率范围：0 ~ min(10×基波, 800kHz)。 */
static void build_spectrum_u(float *maximum_amplitude, float fundamental_freq)
{
    uint32_t point;
    float peak = 1.0e-12f;
    float threshold_abs;
    float threshold_rel;
    float threshold;
    float display_max_freq = fundamental_freq * 10.0f;
    /* 陷波门限 800kHz 为显示上限（超过的 bin 已置零） */
    float notch_freq = (float)G_SIGNALU_NOTCH_BIN *
                       (float)G_SIGNALU_SAMPLE_RATE_HZ /
                       (float)G_SIGNALU_FRAME_SAMPLES;
    uint32_t max_bin;
    if (display_max_freq > notch_freq) display_max_freq = notch_freq;
    if (display_max_freq < 50000.0f) display_max_freq = 50000.0f;
    max_bin = (uint32_t)(display_max_freq / (float)G_SIGNALU_SAMPLE_RATE_HZ *
                         (float)G_SIGNALU_FRAME_SAMPLES);
    if (max_bin < 10U) max_bin = 10U;
    if (max_bin > G_SIGNALU_NOTCH_BIN) max_bin = G_SIGNALU_NOTCH_BIN;

    for (point = 1U; point < max_bin; ++point) {
        if (s_magnitude[point] > peak) peak = s_magnitude[point];
    }
    /* 双门限底噪裁剪（和一二题一致） */
    threshold_abs = 0.005f;      /* 5mV */
    threshold_rel = peak * 0.01f;
    threshold = (threshold_abs > threshold_rel) ? threshold_abs : threshold_rel;
    for (point = 0U; point < UI_HMI_SPECTRUM_PLOT_POINTS; ++point) {
        uint32_t first = 1U + point * (max_bin - 1U) /
                         UI_HMI_SPECTRUM_PLOT_POINTS;
        uint32_t last = 1U + (point + 1U) * (max_bin - 1U) /
                        UI_HMI_SPECTRUM_PLOT_POINTS;
        float local = 0.0f;
        uint32_t bin;
        float scaled;
        for (bin = first; bin <= last && bin < G_SIGNALU_BIN_COUNT; ++bin) {
            if (s_magnitude[bin] > local) local = s_magnitude[bin];
        }
        if (local < threshold) local = threshold;
        /* 对数刻度：0 ~ -40dB 映射到 255 ~ 0 */
        scaled = 20.0f * log10f(local / peak + 1.0e-10f);
        scaled = (scaled + 40.0f) * 255.0f / 40.0f;
        if (scaled < 0.0f) scaled = 0.0f;
        if (scaled > 255.0f) scaled = 255.0f;
        s_spectrum_u[point] = (uint8_t)(scaled + 0.5f);
    }
    *maximum_amplitude = peak;
}

/* Top3 分量幅值提取（用拟合值，和一二题 make_top3 逻辑一致） */
static void make_top3_u(ui_spectrum_measurement_t *result,
                        const float harm_freq[3], const float harm_amp[3])
{
    uint32_t i, j;
    float freq_copy[3];
    float amp_copy[3];

    for (i = 0U; i < 3U; ++i) {
        freq_copy[i] = harm_freq[i];
        amp_copy[i]  = harm_amp[i];
    }
    for (i = 0U; i < 2U; ++i) {
        uint32_t max_idx = i;
        for (j = i + 1U; j < 3U; ++j) {
            if (amp_copy[j] > amp_copy[max_idx]) max_idx = j;
        }
        if (max_idx != i) {
            float tf = freq_copy[i]; freq_copy[i] = freq_copy[max_idx]; freq_copy[max_idx] = tf;
            float ta = amp_copy[i];  amp_copy[i]  = amp_copy[max_idx];  amp_copy[max_idx]  = ta;
        }
    }
    for (i = 0U; i < 3U; ++i) {
        float f_hz = freq_copy[i];
        float a_v  = amp_copy[i];
        if (f_hz < 0.0f) f_hz = 0.0f;
        if (a_v  < 0.0f) a_v  = 0.0f;
        result->frequency_mHz[i] = (uint32_t)(f_hz * 1000.0f + 0.5f);
        result->amplitude_mV[i]  = (uint32_t)(a_v  * 1000.0f + 0.5f);
    }
}

/* ===== U 模式帧分析核心 =====
 * 流程：
 *   1. 预处理统计（fallback 用）
 *   2. 加汉宁窗 FFT → 幅度谱
 *   3. 频域陷波（>=800kHz bin 置零）
 *   4. 找 Top3 真峰（在陷波后频谱上，干扰已置零）
 *   5. 频率精化（原始 raw，IEEE 1057，干扰正交不影响）
 *   6. 多正弦拟合（原始 raw，得 Upp/Urms/beta/harm）
 *   7. 8 帧环形缓冲 + 突变检测
 *   8. 波形重建 + 频谱显示 + Top3 输出
 */
static void analyse_frame_u(void)
{
    uint32_t i;
    float minimum = 1.0e30f, maximum = -1.0e30f, sum2 = 0.0f;
    float coherent_gain;
    spectrum_peak_t fundamental;
    ui_wave_measurement_t wave;
    ui_spectrum_measurement_t spectrum;
    float ignored_peak;
    float fit_upp_mV;
    float harm_freq[3];
    float harm_amp[3];
    uint32_t real_peak_count;
    float fit_beta[FIT_MATRIX_DIM];
    float upp_avg, urms_avg, freq_avg;
    uint32_t notch_bin;

    /* 1. 预处理：统计 min/max/sum2（含干扰，仅 fallback 用） */
    for (i = 0U; i < G_SIGNALU_FRAME_SAMPLES; ++i) {
        float value = code_to_input_voltage(remap_ad9226(s_raw_u[i + G_SIGNAL_DISCARD_SAMPLES]));
        if (value < minimum) minimum = value;
        if (value > maximum) maximum = value;
        sum2 += value * value;
    }

    /* 2. 加汉宁窗 FFT → 幅度谱 */
    coherent_gain = 1.0f;
    (void)sp_window_coherent_gain_f32(SP_WINDOW_HANN,
                                      G_SIGNALU_FRAME_SAMPLES,
                                      &coherent_gain);
    if (coherent_gain <= 0.0f) coherent_gain = 1.0f;
    for (i = 0U; i < G_SIGNALU_FRAME_SAMPLES; ++i) {
        float value = code_to_input_voltage(remap_ad9226(s_raw_u[i + G_SIGNAL_DISCARD_SAMPLES]));
        float coefficient = 1.0f;
        (void)sp_window_coefficient_f32(SP_WINDOW_HANN,
                                        i,
                                        G_SIGNALU_FRAME_SAMPLES,
                                        &coefficient);
        s_fft[i].real = value * coefficient;
        s_fft[i].imag = 0.0f;
    }
    (void)fft_transform_inplace_f32(s_fft, G_SIGNALU_FRAME_SAMPLES, 0U);
    for (i = 0U; i < G_SIGNALU_BIN_COUNT; ++i) {
        float amplitude = sqrtf(s_fft[i].real * s_fft[i].real +
                                s_fft[i].imag * s_fft[i].imag) /
                          ((float)G_SIGNALU_FRAME_SAMPLES * coherent_gain);
        if ((i != 0U) && (i != (G_SIGNALU_FRAME_SAMPLES / 2U))) amplitude *= 2.0f;
        s_magnitude[i] = amplitude;
    }

    /* 3. 频域陷波 s_magnitude：>=800kHz 的频率分量置零（干扰>=1MHz，信号<=500kHz）。
     *    仅清零 s_magnitude（用于频谱显示和找峰），s_fft 后面会用未加窗信号重新填充。
     *    注意：不能在此对加窗 s_fft 做 IFFT，否则恢复的是加窗信号 w(t)·signal(t)，
     *    fit_voltage_u 法方程无窗加权会导致幅度衰减 0.5 倍（汉宁窗相干增益）。 */
    notch_bin = G_SIGNALU_NOTCH_BIN;
    for (i = notch_bin; i < G_SIGNALU_BIN_COUNT; ++i) {
        s_magnitude[i] = 0.0f;
    }

    /* 4. 找 Top3 真峰（在陷波后频谱上，搜索范围 bin4~陷波门限） */
    real_peak_count = find_top3_peaks_u(harm_freq, harm_amp);
    fundamental.frequency_hz = harm_freq[0];

    /* 5. 频率精化（IEEE 1057，在原始 raw 上，干扰正交不影响）
     *    U 模式用 2048 点全点精化，bin 分辨率 5.13kHz，
     *    精化后频率偏差 <1Hz，拟合精度有保障。 */
    fundamental.frequency_hz = refine_frequency(&s_raw_u[G_SIGNAL_DISCARD_SAMPLES],
                                                G_SIGNALU_FRAME_SAMPLES,
                                                (float)G_SIGNALU_SAMPLE_RATE_HZ,
                                                fundamental.frequency_hz);

    /* 谐波频率 = 精化基波 × 谐波次数（和一二题一致） */
    {
        float fund_refined = fundamental.frequency_hz;
        float fund_fft = harm_freq[0];
        uint32_t k;
        harm_freq[0] = fund_refined;
        for (k = 1U; k < real_peak_count; ++k) {
            float ratio = harm_freq[k] / fund_fft;
            uint32_t harmonic_order = (uint32_t)(ratio + 0.5f);
            if (harmonic_order < 2U) harmonic_order = 2U;
            harm_freq[k] = fund_refined * (float)harmonic_order;
        }
    }

    /* 6. 未加窗 FFT → 完整陷波（正负频率）→ IFFT → 干净时域电压。
     *    关键：不能用步骤 2 的加窗 s_fft 做 IFFT！加窗后 IFFT 恢复的是 w(t)·signal(t)，
     *    而 fit_voltage_u 法方程无窗加权，会导致拟合幅度衰减（汉宁窗相干增益 0.5，
     *    叠加负频率未清零的干扰残留，实测 A1 偏小到 1/4）。
     *    正确做法：用未加窗的原始电压重新做 FFT，完整清零 >=800kHz 的正负频率 bin，
     *    再 IFFT 得到无窗衰减、无干扰的干净信号，供 fit_voltage_u 精确拟合。 */
    for (i = 0U; i < G_SIGNALU_FRAME_SAMPLES; ++i) {
        s_fft[i].real = code_to_input_voltage(remap_ad9226(s_raw_u[i + G_SIGNAL_DISCARD_SAMPLES]));
        s_fft[i].imag = 0.0f;
    }
    (void)fft_transform_inplace_f32(s_fft, G_SIGNALU_FRAME_SAMPLES, 0U);
    /* 完整陷波：清零 bin[notch_bin .. N-notch_bin]（正负频率对称置零）。
     * 2048 点 FFT 中 bin k 和 bin N-k 是共轭对（正负频率），必须同时清零，
     * 否则 IFFT 后会残留高频分量。范围：bin 156 到 2048-156=1892。 */
    for (i = notch_bin; i < G_SIGNALU_FRAME_SAMPLES - notch_bin; ++i) {
        s_fft[i].real = 0.0f;
        s_fft[i].imag = 0.0f;
    }
    (void)fft_transform_inplace_f32(s_fft, G_SIGNALU_FRAME_SAMPLES, 1U);
    for (i = 0U; i < G_SIGNALU_FRAME_SAMPLES; ++i) {
        s_clean_voltage_u[i] = s_fft[i].real;
    }

    /* 7. 多正弦最小二乘拟合（在未加窗的干净电压上）。
     *    fit_voltage_u 输入是 s_clean_voltage_u（未加窗、已陷波去干扰），
     *    无窗衰减、无干扰残留，拟合幅度精确恢复。
     *    U 模式用 2048 点全点拟合（与帧长一致），条件数充足，拟合稳定。 */
    fit_upp_mV = fit_voltage_u(s_clean_voltage_u,
                               G_SIGNALU_FRAME_SAMPLES,
                               (float)G_SIGNALU_SAMPLE_RATE_HZ,
                               harm_freq,
                               real_peak_count,
                               harm_freq, harm_amp, fit_beta);

    /* 8. 8 帧环形缓冲 + 突变检测（和一二题策略一致） */
    {
        float new_upp = (fit_upp_mV > 0.0f) ? fit_upp_mV :
                        (maximum - minimum) * 1000.0f;
        /* Urms 用拟合参数 √(ΣAk²/2)×1000，剔除 DC 和干扰能量 */
        float new_urms;
        if (fit_upp_mV > 0.0f) {
            new_urms = sqrtf((harm_amp[0] * harm_amp[0] +
                              harm_amp[1] * harm_amp[1] +
                              harm_amp[2] * harm_amp[2]) * 0.5f) * 1000.0f;
        } else {
            new_urms = sqrtf(sum2 / (float)G_SIGNALU_FRAME_SAMPLES) * 1000.0f;
        }
        float new_freq = fundamental.frequency_hz;

        if (s_avg_filled_u > 0U) {
            float hist_upp_avg = 0.0f;
            float hist_freq_avg = 0.0f;
            uint8_t signal_changed = 0U;
            for (i = 0U; i < s_avg_filled_u; ++i) {
                hist_upp_avg += s_upp_history_u[i];
                hist_freq_avg += s_freq_history_u[i];
            }
            hist_upp_avg /= (float)s_avg_filled_u;
            hist_freq_avg /= (float)s_avg_filled_u;
            if (hist_upp_avg > 1.0f &&
                fabsf(new_upp - hist_upp_avg) > 0.25f * hist_upp_avg) {
                signal_changed = 1U;
            }
            if (hist_freq_avg > 100.0f &&
                fabsf(new_freq - hist_freq_avg) > 0.10f * hist_freq_avg) {
                signal_changed = 1U;
            }
            if (signal_changed) {
                s_avg_index_u = 0U;
                s_avg_filled_u = 0U;
            }
        }

        s_upp_history_u[s_avg_index_u] = new_upp;
        s_urms_history_u[s_avg_index_u] = new_urms;
        s_freq_history_u[s_avg_index_u] = new_freq;
        s_avg_index_u = (s_avg_index_u + 1U) % AVG_FRAME_COUNT_U;
        if (s_avg_filled_u < AVG_FRAME_COUNT_U) s_avg_filled_u++;
    }

    upp_avg = 0.0f; urms_avg = 0.0f; freq_avg = 0.0f;
    for (i = 0U; i < s_avg_filled_u; ++i) {
        upp_avg += s_upp_history_u[i];
        urms_avg += s_urms_history_u[i];
        freq_avg += s_freq_history_u[i];
    }
    upp_avg /= (float)s_avg_filled_u;
    urms_avg /= (float)s_avg_filled_u;
    freq_avg /= (float)s_avg_filled_u;

    /* 8. 输出结果 */
    wave.upp_mV = (uint32_t)(upp_avg + 0.5f);
    wave.urms_mV = (uint32_t)(urms_avg + 0.5f);
    wave.fundamental_mHz = (uint32_t)(freq_avg * 1000.0f + 0.5f);

    build_wave_u(s_wave_one_u, 1U, fit_beta, harm_freq,
                 real_peak_count, fundamental.frequency_hz);
    build_wave_u(s_wave_three_u, 3U, fit_beta, harm_freq,
                 real_peak_count, fundamental.frequency_hz);
    build_spectrum_u(&ignored_peak, fundamental.frequency_hz);
    make_top3_u(&spectrum, harm_freq, harm_amp);

    UI_Controller_SetWaveMeasurement(&wave);
    UI_Controller_SetWaveSamplesForView(UI_VIEW_WAVE_1PERIOD, s_wave_one_u, UI_HMI_WAVE_PLOT_POINTS);
    UI_Controller_SetWaveSamplesForView(UI_VIEW_WAVE_3PERIOD, s_wave_three_u, UI_HMI_WAVE_PLOT_POINTS);
    UI_Controller_SetSpectrumMeasurement(&spectrum);
    UI_Controller_SetSpectrumSamples(s_spectrum_u, UI_HMI_SPECTRUM_PLOT_POINTS);
    /* PC 串口旁路输出 */
    GSignal_PCDebug_Print(&wave, &spectrum, s_wave_one_u, s_spectrum_u);
    if (s_view_u == UI_VIEW_SPECTRUM) UI_Controller_RefreshSpectrum();
    else UI_Controller_RefreshWave(s_view_u);
    UI_Display_SetStatus("READY");
}

/* ===== 公开接口 ===== */

HAL_StatusTypeDef GSignalU_Init(TIM_HandleTypeDef *htim)
{
    if (htim == NULL || htim->Instance != TIM1) return HAL_ERROR;
    s_htim_u = htim;
    s_busy_u = 0U; s_frame_ready_u = 0U; s_dma_fault_u = 0U;
    return HAL_OK;
}

void GSignalU_Abort(void)
{
    if (s_busy_u != 0U) {
        DMA2_Stream5->CR &= ~DMA_SxCR_EN;
        while ((DMA2_Stream5->CR & DMA_SxCR_EN) != 0U) { }
        __HAL_TIM_DISABLE(s_htim_u);
        TIM1->DIER &= ~TIM_DIER_UDE;
        s_busy_u = 0U;
    }
    /* 清空环形缓冲，避免模式切换后旧数据残留 */
    s_avg_index_u = 0U;
    s_avg_filled_u = 0U;
    s_frame_ready_u = 0U;
    s_dma_fault_u = 0U;
}

void GSignalU_Request(ui_requirement_t requirement, ui_view_t view)
{
    if (s_htim_u == NULL || s_busy_u != 0U) return;
    s_requirement_u = requirement;
    s_view_u = view;
    s_frame_ready_u = 0U; s_dma_fault_u = 0U; s_busy_u = 1U;

    /* 诊断：通知 PC 收到 U 模式采集请求 */
    {
        extern UART_HandleTypeDef huart1;
        const char *view_name = "?";
        if (view == UI_VIEW_WAVE_1PERIOD) view_name = "U-WAVE1";
        else if (view == UI_VIEW_WAVE_3PERIOD) view_name = "U-WAVE3";
        else if (view == UI_VIEW_SPECTRUM) view_name = "U-SPECT";
        char msg[64];
        (void)snprintf(msg, sizeof(msg), "REQ: %s\r\n", view_name);
        (void)HAL_UART_Transmit(&huart1, (uint8_t *)msg, (uint16_t)strlen(msg), 100);
    }
    UI_Display_SetStatus("CAPTURE");

    /* 配置 DMA2_Stream5：GPIOE->IDR → s_raw_u，2048+8 点 */
    DMA2_Stream5->CR &= ~DMA_SxCR_EN;
    while ((DMA2_Stream5->CR & DMA_SxCR_EN) != 0U) { }
    DMA2->HIFCR = DMA_HIFCR_CFEIF5 | DMA_HIFCR_CDMEIF5 | DMA_HIFCR_CTEIF5 |
                  DMA_HIFCR_CHTIF5 | DMA_HIFCR_CTCIF5;
    DMA2_Stream5->PAR = (uint32_t)&GPIOE->IDR;
    DMA2_Stream5->M0AR = (uint32_t)s_raw_u;
    DMA2_Stream5->NDTR = G_SIGNALU_DMA_SAMPLES;
    DMA2_Stream5->CR = (6U << DMA_SxCR_CHSEL_Pos) | DMA_SxCR_PL_1 |
                       DMA_SxCR_MINC | DMA_SxCR_PSIZE_0 | DMA_SxCR_MSIZE_0 |
                       DMA_SxCR_TCIE | DMA_SxCR_TEIE;
    DMA2_Stream5->CR |= DMA_SxCR_EN;

    /* 配置 TIM1：10.5MHz 采样（ARR=15, CCR1=8, 50%占空比 → 5.25MHz 方波）。
     * 不用 HAL_TIM_PWM_Start（其内部 __HAL_TIM_ENABLE 会在 DIER.UDE 设置前
     * 启动 TIM1，导致重复采集时 DMA 触发时序不确定，ADC 无时钟输出全 128）。
     * 改为寄存器操作：先配 ARR/CCR1/CCER/BDTR，再设 UDE，最后使能 CEN，
     * 确保第一个更新事件就能正确触发 DMA。 */
    __HAL_TIM_SET_COUNTER(s_htim_u, 0U);
    __HAL_TIM_DISABLE(s_htim_u);
    s_htim_u->Instance->CR1 &= ~TIM_CR1_ARPE;
    __HAL_TIM_SET_AUTORELOAD(s_htim_u, G_SIGNAL_TIM1_ARR_10M);
    __HAL_TIM_SET_COMPARE(s_htim_u, TIM_CHANNEL_1, 8U);
    TIM1->SR = 0U;                     /* 清除残留标志（UIF 等） */
    TIM1->CCER |= TIM_CCER_CC1E;       /* 使能 CH1 输出 */
    TIM1->BDTR |= TIM_BDTR_MOE;        /* 使能主输出 */
    TIM1->DIER |= TIM_DIER_UDE;        /* 先设 UDE，再启动 TIM1 */
    __HAL_TIM_ENABLE(s_htim_u);         /* 启动 TIM1，第一个更新事件即触发 DMA */
}

void GSignalU_Process(void)
{
    if (s_dma_fault_u != 0U) {
        s_dma_fault_u = 0U;
        s_busy_u = 0U;
        UI_Display_SetStatus("DMA_ERR");
    }
    if (s_frame_ready_u != 0U) {
        s_frame_ready_u = 0U;
        analyse_frame_u();
        s_busy_u = 0U;
    }
}

uint8_t GSignalU_IsBusy(void) { return s_busy_u; }

void GSignalU_DMA2_Stream5_IRQHandler(void)
{
    uint32_t status = DMA2->HISR;
    if ((status & (DMA_HISR_TEIF5 | DMA_HISR_DMEIF5 | DMA_HISR_FEIF5)) != 0U) {
        DMA2->HIFCR = DMA_HIFCR_CFEIF5 | DMA_HIFCR_CDMEIF5 | DMA_HIFCR_CTEIF5 |
                      DMA_HIFCR_CHTIF5 | DMA_HIFCR_CTCIF5;
        TIM1->DIER &= ~TIM_DIER_UDE;
        TIM1->CR1 &= ~TIM_CR1_CEN;
        s_dma_fault_u = 1U;
    } else if ((status & DMA_HISR_TCIF5) != 0U) {
        DMA2->HIFCR = DMA_HIFCR_CTCIF5;
        TIM1->DIER &= ~TIM_DIER_UDE;
        TIM1->CR1 &= ~TIM_CR1_CEN;
        s_frame_ready_u = 1U;
    }
}
