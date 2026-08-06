/*
 * BSD LICENSE
 *
 * Copyright(c) 2025-2026 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "mmio_dump.h"

#include "cap.h"
#include "common.h"
#include "log.h"
#include "mmio_common.h"
#include "pqos.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define MMIO_DUMP_LINE_LEN   512
#define BYTE_ELEMS_PER_LINE  16
#define QWORD_ELEMS_PER_LINE 2

/* Map between ACPI structures and appropriate MMIO spaces */
static const struct pqos_mmio_dump_space_map_entry mmio_dump_space_map[] = {
    {MMIO_DUMP_SPACE_CMRC, DM_TYPE_CPU, "CMRC",
     OFFSET_IN_STRUCT(struct pqos_cpu_agent_info, cmrc),
     OFFSET_IN_STRUCT(struct pqos_erdt_cmrc, block_base_addr),
     OFFSET_IN_STRUCT(struct pqos_erdt_cmrc, block_size), PAGE_SIZE},
    {MMIO_DUMP_SPACE_MMRC, DM_TYPE_CPU, "MMRC",
     OFFSET_IN_STRUCT(struct pqos_cpu_agent_info, mmrc),
     OFFSET_IN_STRUCT(struct pqos_erdt_mmrc, reg_block_base_addr),
     OFFSET_IN_STRUCT(struct pqos_erdt_mmrc, reg_block_size), PAGE_SIZE},
    {MMIO_DUMP_SPACE_MARC_OPT, DM_TYPE_CPU, "MARC(OPT)",
     OFFSET_IN_STRUCT(struct pqos_cpu_agent_info, marc),
     OFFSET_IN_STRUCT(struct pqos_erdt_marc, opt_bw_reg_block_base_addr),
     OFFSET_IN_STRUCT(struct pqos_erdt_marc, reg_block_size), PAGE_SIZE},
    {MMIO_DUMP_SPACE_MARC_MIN, DM_TYPE_CPU, "MARC(MIN)",
     OFFSET_IN_STRUCT(struct pqos_cpu_agent_info, marc),
     OFFSET_IN_STRUCT(struct pqos_erdt_marc, min_bw_reg_block_base_addr),
     OFFSET_IN_STRUCT(struct pqos_erdt_marc, reg_block_size), PAGE_SIZE},
    {MMIO_DUMP_SPACE_MARC_MAX, DM_TYPE_CPU, "MARC(MAX)",
     OFFSET_IN_STRUCT(struct pqos_cpu_agent_info, marc),
     OFFSET_IN_STRUCT(struct pqos_erdt_marc, max_bw_reg_block_base_addr),
     OFFSET_IN_STRUCT(struct pqos_erdt_marc, reg_block_size), PAGE_SIZE},
    {MMIO_DUMP_SPACE_CMRD, DM_TYPE_DEVICE, "CMRD",
     OFFSET_IN_STRUCT(struct pqos_device_agent_info, cmrd),
     OFFSET_IN_STRUCT(struct pqos_erdt_cmrd, reg_base_addr),
     OFFSET_IN_STRUCT(struct pqos_erdt_cmrd, reg_block_size), PAGE_SIZE},
    {MMIO_DUMP_SPACE_IBRD, DM_TYPE_DEVICE, "IBRD",
     OFFSET_IN_STRUCT(struct pqos_device_agent_info, ibrd),
     OFFSET_IN_STRUCT(struct pqos_erdt_ibrd, reg_base_addr),
     OFFSET_IN_STRUCT(struct pqos_erdt_ibrd, reg_block_size), PAGE_SIZE},
    {MMIO_DUMP_SPACE_CARD, DM_TYPE_DEVICE, "CARD",
     OFFSET_IN_STRUCT(struct pqos_device_agent_info, card),
     OFFSET_IN_STRUCT(struct pqos_erdt_card, reg_base_addr),
     OFFSET_IN_STRUCT(struct pqos_erdt_card, reg_block_size), PAGE_SIZE}};

/**
 * @brief Hex dump output helpers
 **/

/**
 *  @brief Print a hex dump
 *
 *  @param [in] buf buffer to dump
 *  @param [in] length number of elements to dump
 *  @param [in] width_bytes element width in bytes (1 or 8)
 *  @param [in] le 1 for little-endian, 0 for big-endian
 *  @param [in] binary 1 for binary output, 0 for hexadecimal output
 **/
static void
print_hex_dump(const uint8_t *buf,
               size_t length,
               unsigned width_bytes,
               int le,
               int binary)
{
        size_t i, j, k;
        uint8_t current_byte;
        char line[MMIO_DUMP_LINE_LEN];
        size_t elems_per_line =
            (width_bytes == 8) ? QWORD_ELEMS_PER_LINE : BYTE_ELEMS_PER_LINE;
        /* ceil(length / elems_per_line) so we also print the remainder */
        size_t line_num = (length + elems_per_line - 1) / elems_per_line;

        LOG_DEBUG(
            "%s: Addr: %p. Dumping %zu elements as %s with width %u bytes\n",
            __func__, buf, length, le ? "little-endian" : "big-endian",
            width_bytes);

        for (i = 0; i < line_num; i++) {
                const size_t elems_done = i * elems_per_line;
                const size_t elems_left = length - elems_done;
                const size_t elems_this_line =
                    (elems_left > elems_per_line) ? elems_per_line : elems_left;

                /* byte offset from start of mapped buffer */
                const size_t offset = elems_done * width_bytes;

                LOG_DEBUG("offset: 0x%zx\n", offset);
                snprintf(line, sizeof(line), "%06zx ", offset);

                for (j = 0; j < elems_this_line; j++) {
                        LOG_DEBUG("elem_idx: %zu\n", j);
                        LOG_DEBUG(
                            "elem VA: %p\n",
                            (const void *)(buf + offset + j * width_bytes));

                        if (width_bytes == 8) {
                                uint64_t v;

                                memcpy(&v, buf + offset + j * width_bytes,
                                       sizeof(v));
                                LOG_DEBUG("elem value: %#" PRIx64 "\n", v);
                        }

                        for (k = 0; k < width_bytes; k++) {
                                current_byte =
                                    (le) ? buf[offset + j * width_bytes + k]
                                         : buf[offset + (j + 1) * width_bytes -
                                               k - 1];
                                LOG_DEBUG(
                                    "elem_byte.offset: %02zx. value: %02x\n",
                                    (le) ? (offset + j * width_bytes + k)
                                         : (offset + (j + 1) * width_bytes - k -
                                            1),
                                    (unsigned int)current_byte);
                                if (binary) {
                                        for (int b = 7; b >= 0; --b)
                                                snprintf(
                                                    line + strlen(line),
                                                    sizeof(line) - strlen(line),
                                                    "%c",
                                                    (current_byte & (1 << b))
                                                        ? '1'
                                                        : '0');
                                } else {
                                        snprintf(line + strlen(line),
                                                 sizeof(line) - strlen(line),
                                                 "%02x", current_byte);
                                }
                        }
                        snprintf(line + strlen(line),
                                 sizeof(line) - strlen(line), " ");
                }
                printf("%s\n", line);
        }
}

/**
 * @brief Dump a single address range
 *
 * @param [in] base address
 * @param [in] size size in bytes
 * @param [in] offset offset in elements from base
 * @param [in] length length in elements
 * @param [in] width_bytes element width in bytes (1 or 8)
 * @param [in] le 1 for little-endian, 0 for big-endian
 * @param [in] binary 1 for binary output, 0 for hexadecimal output
 *
 * @return Operation status
 **/
int
dump_mmio_range(uint64_t base,
                uint64_t size,
                unsigned long offset,
                unsigned long length,
                unsigned width_bytes,
                int le,
                int binary)
{
        uint64_t offset_bytes;
        uint64_t length_bytes;
        uint64_t available;
        void *map;
        const uint8_t *buf;

        LOG_INFO("%s: base=%#" PRIx64 " size=%#" PRIx64 " offset=%lu len=%lu "
                 "width(bytes)=%u le=%d bin=%d\n",
                 __func__, base, size, offset, length, width_bytes, le, binary);

        if (width_bytes != 1 && width_bytes != 8) {
                LOG_ERROR("Unsupported MMIO dump width %u\n", width_bytes);
                return PQOS_RETVAL_PARAM;
        }

        available = size / width_bytes;
        if (offset >= available) {
                LOG_ERROR("MMIO dump offset %lu is outside %" PRIu64
                          " available elements\n",
                          offset, available);
                return PQOS_RETVAL_PARAM;
        }

        available -= offset;
        if (length == 0)
                length = available;
        if (length == 0 || length > available) {
                LOG_ERROR("MMIO dump length %lu exceeds %" PRIu64
                          " available elements\n",
                          length, available);
                return PQOS_RETVAL_PARAM;
        }

        offset_bytes = (uint64_t)offset * width_bytes;
        length_bytes = (uint64_t)length * width_bytes;
        if (base > UINT64_MAX - offset_bytes) {
                LOG_ERROR("MMIO dump address overflows\n");
                return PQOS_RETVAL_PARAM;
        }

        map = pqos_mmap_write(base + offset_bytes, length_bytes);

        LOG_DEBUG("%s: map=0x%p\n", __func__, map);

        if (map == NULL)
                return PQOS_RETVAL_ERROR;

        buf = (const uint8_t *)map;

        LOG_DEBUG("%s: map = 0x%p, buf=0x%p\n", __func__, map, buf);

        print_hex_dump(buf, length, width_bytes, le, binary);

        pqos_munmap(map, length_bytes);

        return PQOS_RETVAL_OK;
}

/**
 * @brief Dump a single MMIO space structure
 *
 * @param [in] space_struct pointer to the MMIO space structure
 * @param [in] space_type type of the MMIO space
 * @param [in] dump dump configuration
 *
 * @return Operation status
 **/
static int
mmio_dump_space(const void *space_struct,
                enum pqos_mmio_dump_space space_type,
                const struct pqos_mmio_dump *dump)
{
        uint64_t base = 0;
        uint64_t size = 0;

        switch (space_type) {
        case MMIO_DUMP_SPACE_CMRC: {
                const struct pqos_erdt_cmrc *s =
                    (const struct pqos_erdt_cmrc *)space_struct;
                base = s->block_base_addr;
                size = (uint64_t)s->block_size * PAGE_SIZE;
                break;
        }
        case MMIO_DUMP_SPACE_MMRC: {
                const struct pqos_erdt_mmrc *s =
                    (const struct pqos_erdt_mmrc *)space_struct;
                base = s->reg_block_base_addr;
                size = (uint64_t)s->reg_block_size * PAGE_SIZE;
                break;
        }
        case MMIO_DUMP_SPACE_MARC_OPT: {
                const struct pqos_erdt_marc *s =
                    (const struct pqos_erdt_marc *)space_struct;
                base = s->opt_bw_reg_block_base_addr;
                size = (uint64_t)s->reg_block_size * PAGE_SIZE;
                break;
        }
        case MMIO_DUMP_SPACE_MARC_MIN: {
                const struct pqos_erdt_marc *s =
                    (const struct pqos_erdt_marc *)space_struct;
                base = s->min_bw_reg_block_base_addr;
                size = (uint64_t)s->reg_block_size * PAGE_SIZE;
                break;
        }
        case MMIO_DUMP_SPACE_MARC_MAX: {
                const struct pqos_erdt_marc *s =
                    (const struct pqos_erdt_marc *)space_struct;
                base = s->max_bw_reg_block_base_addr;
                size = (uint64_t)s->reg_block_size * PAGE_SIZE;
                break;
        }
        case MMIO_DUMP_SPACE_CMRD: {
                const struct pqos_erdt_cmrd *s =
                    (const struct pqos_erdt_cmrd *)space_struct;
                base = s->reg_base_addr;
                size = (uint64_t)s->reg_block_size * PAGE_SIZE;
                break;
        }
        case MMIO_DUMP_SPACE_IBRD: {
                const struct pqos_erdt_ibrd *s =
                    (const struct pqos_erdt_ibrd *)space_struct;
                base = s->reg_base_addr;
                size = (uint64_t)s->reg_block_size * PAGE_SIZE;
                break;
        }
        case MMIO_DUMP_SPACE_CARD: {
                const struct pqos_erdt_card *s =
                    (const struct pqos_erdt_card *)space_struct;
                base = s->reg_base_addr;
                size = (uint64_t)s->reg_block_size * PAGE_SIZE;
                break;
        }
        default:
                return PQOS_RETVAL_PARAM;
        }

        unsigned width_bytes =
            (dump->fmt.width == MMIO_DUMP_WIDTH_8BIT) ? 1 : 8;
        int le = dump->fmt.le;
        int binary = dump->fmt.bin;
        unsigned long offset = dump->view.offset;
        unsigned long length = dump->view.length;

        printf("MMIO space dump: base=%#" PRIx64 " size=%#" PRIx64
               " offset=%lu len=%lu width(bytes)=%u le=%d bin=%d\n",
               base, size, offset, length, width_bytes, le, binary);

        return dump_mmio_range(base, size, offset, length, width_bytes, le,
                               binary);
}

int
mmio_dump(const struct pqos_mmio_dump *dump_cfg)
{
        int ret = PQOS_RETVAL_OK;
        const uint16_t *curr_domain;
        const struct pqos_erdt_info *erdt;

        if (dump_cfg == NULL)
                return PQOS_RETVAL_PARAM;

        erdt = _pqos_get_erdt();
        if (erdt == NULL)
                return PQOS_RETVAL_PARAM;

        if (dump_cfg->topology.num_domain_ids == 0 ||
            dump_cfg->topology.domain_ids == NULL ||
            dump_cfg->topology.space < MMIO_DUMP_SPACE_CMRC ||
            dump_cfg->topology.space >= MMIO_DUMP_SPACE_ERROR ||
            (dump_cfg->fmt.width != MMIO_DUMP_WIDTH_64BIT &&
             dump_cfg->fmt.width != MMIO_DUMP_WIDTH_8BIT))
                return PQOS_RETVAL_PARAM;

        curr_domain = dump_cfg->topology.domain_ids;

        for (uint32_t idx = 0; idx < dump_cfg->topology.num_domain_ids; idx++) {
                const int cpu_idx =
                    get_cpu_agent_idx_by_domain_id(erdt, curr_domain[idx]);
                const int dev_idx =
                    get_dev_agent_idx_by_domain_id(erdt, curr_domain[idx]);
                const struct pqos_cpu_agent_info *cpu_agent =
                    cpu_idx >= 0 ? &erdt->cpu_agents[cpu_idx] : NULL;
                const struct pqos_device_agent_info *dev_agent =
                    dev_idx >= 0 ? &erdt->dev_agents[dev_idx] : NULL;

                if (cpu_agent == NULL && dev_agent == NULL) {
                        LOG_ERROR("Domain ID %d is unavailable\n",
                                  curr_domain[idx]);
                        return PQOS_RETVAL_ERROR;
                }

                if (cpu_agent && dev_agent) {
                        LOG_ERROR("Duplicate Domain ID %d is available. "
                                  "Wrong ERDT ACPI table\n",
                                  curr_domain[idx]);
                        return PQOS_RETVAL_ERROR;
                }

                /* Check RMDD Sub-structure is available in Domain CPU */
                if (cpu_agent &&
                    dump_cfg->topology.space != MMIO_DUMP_SPACE_CMRC &&
                    dump_cfg->topology.space != MMIO_DUMP_SPACE_MMRC &&
                    dump_cfg->topology.space != MMIO_DUMP_SPACE_MARC_OPT &&
                    dump_cfg->topology.space != MMIO_DUMP_SPACE_MARC_MIN &&
                    dump_cfg->topology.space != MMIO_DUMP_SPACE_MARC_MAX) {
                        LOG_ERROR("Requested MMIO Reg space is not "
                                  "available in Domain ID %d!\n",
                                  curr_domain[idx]);
                        return PQOS_RETVAL_ERROR;
                }

                /* Check RMDD Sub-structure is available in Domain Device */
                if (dev_agent &&
                    dump_cfg->topology.space != MMIO_DUMP_SPACE_CMRD &&
                    dump_cfg->topology.space != MMIO_DUMP_SPACE_IBRD &&
                    dump_cfg->topology.space != MMIO_DUMP_SPACE_CARD) {
                        LOG_ERROR("Requested MMIO Reg space is not "
                                  "available in Domain ID %d!\n",
                                  curr_domain[idx]);
                        return PQOS_RETVAL_ERROR;
                }

                /* Dump CPU agents' MMIO Registers */
                for (size_t m = 0;
                     cpu_agent && (m < (sizeof(mmio_dump_space_map) /
                                        sizeof(mmio_dump_space_map[0])));
                     ++m) {
                        if (mmio_dump_space_map[m].domain_type != DM_TYPE_CPU)
                                continue;
                        if (mmio_dump_space_map[m].space !=
                            dump_cfg->topology.space)
                                continue;
                        const void *space_struct =
                            (const char *)cpu_agent +
                            mmio_dump_space_map[m].struct_offset;
                        ret = mmio_dump_space(space_struct,
                                              mmio_dump_space_map[m].space,
                                              dump_cfg);
                        if (ret != PQOS_RETVAL_OK)
                                return ret;
                }

                /* Dump Device agents' MMIO Registers */
                for (size_t m = 0;
                     dev_agent && (m < (sizeof(mmio_dump_space_map) /
                                        sizeof(mmio_dump_space_map[0])));
                     ++m) {
                        if (mmio_dump_space_map[m].domain_type !=
                            DM_TYPE_DEVICE)
                                continue;
                        if (mmio_dump_space_map[m].space !=
                            dump_cfg->topology.space)
                                continue;
                        const void *space_struct =
                            (const char *)dev_agent +
                            mmio_dump_space_map[m].struct_offset;
                        ret = mmio_dump_space(space_struct,
                                              mmio_dump_space_map[m].space,
                                              dump_cfg);
                        if (ret != PQOS_RETVAL_OK)
                                return ret;
                }
        }

        return PQOS_RETVAL_OK;
}
