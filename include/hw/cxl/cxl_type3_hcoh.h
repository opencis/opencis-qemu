/*
 * QEMU CXL Host Type3 HCOH Configuration
 *
 * Copyright (c) 2025 EEUM, Inc.
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#ifndef CXL_TYPE3_HCOH_H
#define CXL_TYPE3_HCOH_H

#include "hw/cxl/cxl_hcoh.h"

BiasState cxl_host_type3_hcoh_bias_lookup(uint64_t haddr);
MemTxResult cxl_host_type3_hcoh_read(PCIDevice *d, uint64_t haddr,
                                     uint64_t *data, uint32_t size,
                                     MemTxAttrs attrs);
MemTxResult cxl_host_type3_hcoh_write(PCIDevice *d, uint64_t haddr,
                                      uint64_t data, uint32_t size,
                                      MemTxAttrs attrs);

void cxl_host_type3_hcoh_init(PCIDevice *d);
void cxl_host_type3_hcoh_release(void);

#endif
