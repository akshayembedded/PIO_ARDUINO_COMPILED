
// 这个文件包含 系统heap和lua heap的默认实现


#include <stdlib.h>
#include <string.h>//add for memset
#include "bget.h"
#include "luat_mem.h"
#include "esp_system.h"
#include "esp_attr.h"
#include "esp_heap_caps.h"

#include "esp_psram.h"
#include "luat_log.h"
#define LUAT_LOG_TAG "vmheap"



//-----------------------------------------------------------------------------
#ifndef LUAT_HEAP_SIZE

#if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32S3)
    #if defined(LUAT_USE_NIMBLE) || defined(LUAT_USE_TLS)
        #if LUAT_CONF_WIFI_AND_BLE
            #define LUAT_HEAP_SIZE (72*1024)
        #else
            #define LUAT_HEAP_SIZE (92*1024)
        #endif
    #else
        #define LUAT_HEAP_SIZE (112*1024)
    #endif
#elif defined(CONFIG_IDF_TARGET_ESP32C2)
    #define LUAT_HEAP_SIZE (68*1024)
#else
    #define LUAT_HEAP_SIZE (64*1024)
#endif

#endif // LUAT_HEAP_SIZE

static uint8_t vmheap[LUAT_HEAP_SIZE];
#if LUAT_USE_MEMORY_OPTIMIZATION_CODE_MMAP
static uint32_t heap_addr_start = (uint32_t) vmheap;
static uint32_t heap_addr_end = (uint32_t) vmheap + LUAT_HEAP_SIZE;
#endif

//------------------------------------------------
//  管理系统内存

void* luat_heap_malloc(size_t len) {
    return malloc(len);
}

void luat_heap_free(void* ptr) {
    free(ptr);
}

void* luat_heap_realloc(void* ptr, size_t len) {
    return realloc(ptr, len);
}

void* luat_heap_calloc(size_t count, size_t _size) {
    return calloc(count, _size);
}

void* luat_heap_zalloc(size_t _size) {
    void *ptr = luat_heap_malloc(_size);
    if (ptr) {
        memset(ptr, 0, _size);
    }
    return ptr;
}

//------------------------------------------------

//------------------------------------------------
// ---------- 管理 LuaVM所使用的内存----------------

#if 1
void* IRAM_ATTR luat_heap_alloc(void *ud, void *ptr, size_t osize, size_t nsize) {
    if (ptr == NULL && nsize == 0)
        return NULL;
#if LUAT_USE_MEMORY_OPTIMIZATION_CODE_MMAP
    if (ptr != NULL && nsize == 0) {
        uint32_t addr = (uint32_t) ptr;
        if (addr < heap_addr_start || addr > heap_addr_end) {
            //LLOGD("skip ROM free %p", ptr);
            return NULL;
        }
    }
#endif

    if (nsize)
    {
    	void* ptmp = bgetr(ptr, nsize);
    	if(ptmp == NULL && osize >= nsize)
        {
            return ptr;
        }
        return ptmp;
    }
    brel(ptr);
    return NULL;
}

void luat_meminfo_luavm(size_t *total, size_t *used, size_t *max_used) {
    long curalloc, totfree, maxfree;
    unsigned long nget, nrel;
    bstats(&curalloc, &totfree, &maxfree, &nget, &nrel);
    *used = curalloc;
    *max_used = bstatsmaxget();
    *total = curalloc + totfree;
}

#else
#include "heap_tlsf.h"
static tlsf_t vm_tlfs;

void* luat_heap_alloc(void *ud, void *ptr, size_t osize, size_t nsize) {
    if (ptr == NULL && nsize == 0)
        return NULL;
#if LUAT_USE_MEMORY_OPTIMIZATION_CODE_MMAP
    if (ptr != NULL && nsize == 0) {
        uint32_t addr = (uint32_t) ptr;
        if (addr < heap_addr_start || addr > heap_addr_end) {
            LLOGD("skip ROM free %p", ptr);
            return NULL;
        }
    }
#endif
    return tlsf_realloc(ptr, nsize);
}
void luat_meminfo_luavm(size_t *total, size_t *used, size_t *max_used) {
    *total = 0;
    *used = 0;
    *max_used = 0;
}
#endif

#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp32-hal-psram.h"
void luat_meminfo_sys(size_t *total, size_t *used, size_t *max_used)
{
    *total = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);
    *used = *total - heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    *max_used = *total - heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT);
}

void luat_heap_init(void)
{
#ifdef LUAT_USE_PSRAM
    size_t t = esp_psram_get_size();
    LLOGD("Found %d kbyte PSRAM", t / 1024);
    size_t psram_sz = 0;
    #define LUAT_HEAP_PSRAM_SIZE (4 * 1024 * 1024)
    if (t > 0)
    {
        psram_sz = t / 2 ; // 默认占一半
        char* ptr = heap_caps_malloc(psram_sz, MALLOC_CAP_SPIRAM);
        if (ptr) {
            LLOGD("Use %d kbyte PSRAM for Lua VM", t / 1024 / 2);
            #if LUAT_USE_MEMORY_OPTIMIZATION_CODE_MMAP
            heap_addr_start = (uint32_t)ptr;
            heap_addr_end = (uint32_t)ptr + psram_sz;
            #endif
            bpool(ptr, psram_sz);
        }
        else {
            LLOGE("PSRAM malloc FAILED, fallback to Non-PSRAM mode");
            bpool(vmheap, LUAT_HEAP_SIZE);
        }
    }
    else
    {
        bpool(vmheap, LUAT_HEAP_SIZE);
    }
#else
    bpool(vmheap, LUAT_HEAP_SIZE);
#endif
    // LLOGD("vm heap range %08X %08X", heap_addr_start, heap_addr_end);
}

void luat_heap_opt_init(LUAT_HEAP_TYPE_E type){
    luat_heap_init();
}

void* luat_heap_opt_malloc(LUAT_HEAP_TYPE_E type,size_t len){
    return luat_heap_malloc(len);
}

void luat_heap_opt_free(LUAT_HEAP_TYPE_E type,void* ptr){
    luat_heap_free(ptr);
}

void* luat_heap_opt_realloc(LUAT_HEAP_TYPE_E type,void* ptr, size_t len){
    return luat_heap_realloc(ptr, len);
}

void* luat_heap_opt_calloc(LUAT_HEAP_TYPE_E type,size_t count, size_t size){
    return luat_heap_opt_zalloc(type,count*size);
}

void* luat_heap_opt_zalloc(LUAT_HEAP_TYPE_E type,size_t size){
    void *ptr = luat_heap_opt_malloc(type,size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

void luat_meminfo_opt_sys(LUAT_HEAP_TYPE_E type,size_t* total, size_t* used, size_t* max_used){
    luat_meminfo_sys(total, used, max_used);
}

