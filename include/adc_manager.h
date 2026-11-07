#ifndef ADC_MANAGER_H
#define ADC_MANAGER_H

#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Handle buram pengelola ADC. */
typedef struct adc_manager *adc_manager_handle_t;

/**
 * @brief Membuat pengelola dan ADC oneshot unit.
 *
 * Aplikasi memilih ADC Unit satu kali di sini. Gunakan handle yang sama
 * untuk seluruh sensor pada unit tersebut.
 *
 * @param[in] unit_id ADC_UNIT_1 atau ADC_UNIT_2, sesuai GPIO sensor.
 * @param[out] out_manager Menerima handle jika proses berhasil.
 * @return ESP_OK atau kode error ESP-IDF terkait.
 */
esp_err_t adc_manager_create(adc_unit_t unit_id,
                             adc_manager_handle_t *out_manager);

/**
 * @brief Mendaftarkan dan mengonfigurasi GPIO ADC.
 *
 * Pemetaan GPIO ke ADC channel dilakukan sesuai target ESP-IDF. Pemanggilan
 * berulang untuk GPIO yang sama diperbolehkan jika attenuation dan bitwidth
 * sama. Konfigurasi berbeda pada channel yang sudah terdaftar ditolak agar
 * satu sensor tidak diam-diam mengubah konfigurasi sensor lain.
 *
 * @param[in] manager Pengelola ADC.
 * @param[in] gpio_num GPIO yang terhubung ke sumber analog.
 * @param[in] attenuation Attenuation channel.
 * @param[in] bitwidth Lebar ADC yang didukung target.
 * @return ESP_OK atau kode error ESP-IDF terkait.
 */
esp_err_t adc_manager_configure_channel(adc_manager_handle_t manager,
                                        gpio_num_t gpio_num,
                                        adc_atten_t attenuation,
                                        adc_bitwidth_t bitwidth);

/**
 * @brief Membaca nilai raw dari GPIO yang telah dikonfigurasi.
 *
 * Pembacaan diserialisasi di dalam manager sehingga beberapa sensor/task yang
 * berbagi satu unit tidak mengakses ADC oneshot secara bersamaan.
 *
 * @param[in] manager Pengelola ADC.
 * @param[in] gpio_num GPIO yang telah didaftarkan.
 * @param[out] out_raw Menerima kode ADC raw.
 * @return ESP_OK atau kode error ESP-IDF terkait.
 */
esp_err_t adc_manager_read_raw(adc_manager_handle_t manager,
                               gpio_num_t gpio_num,
                               int *out_raw);

/**
 * @brief Menghapus unit ADC dan melepaskan manager.
 *
 * Panggil setelah semua sensor yang menggunakan manager dihentikan dan
 * dilepas. Fungsi ini tidak boleh dipanggil bersamaan dengan operasi manager.
 *
 * @param[in] manager Pengelola ADC yang dibuat dengan adc_manager_create().
 * @return ESP_OK atau kode error ESP-IDF terkait.
 */
esp_err_t adc_manager_deinit(adc_manager_handle_t manager);

#ifdef __cplusplus
}
#endif

#endif
