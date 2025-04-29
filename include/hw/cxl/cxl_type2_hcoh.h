/*
 * QEMU CXL Host Type2 HCOH Configuration
 *
 * Copyright (c) 2024 EEUM, Inc.
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#ifndef CXL_TYPE2_HCOH_H
#define CXL_TYPE2_HCOH_H

#include "hw/cxl/cxl_hcoh.h"

BiasState cxl_host_type2_hcoh_bias_lookup(uint64_t haddr);
MemTxResult cxl_host_type2_hcoh_read(PCIDevice *d, uint64_t haddr,
                                     uint64_t *data, uint32_t size,
                                     MemTxAttrs attrs);
MemTxResult cxl_host_type2_hcoh_write(PCIDevice *d, uint64_t haddr,
                                      uint64_t data, uint32_t size,
                                      MemTxAttrs attrs);
MemTxResult cxl_host_type2_hcoh_command(PCIDevice *d, uint64_t haddr,
                                        uint8_t *buf, MemTxAttrs attrs);
M2SRsp_BIRsp cxl_host_type2_hcoh_response(CXLMemReq request, MemTxAttrs attrs);

void cxl_host_type2_hcoh_init(PCIDevice *d);
void cxl_host_type2_hcoh_release(void);

#endif
