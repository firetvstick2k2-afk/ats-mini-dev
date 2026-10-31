/*
 * ATS Mini application descriptor for DBFM 4.2.2.
 *
 * Arduino-ESP32 3.x provides a weak esp_app_desc symbol, so this definition
 * supplies the project identity read by ESP-IDF/DBFM without patching the
 * generated firmware image after linking.
 */
#include <string.h>
#include "esp_app_desc.h"
#include "esp_idf_version.h"

const __attribute__((weak)) __attribute__((section(".rodata_desc")))
esp_app_desc_t esp_app_desc = {
    .magic_word = ESP_APP_DESC_MAGIC_WORD,
    .secure_version = 0,
    .reserv1 = {0, 0},
    .version = "1.31",
    .project_name = "ATS-MINI by 13HN2709",
    .time = __TIME__,
    .date = __DATE__,
    .idf_ver = IDF_VER
};
