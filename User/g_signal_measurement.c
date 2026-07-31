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
/* 8 帧环形缓冲，用于指数平均提升精度（精度 ×√8≈2.8） */
#define AVG_FRAME_COUNT  8U
static float s_upp_history[AVG_FRAME_COUNT];
static float s_urms_history[AVG_FRAME_COUNT];
static float s_freq_history[AVG_FRAME_COUNT];
static uint8_t s_avg_index;
static uint8_t s_avg_filled;

/* AD9226 数据位重映射（PCB布局优化版，引脚位序不连续）
 * DMA 读取 GPIOE->IDR 全16位，12根数据线散布在 bit3/5~15，需按下表
 * 提取并组合为连续12位码值。AD9226 的 AD0=MSB、AD11=LSB，故
 * AD0→bit11(MSB) ... AD11→bit0(LSB)（与原 reverse12 输出位序一致）：
 *   AD0(MSB)→PE15  AD1→PE14  AD2→PE13  AD3→PE12  AD4→PE11  AD5→PE10
 *   AD6→PE9         AD7→PE8   AD8→PE7   AD9→PE5   AD10→PE6  AD11(LSB)→PE3
 * 直接传入 IDR 原始值，无需外部先做 & 0x0FFF。 */
static uint16_t remap_ad9226(uint16_t idr)
{
    uint16_t r = 0U;
    if (idr & 0x8000U) r |= 0x0800U;  /* AD0  PE15 → bit11 (MSB) */
    if (idr & 0x4000U) r |= 0x0400U;  /* AD1  PE14 → bit10 */
    if (idr & 0x2000U) r |= 0x0200U;  /* AD2  PE13 → bit9  */
    if (idr & 0x1000U) r |= 0x0100U;  /* AD3  PE12 → bit8  */
    if (idr & 0x0800U) r |= 0x0080U;  /* AD4  PE11 → bit7  */
    if (idr & 0x0400U) r |= 0x0040U;  /* AD5  PE10 → bit6  */
    if (idr & 0x0200U) r |= 0x0020U;  /* AD6  PE9  → bit5  */
    if (idr & 0x0100U) r |= 0x0010U;  /* AD7  PE8  → bit4  */
    if (idr & 0x0080U) r |= 0x0008U;  /* AD8  PE7  → bit3  */
    if (idr & 0x0020U) r |= 0x0004U;  /* AD9  PE5  → bit2  */
    if (idr & 0x0040U) r |= 0x0002U;  /* AD10 PE6  → bit1  */
    if (idr & 0x0008U) r |= 0x0001U;  /* AD11 PE3  → bit0 (LSB) */
    return r;
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


/* ===== 多正弦最小二乘拟合 =====
 * 模型 x[n] = d + Σ_k [ak·cos(ωk·n) + bk·sin(ωk·n)]，K≤3（基波+2谐波）
 * 构造法方程 (XᵀX)·β = Xᵀx，高斯消元解 (2K+1)×(2K+1) 线性方程组。
 * 避免 FFT 单 bin 读幅值的频谱泄漏问题，精度从 5mV 提升到 2.5mV。
 * 无需硬件放大即可在小信号下精确提取幅值。 */
#define FIT_MAX_HARMONICS   3U
#define FIT_MATRIX_DIM      (2U * FIT_MAX_HARMONICS + 1U)  /* 7×7 */

static float s_fit_matrix[FIT_MATRIX_DIM * FIT_MATRIX_DIM];
static float s_fit_vector[FIT_MATRIX_DIM];
static float s_fit_beta[FIT_MATRIX_DIM];

/* 高斯消元法解线性方程组 A·x = b，n 维。带部分主元选择。 */
static uint8_t solve_linear_system(float *A, float *b, float *x, uint32_t n)
{
    uint32_t col, row, pivot, elim;
    for (col = 0U; col < n; ++col) {
        /* 选主元 */
        pivot = col;
        for (row = col + 1U; row < n; ++row) {
            if (fabsf(A[row * n + col]) > fabsf(A[pivot * n + col])) {
                pivot = row;
            }
        }
        if (fabsf(A[pivot * n + col]) < 1.0e-12f) {
            return 0U;  /* 奇异 */
        }
        /* 交换行 */
        if (pivot != col) {
            for (uint32_t k = 0U; k < n; ++k) {
                float tmp = A[pivot * n + k];
                A[pivot * n + k] = A[col * n + k];
                A[col * n + k] = tmp;
            }
            float tmpb = b[pivot];
            b[pivot] = b[col];
            b[col] = tmpb;
        }
        /* 消元 */
        for (row = col + 1U; row < n; ++row) {
            float factor = A[row * n + col] / A[col * n + col];
            A[row * n + col] = 0.0f;
            for (elim = col + 1U; elim < n; ++elim) {
                A[row * n + elim] -= factor * A[col * n + elim];
            }
            b[row] -= factor * b[col];
        }
    }
    /* 回代 */
    for (int32_t r = (int32_t)n - 1; r >= 0; --r) {
        float sum = b[r];
        for (uint32_t c = (uint32_t)r + 1U; c < n; ++c) {
            sum -= A[r * n + c] * x[c];
        }
        x[r] = sum / A[r * n + r];
    }
    return 1U;
}

/* ===== 频段划分（仅用于幅值拟合参数，频率精化统一用最精密算法）=====
 * 调试策略：一个频段一个频段调，调好后参数固定，再调下一个频段。
 *
 * 频段划分（基于赛题 50kHz~500kHz 范围）：
 *   LOW  (<100kHz)  ：每周期 40+ 点，拟合精度好
 *   MID  (100k~300k) ：每周期 13~40 点
 *   HIGH (>300kHz)  ：每周期 8~13 点，相位累积敏感
 *
 * 初始状态：所有频段都用 v3-release 验证参数（8192 点），等效于 v3-release。
 * 调试时逐个频段调整 fit_length，找到该频段最优参数后固定。 */
typedef enum {
    FREQ_BAND_LOW,      /* <100kHz */
    FREQ_BAND_MID,      /* 100k~300kHz */
    FREQ_BAND_HIGH      /* >300kHz */
} freq_band_t;

static freq_band_t get_freq_band(float freq_hz)
{
    if (freq_hz < 100000.0f) return FREQ_BAND_LOW;
    if (freq_hz < 300000.0f) return FREQ_BAND_MID;
    return FREQ_BAND_HIGH;
}

/* 根据频段获取幅值拟合点数
 * 理论依据：fit_length(N) 平衡相位累积误差(∝N·Δf)与量化噪声(∝1/√N)
 *   HIGH(>300k): Δf≈150Hz, N=512 时 θ=0.12rad 误差0.04mV（最优）
 *   MID/LOW: 暂用8192，逐频段调试时再调整 */
static uint32_t get_band_fit_length(freq_band_t band)
{
    switch (band) {
        case FREQ_BAND_LOW:   return 8192U;  /* 待调 */
        case FREQ_BAND_MID:   return 8192U;  /* 待调 */
        case FREQ_BAND_HIGH:  return 512U;   /* HIGH已调：512点 */
        default:              return 8192U;
    }
}

/* ===== 频率精化（3 参数高斯-牛顿迭代，IEEE 1057）=====
 * 模型 x[n] = d + a·cos(ωn) + b·sin(ωn)，ω=2πf/fs
 * 高斯-牛顿迭代：每轮先 3×3 法方程解 [d,a,b]，再用残差梯度修正频率。
 * - FFT 抛物线插值精度 0.1bin≈49Hz，精化后 <1Hz
 * - Upp 拟合精度从 ±5mV 提升到 ±1mV（频率偏差会污染 a1/b1 系数）
 * 3×3 矩阵条件数好，全频段稳定收敛不发散。
 * 15 次迭代 + 0.5Hz 收敛门限（v3-release 验证参数，全频段精度 ±1mV）。
 * 频率修正限制 ±10% 防发散。
 * 参考：梁志国等《四参数正弦波曲线拟合的快速算法》计量学报2006 */
#define FREQ_REFINE_ITERATIONS_MAX  15U
#define FREQ_REFINE_CONVERGE_HZ     0.5f

static float refine_frequency(const uint16_t *raw, uint32_t length,
                              float sample_rate, float freq_init)
{
    float freq = freq_init;
    float freq_min = freq_init * 0.9f;
    float freq_max = freq_init * 1.1f;
    uint32_t iter, n;
    const float two_pi_over_fs = 2.0f * 3.14159265358979f / sample_rate;

    for (iter = 0U; iter < FREQ_REFINE_ITERATIONS_MAX; ++iter) {
        float omega = two_pi_over_fs * freq;
        /* 3x3 法方程：[DC, a, b]，对称矩阵用 9 元素一维存储 */
        float A[9] = { 0.0f };
        float b_vec[3] = { 0.0f };
        float beta3[3];
        float a_coef, b_coef;
        float num = 0.0f, den = 0.0f;
        float delta_f;

        /* 第一遍：3 参数线性拟合，累加法方程 */
        for (n = 0U; n < length; ++n) {
            float phi = omega * (float)n;
            float c = cosf(phi);
            float s = sinf(phi);
            float x = code_to_input_voltage(remap_ad9226(raw[n]));
            A[0] += 1.0f;                                /* DC·DC */
            A[1] += c;                A[3] += c;         /* DC·cos */
            A[2] += s;                A[6] += s;         /* DC·sin */
            A[4] += c * c;            A[5] += c * s;     /* cos·cos, cos·sin */
            A[8] += s * s;                               /* sin·sin */
            b_vec[0] += x;
            b_vec[1] += x * c;
            b_vec[2] += x * s;
        }
        A[7] = A[5];  /* sin·cos = cos·sin，对称 */
        if (solve_linear_system(A, b_vec, beta3, 3U) == 0U) break;
        a_coef = beta3[1];
        b_coef = beta3[2];

        /* 第二遍：计算残差和频率梯度，高斯-牛顿修正
         * 残差 r[n] = x[n] - m[n]
         * ∂m/∂f = (-a·n·sin(ωn) + b·n·cos(ωn)) · 2π/fs
         * Δf = Σ(r·∂m/∂f) / Σ((∂m/∂f)²) */
        for (n = 0U; n < length; ++n) {
            float phi = omega * (float)n;
            float c = cosf(phi);
            float s = sinf(phi);
            float x = code_to_input_voltage(remap_ad9226(raw[n]));
            float model = beta3[0] + a_coef * c + b_coef * s;
            float residual = x - model;
            float dm_df = (-a_coef * s + b_coef * c) * (float)n * two_pi_over_fs;
            num += residual * dm_df;
            den += dm_df * dm_df;
        }
        if (den < 1.0e-20f) break;
        delta_f = num / den;
        freq += delta_f;
        /* 限制频率在 ±10% 范围内防发散 */
        if (freq < freq_min) freq = freq_min;
        if (freq > freq_max) freq = freq_max;
        /* 收敛门限：单步修正 < 0.5Hz 立即退出 */
        if (fabsf(delta_f) < FREQ_REFINE_CONVERGE_HZ) break;
    }
    return freq;
}

/* 从幅度谱找真峰，按幅度降序输出（最强=基波）。
 * 解决三个问题：
 *   1. 旧版只找全局最大峰，但 Top3 都要输出
 *   2. 旧版硬编码 2次/3次谐波，赛题谐波次数任意（如3次+4次）无法处理
 *   3. 旧版强制返回 3 个峰，单频信号也塞 2 个噪声峰，导致 fit 矩阵奇异
 * 策略：跳过 bin 1~3 避免 DC 旁瓣伪峰，峰间距≥5bin 去重，
 *       按幅度选最多 3 个峰，再用相对门限（主峰×5%）过滤噪声假峰。
 *       单频信号只返回 1 个真峰，双频返回 2 个，三频返回 3 个。
 *       fit 按实际真峰数拟合，不硬塞假峰，避免矩阵奇异。
 * 抛物线插值精化频率和幅值。
 * 返回值：真峰个数（1~3）。 */
static uint32_t find_top3_peaks(float freq_out[3], float amp_out[3])
{
    const float bin_freq = (float)G_SIGNAL_SAMPLE_RATE_HZ / (float)G_SIGNAL_FRAME_SAMPLES;
    uint32_t rank, bin, r2;
    uint32_t selected[3] = { 0U, 0U, 0U };
    float raw_amp[3] = { 0.0f, 0.0f, 0.0f };
    float raw_freq[3] = { 0.0f, 0.0f, 0.0f };
    uint32_t real_count = 0U;

    for (rank = 0U; rank < 3U; ++rank) {
        uint32_t best = 4U;  /* 从 bin 4 开始，跳过 DC 附近 */
        for (bin = 4U; bin + 1U < G_SIGNAL_BIN_COUNT; ++bin) {
            uint32_t prev;
            uint8_t rejected = 0U;
            for (prev = 0U; prev < rank; ++prev) {
                int32_t diff = (int32_t)bin - (int32_t)selected[prev];
                if (diff < 0) diff = -diff;
                if (diff < 5) rejected = 1U;  /* ±5bin≈2.4kHz 去重 */
            }
            if (rejected == 0U && s_magnitude[bin] > s_magnitude[best]) best = bin;
        }
        selected[rank] = best;
    }
    /* 抛物线插值精化频率和幅值 */
    for (rank = 0U; rank < 3U; ++rank) {
        uint32_t b = selected[rank];
        float amp = s_magnitude[b];
        if (b > 0U && b + 1U < G_SIGNAL_BIN_COUNT) {
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
    /* 按幅度降序排列（最强=基波），选择排序 */
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
    /* 相对门限过滤：幅度 < 主峰×5% 视为噪声假峰，不输出。
     * 单频信号 → 只 1 个真峰；双频 → 2 个；三频 → 3 个。
     * 这样 fit 只对真峰拟合，不硬塞假峰，彻底避免矩阵奇异。 */
    {
        float threshold = raw_amp[0] * 0.05f;
        for (rank = 0U; rank < 3U; ++rank) {
            if (raw_amp[rank] >= threshold) {
                freq_out[rank] = raw_freq[rank];
                amp_out[rank] = raw_amp[rank];
                ++real_count;
            } else {
                break;  /* 已按幅度降序，后面都更小 */
            }
        }
    }
    return real_count;
}

/* 多正弦最小二乘拟合：返回基波峰峰值（mV），通过 fund_freq 返回精确基波频率。
 * 输入：raw ADC 码值缓冲（已含 DISCARD 偏移）、采样率、3 个实际频率（来自 FFT Top3 峰）。
 * 逐点累加法方程，避免构造 8192×7 大矩阵，省内存。
 * 内部直接从码值转换电压，无需额外 float 缓冲区（省 32KB SRAM）。
 * beta_out 输出 [d, a1, b1, a2, b2, a3, b3] 供波形重建使用。
 * freq_in[0]=基波频率，freq_in[1/2]=谐波频率（任意次数，不假设 2/3 次）。 */
static float fit_multisine_and_upp(const uint16_t *raw, uint32_t length,
                                   float sample_rate,
                                   const float freq_in[3],
                                   uint32_t harmonics_count,
                                   float *fund_freq_out,
                                   float harmonics_freq[3],
                                   float harmonics_amp[3],
                                   float beta_out[FIT_MATRIX_DIM])
{
    uint32_t n, k, row, col;
    float omega[FIT_MAX_HARMONICS];
    float freq[FIT_MAX_HARMONICS];
    float beta[FIT_MATRIX_DIM];
    uint32_t dim = FIT_MATRIX_DIM;
    float fund_upp_mV = 0.0f;

    if (harmonics_count < 1U) harmonics_count = 1U;
    if (harmonics_count > FIT_MAX_HARMONICS) harmonics_count = FIT_MAX_HARMONICS;

    /* 用 FFT Top3 峰的实际频率，支持任意次谐波（2/3/4 次等）。
     * 未使用的谐波槽位频率置 0，后续不参与累加。 */
    for (k = 0U; k < FIT_MAX_HARMONICS; ++k) {
        if (k < harmonics_count) {
            freq[k] = freq_in[k];
        } else {
            freq[k] = 0.0f;
        }
        omega[k] = 2.0f * 3.14159265358979f * freq[k] / sample_rate;
    }

    /* 清零法方程矩阵和向量 */
    for (row = 0U; row < dim; ++row) {
        s_fit_vector[row] = 0.0f;
        for (col = 0U; col < dim; ++col) {
            s_fit_matrix[row * dim + col] = 0.0f;
        }
    }

    /* 逐点累加 XᵀX 和 Xᵀx。内部直接从码值转换电压，省 s_voltage 数组。
     * 设计矩阵列顺序：[1, cos1, sin1, cos2, sin2, cos3, sin3]
     * 只累加 harmonics_count 个谐波对应的列，未用列保持 0。 */
    for (n = 0U; n < length; ++n) {
        float phi[FIT_MAX_HARMONICS];
        float cosv[FIT_MAX_HARMONICS];
        float sinv[FIT_MAX_HARMONICS];
        float row_vec[FIT_MATRIX_DIM];
        float x = code_to_input_voltage(remap_ad9226(raw[n]));

        row_vec[0] = 1.0f;
        for (k = 0U; k < FIT_MAX_HARMONICS; ++k) {
            row_vec[1U + 2U * k] = 0.0f;
            row_vec[2U + 2U * k] = 0.0f;
        }
        for (k = 0U; k < harmonics_count; ++k) {
            phi[k] = omega[k] * (float)n;
            cosv[k] = cosf(phi[k]);
            sinv[k] = sinf(phi[k]);
            row_vec[1U + 2U * k] = cosv[k];
            row_vec[2U + 2U * k] = sinv[k];
        }
        /* 累加 XᵀX（对称）和 Xᵀx，只累加实际使用的行/列 */
        for (row = 0U; row < dim; ++row) {
            if (row > 2U * harmonics_count) continue;  /* 跳过未用列 */
            s_fit_vector[row] += row_vec[row] * x;
            for (col = row; col < dim; ++col) {
                if (col > 2U * harmonics_count) continue;
                float prod = row_vec[row] * row_vec[col];
                s_fit_matrix[row * dim + col] += prod;
                if (row != col) {
                    s_fit_matrix[col * dim + row] += prod;
                }
            }
        }
    }

    /* 未使用的对角线补 1，避免矩阵奇异（0×0 子块不可逆） */
    for (k = harmonics_count; k < FIT_MAX_HARMONICS; ++k) {
        s_fit_matrix[(1U + 2U * k) * dim + (1U + 2U * k)] = 1.0f;
        s_fit_matrix[(2U + 2U * k) * dim + (2U + 2U * k)] = 1.0f;
    }

    /* 解法方程 */
    if (solve_linear_system(s_fit_matrix, s_fit_vector, beta, dim) == 0U) {
        /* 拟合失败：清零 beta_out 防止 build_wave 用上一帧旧系数画波
         *（导致"1周期显示3周期"等幽灵波形） */
        if (beta_out) {
            for (k = 0U; k < FIT_MATRIX_DIM; ++k) beta_out[k] = 0.0f;
        }
        if (fund_freq_out) *fund_freq_out = freq_in[0];
        for (k = 0U; k < 3U; ++k) {
            if (harmonics_freq) harmonics_freq[k] = (k < harmonics_count) ? freq[k % FIT_MAX_HARMONICS] : 0.0f;
            if (harmonics_amp) harmonics_amp[k] = 0.0f;
        }
        return 0.0f;
    }

    /* β = [d, a1, b1, a2, b2, a3, b3]
     * 最小二乘模型 x[n] = d + a·cos(ωn) + b·sin(ωn)
     * 单边峰值幅度 Ak = √(ak²+bk²)
     * 注意：不同于 FFT，最小二乘拟合的 a/b 直接是系数，不需要 ×2 单边补偿
     * 未使用的谐波槽位 amp=0，freq 保持传入值或 0。 */
    for (k = 0U; k < FIT_MAX_HARMONICS; ++k) {
        float ak = beta[1U + 2U * k];
        float bk = beta[2U + 2U * k];
        float peak = sqrtf(ak * ak + bk * bk);  /* 单边峰值幅度 */
        if (harmonics_amp) harmonics_amp[k] = (k < harmonics_count) ? peak : 0.0f;
        if (harmonics_freq) harmonics_freq[k] = (k < harmonics_count) ? freq[k] : 0.0f;
    }
    /* 合成波形峰峰值 Upp：用拟合参数数值搜索一个基波周期内的极值。
     * 旧版 Upp = 2×A1（仅基波），对合成信号偏小：
     *   10kHz@80mV + 20kHz@20mV → 旧版 Upp=160mV，实际合成 Upp=176mV，偏小16mV超5mV上限。
     * 新版在一个基波周期内取 4096 点求 max-min，精度 <0.1mV。
     * 单频信号时 max-min = 2×A1，跟旧版一致。 */
    {
        uint32_t fine_n;
        uint32_t fine_points = 4096U;
        float model_max = -1.0e30f;
        float model_min = 1.0e30f;
        float fund_freq_local = freq[0];
        if (fund_freq_local < 1.0f) fund_freq_local = 1000.0f;
        for (fine_n = 0U; fine_n < fine_points; ++fine_n) {
            float t = (float)fine_n / (float)fine_points;  /* 0~1 基波周期 */
            float value = beta[0];  /* DC */
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
    if (fund_freq_out) *fund_freq_out = freq[0];
    /* 输出 beta 供波形重建使用 */
    if (beta_out) {
        for (k = 0U; k < FIT_MATRIX_DIM; ++k) {
            beta_out[k] = beta[k];
        }
    }
    return fund_upp_mV;
}

/* 正弦模型重建波形：用最小二乘拟合的参数直接生成数学级光滑波形。
 * 模型 x(t) = d + Σ_k [ak·cos(2π·k·t) + bk·sin(2π·k·t)]，t 为归一化周期数。
 * 优点：
 *   1. 无 FFT 频谱泄漏/混叠问题，全频段（50k~500k）一致光滑
 *   2. 无需 FFT+IFFT，计算量仅 255×6 次三角函数（<1ms）
 *   3. 波形纯净，直接反映拟合参数，与 Upp 测量一致
 *   4. 不依赖过零检测，无噪声误触发问题 */
static void build_wave(uint8_t *destination, uint8_t periods,
                       const float *beta, const float *freq,
                       uint32_t harmonics_count, float frequency_hz)
{
    uint32_t i, k;
    float minimum = 1.0e30f;
    float maximum = -1.0e30f;
    static float s_values[UI_HMI_WAVE_PLOT_POINTS];
    float dc = beta[0];

    if (frequency_hz < 1.0f) frequency_hz = 1000.0f;

    /* t 从 0 到 periods，对应 periods 个完整基波周期。
     * 由于 ωk·n = 2π·fk/fs · n，而 n = t·fs/f0，
     * 所以 ωk·n = 2π·(fk/f0)·t，与采样率无关。
     * fk/f0 = 谐波次数（支持 2/3/4 次任意谐波，不硬编码 1/2/3）。
     * 旧版硬编码 (k+1) 会导致 30kHz 谐波被画成 20kHz，波形失真。 */
    {
        float fund_freq = freq[0];
        if (fund_freq < 1.0f) fund_freq = 1000.0f;
        for (i = 0U; i < UI_HMI_WAVE_PLOT_POINTS; ++i) {
            float t = (float)i / (float)(UI_HMI_WAVE_PLOT_POINTS - 1U) * (float)periods;
            float value = dc;
            for (k = 0U; k < harmonics_count && k < FIT_MAX_HARMONICS; ++k) {
                float harmonic_ratio = freq[k] / fund_freq;  /* 谐波次数 */
                float phase = 2.0f * 3.14159265358979f * harmonic_ratio * t;
                value += beta[1U + 2U * k] * cosf(phase);
                value += beta[2U + 2U * k] * sinf(phase);
            }
            s_values[i] = value;
            if (value < minimum) minimum = value;
            if (value > maximum) maximum = value;
        }
    }
    for (i = 0U; i < UI_HMI_WAVE_PLOT_POINTS; ++i) {
        destination[i] = scale_to_u8(s_values[i], minimum, maximum);
    }
}

static void build_spectrum(float *maximum_amplitude, float fundamental_freq)
{
    uint32_t point;
    float peak = 1.0e-12f;
    float threshold_abs;
    float threshold_rel;
    float threshold;
    /* 动态频率范围：显示 0 ~ min(10×基波, 2MHz)。
     * 旧版固定 0~2MHz，10kHz 和 20kHz 分别在 point 1 和 2，两峰重合。
     * 新版 10kHz 基波 → 显示 0~100kHz，10k 在 point 25，20k 在 point 51，清晰分离。
     * 500kHz 基波 → 显示 0~2MHz（Nyquist 上限），500k 在 point 64，1M 在 point 128。 */
    float display_max_freq = fundamental_freq * 10.0f;
    float nyquist = (float)G_SIGNAL_SAMPLE_RATE_HZ * 0.5f;
    uint32_t max_bin;
    if (display_max_freq > nyquist) display_max_freq = nyquist;
    if (display_max_freq < 50000.0f) display_max_freq = 50000.0f;  /* 最小 50kHz */
    max_bin = (uint32_t)(display_max_freq / (float)G_SIGNAL_SAMPLE_RATE_HZ *
                         (float)G_SIGNAL_FRAME_SAMPLES);
    if (max_bin < 10U) max_bin = 10U;
    if (max_bin > G_SIGNAL_BIN_COUNT - 1U) max_bin = G_SIGNAL_BIN_COUNT - 1U;

    for (point = 1U; point < max_bin; ++point) {
        if (s_magnitude[point] > peak) peak = s_magnitude[point];
    }
    /* 双门限底噪裁剪（参考方案做法）：
     * - 绝对门限 5mV（题目最小信号 50mVpp 的 1/10，滤除量化噪声底）
     * - 相对门限 peak×1%（-40dB，主峰动态范围）
     * 取较大者，保证小信号下也能看到真实谱线，大信号下底噪全黑。 */
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
        for (bin = first; bin <= last && bin < G_SIGNAL_BIN_COUNT; ++bin) {
            if (s_magnitude[bin] > local) local = s_magnitude[bin];
        }
        /* 底噪裁剪 */
        if (local < threshold) local = threshold;
        /* 对数刻度：0 ~ -40dB 映射到 255 ~ 0 */
        scaled = 20.0f * log10f(local / peak + 1.0e-10f);
        scaled = (scaled + 40.0f) * 255.0f / 40.0f;
        if (scaled < 0.0f) scaled = 0.0f;
        if (scaled > 255.0f) scaled = 255.0f;
        s_spectrum[point] = (uint8_t)(scaled + 0.5f);
    }
    *maximum_amplitude = peak;
}

/* Top3 分量幅值提取：用拟合值 harm_freq/harm_amp 替代 FFT 单 bin 值。
 * 避免 DC 旁瓣伪峰（500kHz 纯正弦下 FFT bin1 附近会读出 18mV 假峰），
 * 拟合值无频谱泄漏、无谐波间干扰，精度 ±0.5mV（A1 基波 ±1.5mV）。
 * 按幅值降序排序输出，频率用精化后的（不是 bin×488Hz）。 */
static void make_top3(ui_spectrum_measurement_t *result,
                      const float harm_freq[3], const float harm_amp[3])
{
    uint32_t i, j;
    float freq_copy[3];
    float amp_copy[3];

    for (i = 0U; i < 3U; ++i) {
        freq_copy[i] = harm_freq[i];
        amp_copy[i]  = harm_amp[i];
    }
    /* 按幅值降序排序（选择排序，基波排第一） */
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
    /* 输出：频率 mHz，幅值 mV（峰值，非峰峰值；A1=Upp/2） */
    for (i = 0U; i < 3U; ++i) {
        float f_hz = freq_copy[i];
        float a_v  = amp_copy[i];
        if (f_hz < 0.0f) f_hz = 0.0f;
        if (a_v  < 0.0f) a_v  = 0.0f;
        result->frequency_mHz[i] = (uint32_t)(f_hz * 1000.0f + 0.5f);
        result->amplitude_mV[i]  = (uint32_t)(a_v  * 1000.0f + 0.5f);
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
    float fit_upp_mV;
    float fit_freq;
    float harm_freq[3];
    float harm_amp[3];
    uint32_t real_peak_count;
    float fit_beta[FIT_MATRIX_DIM];
    float upp_avg, urms_avg, freq_avg;

    /* 1. 预处理：统计 min/max/sum2 用于 Urms 计算（max-min 仅作 fallback）。
     *    不缓存电压数组（省 32KB SRAM），fit 函数内部直接从 raw 转换。 */
    for (i = 0U; i < G_SIGNAL_FRAME_SAMPLES; ++i) {
        float value = code_to_input_voltage(remap_ad9226(s_raw[i + G_SIGNAL_DISCARD_SAMPLES]));
        if (value < minimum) minimum = value;
        if (value > maximum) maximum = value;
        sum2 += value * value;
    }

    /* 2. 加汉宁窗 FFT，用于找基波峰位和频谱显示 */
    coherent_gain = 1.0f;
    (void)sp_window_coherent_gain_f32(SP_WINDOW_HANN,
                                      G_SIGNAL_FRAME_SAMPLES,
                                      &coherent_gain);
    if (coherent_gain <= 0.0f) coherent_gain = 1.0f;
    for (i = 0U; i < G_SIGNAL_FRAME_SAMPLES; ++i) {
        float value = code_to_input_voltage(remap_ad9226(s_raw[i + G_SIGNAL_DISCARD_SAMPLES]));
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

    /* 2.5 找真峰（按幅度降序，最强=基波）。
     *    find_top3_peaks 内部用主峰×5% 门限过滤噪声假峰：
     *      单频信号 → 返回 1 个真峰
     *      双频信号 → 返回 2 个真峰
     *      三频信号 → 返回 3 个真峰
     *    fit 按实际真峰数拟合，不硬塞假峰，彻底避免矩阵奇异。
     *    这解决了单频信号 Upp 偏小的问题：旧版用假峰频率拟合，
     *    最小二乘把能量分配到假频率上，基波系数被分走，Upp 偏小 3~5%。 */
    real_peak_count = find_top3_peaks(harm_freq, harm_amp);
    fundamental.frequency_hz = harm_freq[0];  /* 基波 = 最强峰 */

    /* 3. 频率精化（IEEE 1057，3 参数高斯-牛顿迭代）—— 统一最精密算法。
     *    频率精化是幅值拟合的基础，不分频段，统一用 8192 点全点。
     *    v3-release 验证参数：15 次迭代 + 0.5Hz 门限，精化后频率偏差 < 1Hz。
     *    精化后 8192 点相位累积 < 0.013rad，可忽略，不影响幅值拟合。 */
    fundamental.frequency_hz = refine_frequency(&s_raw[G_SIGNAL_DISCARD_SAMPLES],
                                                G_SIGNAL_FRAME_SAMPLES,
                                                (float)G_SIGNAL_SAMPLE_RATE_HZ,
                                                fundamental.frequency_hz);

    /* 基波用精化值；真谐波 = 精化基波 × 谐波次数（严格倍数）。
     * 谐波次数 = FFT 峰值频率 ÷ 基波 FFT 峰值频率 四舍五入，支持 2/3/4 次任意谐波。 */
    {
        float fund_refined = fundamental.frequency_hz;
        float fund_fft = harm_freq[0];  /* 精化前的基波 FFT 频率 */
        uint32_t k;
        harm_freq[0] = fund_refined;
        for (k = 1U; k < real_peak_count; ++k) {
            float ratio = harm_freq[k] / fund_fft;
            uint32_t harmonic_order = (uint32_t)(ratio + 0.5f);
            if (harmonic_order < 2U) harmonic_order = 2U;
            harm_freq[k] = fund_refined * (float)harmonic_order;
        }
    }

    /* 4. 多正弦最小二乘拟合：分频段参数，逐频段调试。
     *    频率已精化到位（偏差<1Hz），幅值拟合用分频段的 fit_length。
     *    初始全部 8192（v3-release 等效），逐频段调优后固定。 */
    {
        freq_band_t band = get_freq_band(fundamental.frequency_hz);
        uint32_t fit_length = get_band_fit_length(band);
        fit_upp_mV = fit_multisine_and_upp(&s_raw[G_SIGNAL_DISCARD_SAMPLES],
                                           fit_length,
                                           (float)G_SIGNAL_SAMPLE_RATE_HZ,
                                           harm_freq,
                                           real_peak_count,
                                           &fit_freq, harm_freq, harm_amp, fit_beta);
    }

    /* 5. 8 帧环形缓冲平均 + 突变检测。
     *    正常情况下平均 8 帧提升精度（×√8≈2.8）；
     *    当用户切换信号源（幅度突变>25% 或 频率突变>10%）时，
     *    立即清空缓冲区，避免旧帧拖累响应（解决切换后测量值滞后问题）。 */
    {
        float new_upp = (fit_upp_mV > 0.0f) ? fit_upp_mV :
                        (maximum - minimum) * 1000.0f;
        /* Urms 用拟合参数 √(ΣAk²/2) ×1000 剔除 DC 偏置（ADC 失调+运放偏置）。
         * 旧版 √(Σx²/N) 含 DC：64mVpp 实测 29mV（理论 22.6，偏大 28%）；
         * 新版只算交流分量，64mVpp 实测 22.6mV（准确）。
         * harm_amp[k] 是峰值(V)，Urms_V = √((A1²+A2²+A3²)/2)。
         * fit 失败时 fallback 到时域 RMS。 */
        float new_urms;
        if (fit_upp_mV > 0.0f) {
            new_urms = sqrtf((harm_amp[0] * harm_amp[0] +
                              harm_amp[1] * harm_amp[1] +
                              harm_amp[2] * harm_amp[2]) * 0.5f) * 1000.0f;
        } else {
            new_urms = sqrtf(sum2 / (float)G_SIGNAL_FRAME_SAMPLES) * 1000.0f;
        }
        float new_freq = fundamental.frequency_hz;

        /* 突变检测：先算当前历史均值，再判断新帧是否偏离过大 */
        if (s_avg_filled > 0U) {
            float hist_upp_avg = 0.0f;
            float hist_freq_avg = 0.0f;
            uint8_t signal_changed = 0U;
            for (i = 0U; i < s_avg_filled; ++i) {
                hist_upp_avg += s_upp_history[i];
                hist_freq_avg += s_freq_history[i];
            }
            hist_upp_avg /= (float)s_avg_filled;
            hist_freq_avg /= (float)s_avg_filled;
            /* 幅度突变 >25% 或 频率突变 >10%，判定为信号源切换 */
            if (hist_upp_avg > 1.0f &&
                fabsf(new_upp - hist_upp_avg) > 0.25f * hist_upp_avg) {
                signal_changed = 1U;
            }
            if (hist_freq_avg > 100.0f &&
                fabsf(new_freq - hist_freq_avg) > 0.10f * hist_freq_avg) {
                signal_changed = 1U;
            }
            if (signal_changed) {
                /* 清空缓冲，从新信号重新开始累积 */
                s_avg_index = 0U;
                s_avg_filled = 0U;
            }
        }

        s_upp_history[s_avg_index] = new_upp;
        s_urms_history[s_avg_index] = new_urms;
        s_freq_history[s_avg_index] = new_freq;
        s_avg_index = (s_avg_index + 1U) % AVG_FRAME_COUNT;
        if (s_avg_filled < AVG_FRAME_COUNT) s_avg_filled++;
    }

    upp_avg = 0.0f; urms_avg = 0.0f; freq_avg = 0.0f;
    for (i = 0U; i < s_avg_filled; ++i) {
        upp_avg += s_upp_history[i];
        urms_avg += s_urms_history[i];
        freq_avg += s_freq_history[i];
    }
    upp_avg /= (float)s_avg_filled;
    urms_avg /= (float)s_avg_filled;
    freq_avg /= (float)s_avg_filled;

    /* 6. 输出结果：Upp 用拟合值（或 fallback），频率用精化后的值 */
    wave.upp_mV = (uint32_t)(upp_avg + 0.5f);
    wave.urms_mV = (uint32_t)(urms_avg + 0.5f);
    wave.fundamental_mHz = (uint32_t)(freq_avg * 1000.0f + 0.5f);

    /* 7. 波形重建（正弦模型）和频谱图。
     *    用最小二乘拟合的 a/b 系数直接生成波形，数学级光滑，
     *    无 FFT 频谱泄漏/混叠问题，全频段一致。 */
    build_wave(s_wave_one, 1U, fit_beta, harm_freq, real_peak_count, fundamental.frequency_hz);
    build_wave(s_wave_three, 3U, fit_beta, harm_freq, real_peak_count, fundamental.frequency_hz);
    build_spectrum(&ignored_peak, fundamental.frequency_hz);
    make_top3(&spectrum, harm_freq, harm_amp);

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
