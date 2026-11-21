// SPDX-License-Identifier: GPL-3.0
//
// Strong ESP-IDF application descriptor for the ATS Mini OSPI firmware.
//
// Arduino-ESP32's prebuilt libraries provide a weak descriptor containing
// "arduino-lib-builder". This strong definition replaces it in the final
// application image, allowing the Dual-Boot manager to display our product
// name and version from esp_app_desc_t.

#include <stdint.h>
#include <string_view>

#include "sdkconfig.h"
#include "esp_app_desc.h"

#ifndef IDF_VER
#define IDF_VER "unknown"
#endif

namespace {

constexpr std::string_view MINI_OS_PROJECT_NAME = "Mini OS";
constexpr std::string_view MINI_OS_VERSION = "BETA";

static_assert(MINI_OS_PROJECT_NAME.size() < sizeof(esp_app_desc_t::project_name),
              "ESP app project name must fit in esp_app_desc_t::project_name");
static_assert(MINI_OS_VERSION.size() < sizeof(esp_app_desc_t::version),
              "ESP app version must fit in esp_app_desc_t::version");

constexpr esp_app_desc_t makeMiniOsAppDescription()
{
  esp_app_desc_t desc = {};

  desc.magic_word = ESP_APP_DESC_MAGIC_WORD;
  MINI_OS_PROJECT_NAME.copy(desc.project_name, sizeof(desc.project_name) - 1U);
  MINI_OS_VERSION.copy(desc.version, sizeof(desc.version) - 1U);
  std::string_view(__TIME__).copy(desc.time, sizeof(desc.time) - 1U);
  std::string_view(__DATE__).copy(desc.date, sizeof(desc.date) - 1U);
  std::string_view(IDF_VER).copy(desc.idf_ver, sizeof(desc.idf_ver) - 1U);

  // Preserve the compatibility fields expected by the ESP-IDF bootloader.
  desc.min_efuse_blk_rev_full = CONFIG_ESP_EFUSE_BLOCK_REV_MIN_FULL;
  desc.max_efuse_blk_rev_full = CONFIG_ESP_EFUSE_BLOCK_REV_MAX_FULL;
  desc.mmu_page_size = 31 - __builtin_clz(CONFIG_MMU_PAGE_SIZE);

  // espsecure.py / esptool fills app_elf_sha256 after linking when enabled.
  return desc;
}

} // namespace

// The strong symbol overrides Arduino-ESP32's weak "arduino-lib-builder"
// descriptor. The Dual-Boot manager reads project_name and version here.
extern "C" constinit const __attribute__((section(".rodata_desc"), used))
esp_app_desc_t esp_app_desc = makeMiniOsAppDescription();
