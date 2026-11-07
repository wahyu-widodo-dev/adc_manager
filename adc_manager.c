#include "adc_manager.h"

#include <stdbool.h>
#include <stdlib.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "adc_manager";

typedef struct adc_manager_channel {
    gpio_num_t gpio_num;
    adc_channel_t channel;
    adc_atten_t attenuation;
    adc_bitwidth_t bitwidth;
    struct adc_manager_channel *next;
} adc_manager_channel_t;

struct adc_manager {
    adc_unit_t unit_id;
    adc_oneshot_unit_handle_t unit;
    adc_manager_channel_t *channels;
    SemaphoreHandle_t lock;
};

esp_err_t adc_manager_create(adc_unit_t unit_id,
                             adc_manager_handle_t *out_manager)
{
    if (out_manager == NULL) {
        ESP_LOGE(TAG, "Handle keluaran tidak valid");
        return ESP_ERR_INVALID_ARG;
    }
    *out_manager = NULL;

    if (unit_id != ADC_UNIT_1 && unit_id != ADC_UNIT_2) {
        ESP_LOGE(TAG, "ADC Unit tidak valid: %d", (int)unit_id);
        return ESP_ERR_INVALID_ARG;
    }

    adc_manager_handle_t manager = calloc(1, sizeof(*manager));
    if (manager == NULL) {
        ESP_LOGE(TAG, "Gagal mengalokasikan manager");
        return ESP_ERR_NO_MEM;
    }

    manager->unit_id = unit_id;
    manager->lock = xSemaphoreCreateMutex();
    if (manager->lock == NULL) {
        ESP_LOGE(TAG, "Gagal membuat mutex manager");
        free(manager);
        return ESP_ERR_NO_MEM;
    }

    const adc_oneshot_unit_init_cfg_t unit_config = {
        .unit_id = unit_id,
    };
    esp_err_t err = adc_oneshot_new_unit(&unit_config, &manager->unit);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Gagal membuat ADC Unit %d: %s",
                 (int)unit_id, esp_err_to_name(err));
        vSemaphoreDelete(manager->lock);
        free(manager);
        return err;
    }

    *out_manager = manager;
    ESP_LOGI(TAG, "ADC Unit %d siap", (int)unit_id);
    return ESP_OK;
}

esp_err_t adc_manager_configure_channel(adc_manager_handle_t manager,
                                        gpio_num_t gpio_num,
                                        adc_atten_t attenuation,
                                        adc_bitwidth_t bitwidth)
{
    if (manager == NULL) {
        ESP_LOGE(TAG, "Handle manager tidak valid");
        return ESP_ERR_INVALID_ARG;
    }

    adc_unit_t mapped_unit;
    adc_channel_t channel;
    esp_err_t err = adc_oneshot_io_to_channel(gpio_num, &mapped_unit, &channel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "GPIO %d bukan input ADC yang valid: %s",
                 (int)gpio_num, esp_err_to_name(err));
        return err;
    }
    if (mapped_unit != manager->unit_id) {
        ESP_LOGE(TAG, "GPIO %d terhubung ke ADC Unit %d, bukan ADC Unit %d",
                 (int)gpio_num, (int)mapped_unit, (int)manager->unit_id);
        return ESP_ERR_INVALID_ARG;
    }

    adc_manager_channel_t *entry = calloc(1, sizeof(*entry));
    if (entry == NULL) {
        ESP_LOGE(TAG, "Gagal mengalokasikan konfigurasi channel");
        return ESP_ERR_NO_MEM;
    }
    entry->gpio_num = gpio_num;
    entry->channel = channel;
    entry->attenuation = attenuation;
    entry->bitwidth = bitwidth;

    xSemaphoreTake(manager->lock, portMAX_DELAY);
    for (adc_manager_channel_t *current = manager->channels;
         current != NULL;
         current = current->next) {
        if (current->channel == channel) {
            bool same_config = current->gpio_num == gpio_num &&
                               current->attenuation == attenuation &&
                               current->bitwidth == bitwidth;
            xSemaphoreGive(manager->lock);
            free(entry);
            if (!same_config) {
                ESP_LOGE(TAG,
                         "ADC Unit %d channel %d sudah terdaftar dengan GPIO atau konfigurasi berbeda",
                         (int)manager->unit_id, (int)channel);
                return ESP_ERR_INVALID_STATE;
            }
            return ESP_OK;
        }
    }

    const adc_oneshot_chan_cfg_t channel_config = {
        .atten = attenuation,
        .bitwidth = bitwidth,
    };
    err = adc_oneshot_config_channel(manager->unit, channel, &channel_config);
    if (err == ESP_OK) {
        entry->next = manager->channels;
        manager->channels = entry;
    }
    xSemaphoreGive(manager->lock);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Gagal mengonfigurasi ADC Unit %d channel %d (GPIO %d): %s",
                 (int)manager->unit_id,
                 (int)channel,
                 (int)gpio_num,
                 esp_err_to_name(err));
        free(entry);
        return err;
    }

    ESP_LOGI(TAG, "GPIO %d terdaftar pada ADC Unit %d channel %d",
             (int)gpio_num, (int)manager->unit_id, (int)channel);
    return ESP_OK;
}

esp_err_t adc_manager_read_raw(adc_manager_handle_t manager,
                               gpio_num_t gpio_num,
                               int *out_raw)
{
    if (manager == NULL || out_raw == NULL) {
        ESP_LOGE(TAG, "Handle manager atau keluaran pembacaan tidak valid");
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(manager->lock, portMAX_DELAY);
    adc_manager_channel_t *entry = manager->channels;
    while (entry != NULL && entry->gpio_num != gpio_num) {
        entry = entry->next;
    }
    if (entry == NULL) {
        xSemaphoreGive(manager->lock);
        ESP_LOGE(TAG, "GPIO %d belum dikonfigurasi", (int)gpio_num);
        return ESP_ERR_INVALID_STATE;
    }

    adc_channel_t channel = entry->channel;
    esp_err_t err = adc_oneshot_read(manager->unit, channel, out_raw);
    xSemaphoreGive(manager->lock);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Pembacaan ADC Unit %d channel %d (GPIO %d) gagal: %s",
                 (int)manager->unit_id,
                 (int)channel,
                 (int)gpio_num,
                 esp_err_to_name(err));
    }
    return err;
}

esp_err_t adc_manager_deinit(adc_manager_handle_t manager)
{
    if (manager == NULL) {
        ESP_LOGE(TAG, "Handle manager tidak valid");
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(manager->lock, portMAX_DELAY);
    esp_err_t err = adc_oneshot_del_unit(manager->unit);
    if (err != ESP_OK) {
        xSemaphoreGive(manager->lock);
        ESP_LOGE(TAG, "Gagal menghapus ADC Unit %d: %s",
                 (int)manager->unit_id, esp_err_to_name(err));
        return err;
    }

    adc_manager_channel_t *entry = manager->channels;
    while (entry != NULL) {
        adc_manager_channel_t *next = entry->next;
        free(entry);
        entry = next;
    }
    xSemaphoreGive(manager->lock);
    vSemaphoreDelete(manager->lock);
    free(manager);
    return ESP_OK;
}
