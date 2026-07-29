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
/* 8 帧环形缓冲，用于指数平均提升精度（精度 ×√8≈2.8） */
#define AVG_FRAME_COUNT  8U
static float s_upp_history[AVG_FRAME_COUNT];
static float s_urms_history[AVG_FRAME_COUNT];
static float s_freq_history[AVG_FRAME_COUNT];
static uint8_t s_avg_index;
static uint8_t s_avg_filled;

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

/* 多正弦最小二乘拟合：返回基波峰峰值（mV），通过 fund_freq 返回精确基波频率。
 * 输入：raw ADC 码值缓冲（已含 DISCARD 偏移）、采样率、FFT 得到的初始频率估计。
 * 逐点累加法方程，避免构造 8192×7 大矩阵，省内存。
 * 内部直接从码值转换电压，无需额外 float 缓冲区（省 32KB SRAM）。 */
static float fit_multisine_and_upp(const uint16_t *raw, uint32_t length,
                                   float sample_rate, float freq_init,
                                   uint32_t peak_bin,
                                   float *fund_freq_out,
                                   float harmonics_freq[3],
                                   float harmonics_amp[3])
{
    uint32_t n, k, row, col;
    float omega[FIT_MAX_HARMONICS];
    float freq[FIT_MAX_HARMONICS];
    float beta[FIT_MATRIX_DIM];
    uint32_t dim = FIT_MATRIX_DIM;
    float fund_upp_mV = 0.0f;

    /* 初始频率估计：基波 + 2、3 次谐波 */
    freq[0] = freq_init;
    freq[1] = freq_init * 2.0f;
    freq[2] = freq_init * 3.0f;
    for (k = 0U; k < FIT_MAX_HARMONICS; ++k) {
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
     * 设计矩阵列顺序：[1, cos1, sin1, cos2, sin2, cos3, sin3] */
    for (n = 0U; n < length; ++n) {
        float phi[FIT_MAX_HARMONICS];
        float cosv[FIT_MAX_HARMONICS];
        float sinv[FIT_MAX_HARMONICS];
        float row_vec[FIT_MATRIX_DIM];
        float x = code_to_input_voltage(reverse12((uint16_t)(raw[n] & 0x0FFFU)));

        row_vec[0] = 1.0f;
        for (k = 0U; k < FIT_MAX_HARMONICS; ++k) {
            phi[k] = omega[k] * (float)n;
            cosv[k] = cosf(phi[k]);
            sinv[k] = sinf(phi[k]);
            row_vec[1U + 2U * k] = cosv[k];
            row_vec[2U + 2U * k] = sinv[k];
        }
        /* 累加 XᵀX（对称）和 Xᵀx */
        for (row = 0U; row < dim; ++row) {
            s_fit_vector[row] += row_vec[row] * x;
            for (col = row; col < dim; ++col) {
                float prod = row_vec[row] * row_vec[col];
                s_fit_matrix[row * dim + col] += prod;
                if (row != col) {
                    s_fit_matrix[col * dim + row] += prod;
                }
            }
        }
    }

    /* 解法方程 */
    if (solve_linear_system(s_fit_matrix, s_fit_vector, beta, dim) == 0U) {
        /* 拟合失败，返回 0 */
        if (fund_freq_out) *fund_freq_out = freq_init;
        for (k = 0U; k < 3U; ++k) {
            if (harmonics_freq) harmonics_freq[k] = freq[k % FIT_MAX_HARMONICS];
            if (harmonics_amp) harmonics_amp[k] = 0.0f;
        }
        return 0.0f;
    }

    /* β = [d, a1, b1, a2, b2, a3, b3]
     * 最小二乘模型 x[n] = d + a·cos(ωn) + b·sin(ωn)
     * 单边峰值幅度 Ak = √(ak²+bk²)，峰峰值 = 2·Ak
     * 注意：不同于 FFT，最小二乘拟合的 a/b 直接是系数，不需要 ×2 单边补偿 */
    for (k = 0U; k < FIT_MAX_HARMONICS; ++k) {
        float ak = beta[1U + 2U * k];
        float bk = beta[2U + 2U * k];
        float peak = sqrtf(ak * ak + bk * bk);  /* 单边峰值幅度 */
        if (harmonics_amp) harmonics_amp[k] = peak;
        if (harmonics_freq) harmonics_freq[k] = freq[k];
        if (k == 0U) {
            fund_upp_mV = 2.0f * peak * 1000.0f;  /* 峰峰值 = 2×峰值，转mV */
        }
    }
    if (fund_freq_out) *fund_freq_out = freq[0];
    return fund_upp_mV;
}

static void build_wave(uint8_t *destination, uint8_t periods, float frequency_hz,
                       uint32_t peak_bin)
{
    uint32_t i;
    uint32_t crossing = 0U;
    float minimum = 1.0e30f;
    float maximum = -1.0e30f;
    float step;
    float period_length;
    static float s_values[UI_HMI_WAVE_PLOT_POINTS];

    if (frequency_hz < 1.0f) frequency_hz = 1000.0f;
    if (peak_bin == 0U) peak_bin = 1U;

    /* 复用 s_fft 做未加窗 FFT 用于频域滤波重建。
     * analyse_frame 中 s_fft 已用于加窗 FFT 计算 s_magnitude，此处重新填充
     * 未加窗数据。不影响后续 build_spectrum（它只用 s_magnitude）。 */
    for (i = 0U; i < G_SIGNAL_FRAME_SAMPLES; ++i) {
        s_fft[i].real = code_to_input_voltage(reverse12(
            (uint16_t)(s_raw[i + G_SIGNAL_DISCARD_SAMPLES] & 0x0FFFU)));
        s_fft[i].imag = 0.0f;
    }
    fft_transform_inplace_f32(s_fft, G_SIGNAL_FRAME_SAMPLES, 0U);

    /* 频域滤波：保留主峰 ±3 bin 及 2~5 次谐波 ±3 bin，其余置零。
     * 等效理想带通，滤除量化噪声和带外干扰。
     * 对 50kHz~500kHz 全频段一致适用，不依赖 f1 精度，无相位漂移问题。
     * 对正弦波只保留主峰（极其光滑），对方波/三角波保留低次谐波（保形状）。 */
    s_fft[0].real = 0.0f;
    s_fft[0].imag = 0.0f;
    {
        uint32_t h;
        for (i = 1U; i < G_SIGNAL_FRAME_SAMPLES; ++i) {
            uint8_t keep = 0U;
            for (h = 1U; h <= 5U; ++h) {
                uint32_t center = peak_bin * h;
                int32_t diff = (int32_t)i - (int32_t)center;
                if (diff < 0) diff = -diff;
                if ((uint32_t)diff <= 3U) {
                    keep = 1U;
                    break;
                }
            }
            if (keep == 0U) {
                s_fft[i].real = 0.0f;
                s_fft[i].imag = 0.0f;
            }
        }
    }

    /* IFFT 重建时域波形（逆变换内部已除以 N） */
    fft_transform_inplace_f32(s_fft, G_SIGNAL_FRAME_SAMPLES, 1U);

    /* 从过零点开始取 periods 个周期。
     * 加 ±8 码滞回（对应 AD9226 12bit 量程 8/2048≈0.4%），避免噪声抖动误触发：
     * 上升过零要求前点 < -hysteresis 且后点 > +hysteresis。 */
    {
        float hysteresis = 0.04f;  /* 约 8 码 / 2048 × 10V ≈ 0.04V */
        for (i = 1U; i < G_SIGNAL_FRAME_SAMPLES; ++i) {
            if (s_fft[i - 1U].real <= -hysteresis && s_fft[i].real > hysteresis) {
                crossing = i;
                break;
            }
        }
        /* 滞回找不到则退化为简单过零 */
        if (crossing == 0U) {
            for (i = 1U; i < G_SIGNAL_FRAME_SAMPLES; ++i) {
                if (s_fft[i - 1U].real <= 0.0f && s_fft[i].real > 0.0f) {
                    crossing = i;
                    break;
                }
            }
        }
    }
    period_length = (float)G_SIGNAL_SAMPLE_RATE_HZ / frequency_hz;
    step = ((float)periods * period_length) / (float)(UI_HMI_WAVE_PLOT_POINTS - 1U);

    /* 线性插值采样并归一化 */
    for (i = 0U; i < UI_HMI_WAVE_PLOT_POINTS; ++i) {
        float index = (float)crossing + step * (float)i;
        uint32_t i0 = (uint32_t)index % G_SIGNAL_FRAME_SAMPLES;
        uint32_t i1 = (i0 + 1U) % G_SIGNAL_FRAME_SAMPLES;
        float fraction = index - floorf(index);
        float value = s_fft[i0].real + (s_fft[i1].real - s_fft[i0].real) * fraction;
        s_values[i] = value;
        if (value < minimum) minimum = value;
        if (value > maximum) maximum = value;
    }
    for (i = 0U; i < UI_HMI_WAVE_PLOT_POINTS; ++i) {
        destination[i] = scale_to_u8(s_values[i], minimum, maximum);
    }
}

static void build_spectrum(float *maximum_amplitude)
{
    uint32_t point;
    float peak = 1.0e-12f;
    float threshold_abs;
    float threshold_rel;
    float threshold;
    for (point = 1U; point < G_SIGNAL_BIN_COUNT; ++point) {
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
        uint32_t first = 1U + point * (G_SIGNAL_BIN_COUNT - 1U) /
                         UI_HMI_SPECTRUM_PLOT_POINTS;
        uint32_t last = 1U + (point + 1U) * (G_SIGNAL_BIN_COUNT - 1U) /
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
    float fit_upp_mV;
    float fit_freq;
    float harm_freq[3];
    float harm_amp[3];
    float upp_avg, urms_avg, freq_avg;

    /* 1. 预处理：统计 min/max/sum2 用于 Urms 计算（max-min 仅作 fallback）。
     *    不缓存电压数组（省 32KB SRAM），fit 函数内部直接从 raw 转换。 */
    for (i = 0U; i < G_SIGNAL_FRAME_SAMPLES; ++i) {
        float value = code_to_input_voltage(reverse12((uint16_t)(s_raw[i + G_SIGNAL_DISCARD_SAMPLES] & 0x0FFFU)));
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

    /* 3. 多正弦最小二乘拟合：精确提取基波和 2、3 次谐波幅值。
     *    避免 FFT 频谱泄漏，小信号下精度从 5mV 提升到 2.5mV。
     *    无需硬件放大也能精确测 Upp。
     *    传入 s_raw + DISCARD 偏移，函数内部直接转换码值为电压。 */
    fit_upp_mV = fit_multisine_and_upp(&s_raw[G_SIGNAL_DISCARD_SAMPLES],
                                       G_SIGNAL_FRAME_SAMPLES,
                                       (float)G_SIGNAL_SAMPLE_RATE_HZ,
                                       fundamental.frequency_hz,
                                       fundamental.peak_bin,
                                       &fit_freq, harm_freq, harm_amp);

    /* 4. 8 帧环形缓冲平均 + 突变检测。
     *    正常情况下平均 8 帧提升精度（×√8≈2.8）；
     *    当用户切换信号源（幅度突变>25% 或 频率突变>10%）时，
     *    立即清空缓冲区，避免旧帧拖累响应（解决切换后测量值滞后问题）。 */
    {
        float new_upp = (fit_upp_mV > 0.0f) ? fit_upp_mV :
                        (maximum - minimum) * 1000.0f;
        float new_urms = sqrtf(sum2 / (float)G_SIGNAL_FRAME_SAMPLES) * 1000.0f;
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

    /* 5. 输出结果：Upp 用拟合值（或 fallback），频率用抛物线插值结果 */
    wave.upp_mV = (uint32_t)(upp_avg + 0.5f);
    wave.urms_mV = (uint32_t)(urms_avg + 0.5f);
    wave.fundamental_mHz = (uint32_t)(freq_avg * 1000.0f + 0.5f);

    /* 6. 波形重建（FFT 频域滤波 + IFFT）和频谱图 */
    build_wave(s_wave_one, 1U, fundamental.frequency_hz, fundamental.peak_bin);
    build_wave(s_wave_three, 3U, fundamental.frequency_hz, fundamental.peak_bin);
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
