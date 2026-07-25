/**
 * @file ad9226_parallel.c
 * @brief AD9226 parallel capture driver implementation.
 */

#include "ad9226_parallel.h"

#define AD9226_DATA_BITS (12U)
#define AD9226_DATA_MASK (0x0FFFU)

static uint16_t AD9226_Reverse12(uint16_t value)
{
    uint16_t result = 0U;
    uint8_t bit;

    for (bit = 0U; bit < AD9226_DATA_BITS; ++bit)
    {
        if ((value & (uint16_t)(1U << bit)) != 0U)
        {
            result |= (uint16_t)(1U << ((AD9226_DATA_BITS - 1U) - bit));
        }
    }

    return result;
}

HAL_StatusTypeDef AD9226_Init(AD9226_Handle *handle,
                              TIM_HandleTypeDef *timer,
                              uint32_t pwm_channel,
                              GPIO_TypeDef *data_port,
                              uint8_t data_lsb_pin,
                              uint8_t ad0_is_msb,
                              uint16_t *frame_buffer,
                              uint16_t frame_length)
{
    if ((handle == NULL) || (timer == NULL) || (data_port == NULL) ||
        (frame_buffer == NULL) || (frame_length == 0U) ||
        (data_lsb_pin > 4U))
    {
        return HAL_ERROR;
    }

    handle->timer = timer;
    handle->pwm_channel = pwm_channel;
    handle->data_port = data_port;
    handle->data_lsb_pin = data_lsb_pin;
    handle->frame_buffer = frame_buffer;
    handle->frame_length = frame_length;
    handle->ad0_is_msb = (ad0_is_msb != 0U) ? 1U : 0U;
    handle->sample_count = 0U;
    handle->frame_ready = 0U;

    return HAL_OK;
}

HAL_StatusTypeDef AD9226_Start(AD9226_Handle *handle)
{
    if ((handle == NULL) || (handle->timer == NULL))
    {
        return HAL_ERROR;
    }

    handle->sample_count = 0U;
    handle->frame_ready = 0U;

    if (HAL_TIM_PWM_Start(handle->timer, handle->pwm_channel) != HAL_OK)
    {
        return HAL_ERROR;
    }

    if (HAL_TIM_Base_Start_IT(handle->timer) != HAL_OK)
    {
        (void)HAL_TIM_PWM_Stop(handle->timer, handle->pwm_channel);
        return HAL_ERROR;
    }

    return HAL_OK;
}

HAL_StatusTypeDef AD9226_Stop(AD9226_Handle *handle)
{
    HAL_StatusTypeDef status;

    if ((handle == NULL) || (handle->timer == NULL))
    {
        return HAL_ERROR;
    }

    status = HAL_TIM_Base_Stop_IT(handle->timer);
    if (HAL_TIM_PWM_Stop(handle->timer, handle->pwm_channel) != HAL_OK)
    {
        status = HAL_ERROR;
    }

    return status;
}

void AD9226_OnTimerUpdate(AD9226_Handle *handle,
                          TIM_HandleTypeDef *callback_timer)
{
    uint16_t raw_code;

    if ((handle == NULL) || (callback_timer != handle->timer) ||
        (handle->frame_ready != 0U))
    {
        return;
    }

    raw_code = (uint16_t)((handle->data_port->IDR >> handle->data_lsb_pin) &
                          AD9226_DATA_MASK);
    if (handle->ad0_is_msb != 0U)
    {
        raw_code = AD9226_Reverse12(raw_code);
    }

    handle->frame_buffer[handle->sample_count] = raw_code;
    ++handle->sample_count;

    if (handle->sample_count >= handle->frame_length)
    {
        handle->sample_count = 0U;
        handle->frame_ready = 1U;
    }
}

uint8_t AD9226_IsFrameReady(const AD9226_Handle *handle)
{
    return ((handle != NULL) && (handle->frame_ready != 0U)) ? 1U : 0U;
}

void AD9226_ReleaseFrame(AD9226_Handle *handle)
{
    if (handle != NULL)
    {
        handle->frame_ready = 0U;
    }
}

HAL_StatusTypeDef AD9226_ComputeFrameStats(const uint16_t *samples,
                                           uint16_t count,
                                           AD9226_FrameStats *stats)
{
    uint16_t index;
    uint16_t min_code = AD9226_DATA_MASK;
    uint16_t max_code = 0U;
    uint32_t sum = 0U;

    if ((samples == NULL) || (stats == NULL) || (count == 0U))
    {
        return HAL_ERROR;
    }

    for (index = 0U; index < count; ++index)
    {
        sum += samples[index];
        if (samples[index] < min_code)
        {
            min_code = samples[index];
        }
        if (samples[index] > max_code)
        {
            max_code = samples[index];
        }
    }

    stats->min_code = min_code;
    stats->max_code = max_code;
    stats->peak_to_peak = (uint16_t)(max_code - min_code);
    stats->mean_code = (sum + ((uint32_t)count / 2U)) / (uint32_t)count;

    return HAL_OK;
}
