/*
 * SPDX-FileCopyrightText: 2020-2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <string.h>
#include "esp_types.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/adc.h"
#include "hal/adc_types.h"
#include "esp_adc_cal.h"
#include "esp_adc_cal_internal.h"

#define ADC_CAL_CHECK(cond, ret) ({                                         \
            if(!(cond)){                                                    \
                return ret;                                                 \
            }                                                               \
})

const __attribute__((unused)) static char *TAG = "ADC_CALI";

esp_err_t esp_adc_cal_get_voltage(adc_channel_t channel,
                                  const esp_adc_cal_characteristics_t *chars,
                                  uint32_t *voltage)
{
    // Check parameters
    ADC_CAL_CHECK(chars != NULL, ESP_ERR_INVALID_ARG);
    ADC_CAL_CHECK(voltage != NULL, ESP_ERR_INVALID_ARG);

    esp_err_t ret = ESP_OK;
    int adc_reading;
    if (chars->adc_num == ADC_UNIT_1) {
        ADC_CAL_CHECK(channel < SOC_ADC_CHANNEL_NUM(0), ESP_ERR_INVALID_ARG);
        adc_reading = adc1_get_raw(channel);
    } else {
        ADC_CAL_CHECK(channel < SOC_ADC_CHANNEL_NUM(1), ESP_ERR_INVALID_ARG);
        ret = adc2_get_raw(channel, chars->bit_width, &adc_reading);
    }

    if (ret == ESP_OK) {
        *voltage = esp_adc_cal_raw_to_voltage((uint32_t)adc_reading, chars);
    }
    return ret;
}

#if ESP_ADC_CAL_CURVE_FITTING_SUPPORTED
/*------------------------------------------------------------------------------
 * Private API
 *----------------------------------------------------------------------------*/
int32_t esp_adc_cal_get_reading_error(const esp_adc_error_calc_param_t *param, uint8_t atten)
{
    if (param->v_cali_input == 0) {
        return 0;
    }

    uint64_t v_cali_1 = param->v_cali_input;
    uint8_t term_num = param->term_num;
    int32_t error = 0;
    uint64_t coeff = 0;
    uint64_t variable[term_num];
    uint64_t term[term_num];
    memset(variable, 0, term_num * sizeof(uint64_t));
    memset(term, 0, term_num * sizeof(uint64_t));

    /**
     * For atten0 ~ 2:
     * error = (K0 * X^0) + (K1 * X^1) + (K2 * X^2);
     *
     * For atten3:
     * error = (K0 * X^0) + (K1 * X^1)  + (K2 * X^2) + (K3 * X^3) + (K4 * X^4);
     */
    variable[0] = 1;
    coeff = (*param->coeff)[atten][0][0];
    term[0] = variable[0] * coeff / (*param->coeff)[atten][0][1];
    error = (int32_t)term[0] * (*param->sign)[atten][0];

    for (int i = 1; i < term_num; i++) {
        variable[i] = variable[i-1] * v_cali_1;
        coeff = (*param->coeff)[atten][i][0];
        term[i] = variable[i] * coeff;
        ESP_LOGV(TAG, "big coef is %llu, big term%d is %llu, coef_id is %d", coeff, i, term[i], i);

        term[i] = term[i] / (*param->coeff)[atten][i][1];
        error += (int32_t)term[i] * (*param->sign)[atten][i];
        ESP_LOGV(TAG, "term%d is %llu, error is %d", i, term[i], error);
    }

    return error;
}
#endif  //#if ESP_ADC_CAL_CURVE_FITTING_SUPPORTED
