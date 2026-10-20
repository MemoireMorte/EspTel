#pragma once
#include <cstdint>
struct esp_partition_t {
  char label[17];
  uint32_t size;
};
inline const esp_partition_t* esp_ota_get_running_partition() {
  static esp_partition_t p{"app0", 1966080};
  return &p;
}
inline const esp_partition_t* esp_ota_get_next_update_partition(const esp_partition_t*) {
  static esp_partition_t p{"app1", 1966080};
  return &p;
}
