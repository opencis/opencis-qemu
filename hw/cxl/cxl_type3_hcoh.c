/*
 * QEMU CXL Host Type2 HCOH Implementation
 *
 * Copyright (c) 2024 EEUM, Inc.
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "qemu/error-report.h"

#include "hw/cxl/cxl.h"
#include "hw/cxl/cxl_hcache.h"
#include "hw/cxl/cxl_type3_hcoh.h"

static HostCoh *hcoh;
static Cache *hcache;
QemuSpin ct2d_lock;

static GRand *rng_opc;
static GRand *rng_addr;
static GRand *rng_size;

static MemTxResult __host_hcoh_access(CacheCommand cmd, PCIDevice *d,
                                      uint64_t haddr, uint64_t *data,
                                      uint32_t size, MemTxAttrs attrs)
{
    CacheState cache_state;
    MemTxResult rsp;
    uint64_t assem_addr, tag, set;
    int32_t cache_blk;
    uint8_t *blk_addr;

    tag = host_cache_extract_tag(hcache, haddr);
    set = host_cache_extract_set(hcache, haddr);

    cache_blk = host_cache_find_valid_block(hcache, tag, set);

    if (cache_blk != -1) {
        if (cmd == CACHE_READ) {
            host_cache_data_read(hcache, haddr, set, cache_blk, data, size);
        } else if (cmd == CACHE_UPDATE) {
            host_cache_data_write(hcache, haddr, set, cache_blk, data, size);
        }
    } else {
        cache_blk = host_cache_find_invalid_block(hcache, set);

        if (cache_blk == -1) {
            cache_blk = host_cache_find_replace_block(hcache, set);
            blk_addr = host_cache_extract_block_addr(hcache, set, cache_blk);

            assem_addr = host_cache_assem_haddr(hcache, set, cache_blk);

            CXL_HCOH_BIAS(
                assem_addr,
                "cache miss -> vitctim write -> haddr: 0x%lx, data: 0x%lx",
                assem_addr, *(uint64_t *)blk_addr);
            host_cache_print_data_block(hcache, set, cache_blk);

            // Write
            rsp = cxl_remote_cxl_mem_write_with_cache(d, haddr, *(uint64_t *)blk_addr, HOST_BLKSIZE, attrs);
            if (rsp != MEMTX_OK) {
                CXL_HCOH_BIAS(haddr, "cache miss -> write error -> haddr: 0x%lx", haddr);
                return MEMTX_ERROR;
            }

            host_cache_update_block_state(hcache, tag, set, cache_blk,
                                            CACHE_EXCLUSIVE);
        }

        CXL_HCOH_BIAS(haddr, "cache miss -> read request -> haddr: 0x%lx",
                      haddr);
        blk_addr = host_cache_extract_block_addr(hcache, set, cache_blk);

        // Read
        rsp = cxl_remote_cxl_mem_read_with_cache(d, haddr, (uint64_t*)blk_addr, HOST_BLKSIZE, attrs);
        if (rsp != MEMTX_OK) {
            CXL_HCOH_BIAS(haddr, "cache miss -> read error -> haddr: 0x%lx", haddr);
            return MEMTX_ERROR;
        }

        CXL_HCOH_BIAS(haddr,
                      "cache miss -> read done -> haddr: 0x%lx, data: 0x%lx",
                      haddr, *(uint64_t *)blk_addr);
        host_cache_print_data_block(hcache, set, cache_blk);

        cache_state = CACHE_EXCLUSIVE;

        host_cache_update_block_state(hcache, tag, set, cache_blk, cache_state);

        if (cmd == CACHE_READ) {
            g_assert((cache_state == CACHE_EXCLUSIVE) ||
                     (cache_state == CACHE_SHARED));
            host_cache_data_read(hcache, haddr, set, cache_blk, data, size);
        } else if (cmd == CACHE_UPDATE) {
            g_assert(cache_state == CACHE_EXCLUSIVE);
            host_cache_data_write(hcache, haddr, set, cache_blk, data, size);
        }
    }

    return MEMTX_OK;
}

static HostCoh *__host_hcoh_init(void)
{
    HostCoh *coh;

    coh = g_new(HostCoh, 1);
    coh->bias_table_size = HOST_BIAS_TABLE_SIZE;
    coh->bias_entry_size = HOST_BIAS_ENTRY_SIZE;
    coh->bias_table = g_new0(uint32_t, coh->bias_table_size);

    coh->bias_table[0] = HOST_BIAS;
    coh->bias_table[1] = DEVICE_BIAS;

    return coh;
}

static void __host_hcoh_free(HostCoh *coh)
{
    g_free(coh->bias_table);
    g_free(coh);
}

MemTxResult cxl_host_type3_hcoh_read(PCIDevice *d, uint64_t haddr,
                                     uint64_t *data, uint32_t size,
                                     MemTxAttrs attrs)
{
    MemTxResult result = MEMTX_OK;
    uint64_t cur_cb_addr = haddr & ~(HOST_BLKSIZE - 1);
    uint64_t next_cb_addr = (haddr + size - 1) & ~(HOST_BLKSIZE - 1);

    qemu_spin_lock(&ct2d_lock);
    // CXL_THREAD("host hcache lock");

    if (cur_cb_addr != next_cb_addr) {
        uint64_t next_data;
        uint32_t cur_cb_size = next_cb_addr - haddr;

        if (MEMTX_OK == __host_hcoh_access(CACHE_READ, d, haddr, data,
                                           cur_cb_size, attrs)) {
            if (MEMTX_OK == __host_hcoh_access(CACHE_READ, d, next_cb_addr,
                                               &next_data, size - cur_cb_size,
                                               attrs)) {
                *data |= (next_data << (cur_cb_size * BITS_PER_BYTE));
                goto out;
            }
        }
        result = MEMTX_ERROR;
        goto out;
    }
    result = __host_hcoh_access(CACHE_READ, d, haddr, data, size, attrs);

out:
    // CXL_THREAD("host hcache unlock");
    qemu_spin_unlock(&ct2d_lock);

    return result;
}

MemTxResult cxl_host_type3_hcoh_write(PCIDevice *d, uint64_t haddr,
                                      uint64_t data, uint32_t size,
                                      MemTxAttrs attrs)
{
    MemTxResult result = MEMTX_OK;
    uint64_t cur_cb_addr = haddr & ~(HOST_BLKSIZE - 1);
    uint64_t next_cb_addr = (haddr + size - 1) & ~(HOST_BLKSIZE - 1);

    qemu_spin_lock(&ct2d_lock);
    // CXL_THREAD("host hcache lock");

    if (cur_cb_addr != next_cb_addr) {
        uint64_t next_data;
        uint32_t cur_cb_size = next_cb_addr - haddr;

        next_data = data >> (cur_cb_size * BITS_PER_BYTE);
        data &= (((uint64_t)1 << (cur_cb_size * BITS_PER_BYTE)) - 1);

        if (MEMTX_OK == __host_hcoh_access(CACHE_UPDATE, d, haddr, &data,
                                           cur_cb_size, attrs)) {
            if (MEMTX_OK == __host_hcoh_access(CACHE_UPDATE, d, next_cb_addr,
                                               &next_data, size - cur_cb_size,
                                               attrs)) {
                goto out;
            }
        }
        result = MEMTX_ERROR;
        goto out;
    }
    result = __host_hcoh_access(CACHE_UPDATE, d, haddr, &data, size, attrs);

out:
    // CXL_THREAD("host hcache unlock");
    qemu_spin_unlock(&ct2d_lock);

    return result;
}

void cxl_host_type3_hcoh_init(PCIDevice *d)
{
    cxl_host_cache_init(&hcache);
    hcoh = __host_hcoh_init();

    rng_opc = g_rand_new();
    rng_addr = g_rand_new();
    rng_size = g_rand_new();

    CXL_DEBUG("ct2 host hcoh realized");
}

void cxl_host_type3_hcoh_release(void)
{
    __host_hcoh_free(hcoh);
    cxl_host_cache_release(&hcache);

    CXL_DEBUG("ct2 host hcoh released");
}
