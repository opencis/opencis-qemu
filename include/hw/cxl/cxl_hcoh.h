/*
 * QEMU CXL Host HCOH Configuration
 *
 * Copyright (c) 2025 EEUM, Inc.
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#ifndef CXL_HCOH_H
#define CXL_HCOH_H

#include "trace/trace-hw_cxl.h"

#define CFMWS_BASE_ADDR (0x490000000)
#define HOST_BIAS_TABLE_SIZE (2)
#define HOST_BIAS_ENTRY_SIZE (0x8000000)

typedef struct {
    uint32_t *bias_table;
    uint32_t bias_table_size;
    uint32_t bias_entry_size;
} HostCoh;

typedef enum {
    MEM_Read_MemInv = 0,
    MEM_NDR_MemInv,
    MEM_NDR_MemShared,
    MEM_NDR_HCacheInv,
    MEM_NDR_SpecRd,
    MEM_NDR_ClnEvct,
} MemCommand;

#define CXL_HCOH_BIAS(addr, str, ...) do { \
    char buf[4096]; \
    snprintf(buf, sizeof(buf), "addr: 0x%lx" str, addr, __VA_ARGS__); \
    trace_cxl_hcoh_bias(__func__, __LINE__, buf); \
} while (0)

#endif
