#include "luat_base.h"
#include "luat_ota.h"

#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_log.h"
#include "esp_err.h"
#ifdef LUAT_SPIFFS
#include "esp_spiffs.h"
#elif defined(LUAT_LITTLEFS)
#include "esp_littlefs.h"
#endif

#include "esp_system.h"
#include "esp_partition.h"

extern const esp_partition_t * luadb_partition;

int luat_ota_exec(void) {
    if (luadb_partition == NULL) {
        return -1;
    }
    return luat_ota(luadb_partition->address);
}
