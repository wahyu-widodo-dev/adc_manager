# Komponen ADC Manager

`adc_manager` mengelola satu ADC oneshot Unit ESP-IDF dan channel-channelnya
untuk beberapa sensor analog. Aplikasi memilih ADC Unit satu kali saat membuat
manager. Setiap sensor kemudian mendaftarkan GPIO yang dipakainya; pemetaan
GPIO ke ADC channel dilakukan otomatis sesuai target chip.
Gunakan satu manager untuk setiap ADC Unit yang dipakai aplikasi.

Komponen ini kompatibel dengan ESP-IDF 5.3.0 atau lebih baru.

## Fungsi

- Membuat dan memiliki satu ADC oneshot Unit.
- Memetakan GPIO ADC ke Unit dan channel berdasarkan target ESP-IDF.
- Mengonfigurasi channel untuk sensor analog.
- Menolak GPIO yang bukan ADC atau tidak termasuk Unit manager.
- Menolak konfigurasi attenuation/bitwidth yang bertentangan pada channel
  yang sama.
- Menserialisasi pembacaan raw yang memakai Unit bersama.
- Menyediakan hasil ADC raw; konversi ke volt atau satuan sensor dilakukan oleh
  aplikasi/komponen sensor.

## Contoh: beberapa input analog

Contoh berikut menggunakan ESP32-S3 dan `ADC_UNIT_1`. Pastikan GPIO yang
dipilih memang pin ADC pada chip dan board Anda; pemetaan pin berbeda antar
target.

```c
#include "adc_manager.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void app_main(void)
{
    adc_manager_handle_t adc = NULL;
    ESP_ERROR_CHECK(adc_manager_create(ADC_UNIT_1, &adc));

    ESP_ERROR_CHECK(adc_manager_configure_channel(
        adc, GPIO_NUM_1, ADC_ATTEN_DB_12, ADC_BITWIDTH_12));
    ESP_ERROR_CHECK(adc_manager_configure_channel(
        adc, GPIO_NUM_2, ADC_ATTEN_DB_12, ADC_BITWIDTH_12));
    ESP_ERROR_CHECK(adc_manager_configure_channel(
        adc, GPIO_NUM_3, ADC_ATTEN_DB_12, ADC_BITWIDTH_12));

    while (true) {
        int raw_1;
        int raw_2;
        int raw_3;
        ESP_ERROR_CHECK(adc_manager_read_raw(adc, GPIO_NUM_1, &raw_1));
        ESP_ERROR_CHECK(adc_manager_read_raw(adc, GPIO_NUM_2, &raw_2));
        ESP_ERROR_CHECK(adc_manager_read_raw(adc, GPIO_NUM_3, &raw_3));

        /* Ubah nilai raw sesuai skala sensor dan target chip. */
        (void)raw_1;
        (void)raw_2;
        (void)raw_3;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

Untuk memakai NTC10K bersama sensor analog lain, daftarkan semua GPIO melalui
manager yang sama. NTC10K juga menggunakan manager itu sehingga aplikasi cukup
membuat ADC Unit sekali.

## Konfigurasi channel

Setiap GPIO perlu didaftarkan sebelum dibaca. Beberapa pemanggil boleh
mendaftarkan GPIO yang sama jika attenuation dan bitwidth-nya sama. Jika ada
konfigurasi berbeda pada channel yang sudah didaftarkan, manager mengembalikan
`ESP_ERR_INVALID_STATE`; manager tidak diam-diam mengubah konfigurasi channel
yang mungkin sedang dipakai sensor lain.

`adc_manager_read_raw()` hanya menerima GPIO yang telah dikonfigurasi.
Pembacaan raw tidak otomatis menjadi volt karena hubungan kode ADC dan tegangan
bergantung pada target, attenuation, dan kalibrasi. Gunakan kalibrasi ADC yang
sesuai jika aplikasi memerlukan konversi tegangan yang lebih akurat.

## Siklus hidup

1. Buat satu manager untuk ADC Unit yang akan dipakai.
2. Daftarkan GPIO tiap sensor dengan konfigurasi channel yang sesuai.
3. Gunakan manager bersama selama semua sensor masih aktif.
4. Hentikan dan lepaskan semua sensor sebelum memanggil `adc_manager_deinit()`.

Manager memiliki ADC Unit yang dibuatnya. Jangan mengonfigurasi atau membaca
Unit tersebut langsung melalui API oneshot ESP-IDF di luar manager. Jangan
memanggil `adc_manager_deinit()` saat ada pembacaan atau sensor yang masih
menggunakan manager.

## Komponen terkait

Komponen [ntc10k](../ntc10k/README.md) menggunakan `adc_manager` untuk
menyiapkan GPIO dan membaca sampel NTC. Jika menerbitkan NTC10K ke ESP
Component Registry, terbitkan `adc_manager` terlebih dahulu agar dependensinya
dapat ditemukan.
