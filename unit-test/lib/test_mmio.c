/*
 * BSD LICENSE
 *
 * Copyright(c) 2026 Intel Corporation. All rights reserved.
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
 */

#include "mmio.h"
#include "mmio_allocation.h"
#include "mmio_monitoring.h"
#include "monitoring.h"
#include "test.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

uint8_t *
__wrap_pqos_mmap_read(uint64_t address, const uint64_t size)
{
        check_expected(address);
        check_expected(size);

        return mock_ptr_type(uint8_t *);
}

void
__wrap_pqos_munmap(void *mem, const uint64_t size)
{
        check_expected_ptr(mem);
        check_expected(size);
}

const struct pqos_erdt_info *
__wrap__pqos_get_erdt(void)
{
        return mock_ptr_type(const struct pqos_erdt_info *);
}

const struct pqos_mrrm_info *
__wrap__pqos_get_mrrm(void)
{
        return mock_ptr_type(const struct pqos_mrrm_info *);
}

const struct pqos_channels_domains *
__wrap__pqos_get_channels_domains(void)
{
        return mock_ptr_type(const struct pqos_channels_domains *);
}

int
__wrap_get_total_iol3_mbm_rmid_range_v1(const struct pqos_erdt_ibrd *ibrd,
                                        unsigned int rmid_first,
                                        unsigned int rmid_last,
                                        iol3_mbm_rmid_t *rmids_val)
{
        check_expected_ptr(ibrd);
        check_expected(rmid_first);
        check_expected(rmid_last);

        *rmids_val = TOTAL_IO_BW_RMID_O_MASK;
        return mock_type(int);
}

int
__wrap_get_miss_iol3_mbm_rmid_range_v1(const struct pqos_erdt_ibrd *ibrd,
                                       unsigned int rmid_first,
                                       unsigned int rmid_last,
                                       iol3_mbm_rmid_t *rmids_val)
{
        check_expected_ptr(ibrd);
        check_expected(rmid_first);
        check_expected(rmid_last);

        *rmids_val = TOTAL_IO_BW_RMID_O_MASK;
        return mock_type(int);
}

int
__wrap_get_mba_optimal_bw_region_clos_v1(const struct pqos_erdt_marc *marc,
                                         int region_num,
                                         unsigned int clos_number,
                                         unsigned int *value)
{
        check_expected_ptr(marc);
        check_expected(region_num);
        check_expected(clos_number);

        *value = 0;
        return PQOS_RETVAL_OK;
}

int
__wrap_get_mba_min_bw_region_clos_v1(const struct pqos_erdt_marc *marc,
                                     int region_num,
                                     unsigned int clos_number,
                                     unsigned int *value)
{
        check_expected_ptr(marc);
        check_expected(region_num);
        check_expected(clos_number);

        *value = 0;
        return PQOS_RETVAL_OK;
}

int
__wrap_get_mba_max_bw_region_clos_v1(const struct pqos_erdt_marc *marc,
                                     int region_num,
                                     unsigned int clos_number,
                                     unsigned int *value)
{
        check_expected_ptr(marc);
        check_expected(region_num);
        check_expected(clos_number);

        *value = 0;
        return PQOS_RETVAL_OK;
}

int
__wrap_set_mba_optimal_bw_region_clos_v1(const struct pqos_erdt_marc *marc,
                                         int region_num,
                                         unsigned int clos_number,
                                         unsigned int value)
{
        check_expected_ptr(marc);
        check_expected(region_num);
        check_expected(clos_number);
        check_expected(value);

        return mock_type(int);
}

static void
test_cpu_cmt_range_crosses_clump(void **state __attribute__((unused)))
{
        struct pqos_erdt_cmrc cmrc = {0};
        uint64_t registers[PAGE_SIZE / sizeof(uint64_t)] = {0};
        l3_cmt_rmid_t values[4] = {0};
        const uint64_t expected[] = {2, 3, 4, 5};
        int ret;

        cmrc.block_base_addr = 0x1000;
        cmrc.block_size = 1;
        cmrc.clump_size = 4;
        cmrc.clump_stride = 64;

        registers[2] = expected[0];
        registers[3] = expected[1];
        registers[8] = expected[2];
        registers[9] = expected[3];

        expect_value(__wrap_pqos_mmap_read, address, cmrc.block_base_addr);
        expect_value(__wrap_pqos_mmap_read, size, PAGE_SIZE);
        will_return(__wrap_pqos_mmap_read, registers);
        expect_value(__wrap_pqos_munmap, mem, registers);
        expect_value(__wrap_pqos_munmap, size, PAGE_SIZE);

        ret = get_l3_cmt_rmid_range_v1(&cmrc, 2, 5, values);

        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_memory_equal(values, expected, sizeof(expected));
}

static void
test_io_cmt_range_crosses_page(void **state __attribute__((unused)))
{
        struct pqos_erdt_cmrd cmrd = {0};
        uint64_t registers[2 * PAGE_SIZE / sizeof(uint64_t)] = {0};
        iol3_cmt_rmid_t values[3] = {0};
        const uint64_t expected[] = {1, 2, 3};
        int ret;

        cmrd.reg_base_addr = 0x2000;
        cmrd.reg_block_size = 2;
        cmrd.offset = 32;
        cmrd.clump_size = 2;

        registers[5] = expected[0];
        registers[(PAGE_SIZE + 32) / sizeof(uint64_t)] = expected[1];
        registers[(PAGE_SIZE + 40) / sizeof(uint64_t)] = expected[2];

        expect_value(__wrap_pqos_mmap_read, address, cmrd.reg_base_addr);
        expect_value(__wrap_pqos_mmap_read, size, 2 * PAGE_SIZE);
        will_return(__wrap_pqos_mmap_read, registers);
        expect_value(__wrap_pqos_munmap, mem, registers);
        expect_value(__wrap_pqos_munmap, size, 2 * PAGE_SIZE);

        ret = get_iol3_cmt_rmid_range_v1(&cmrd, 1, 3, values);

        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_memory_equal(values, expected, sizeof(expected));
}

static void
test_cmt_range_rejects_invalid_range(void **state __attribute__((unused)))
{
        struct pqos_erdt_cmrc cmrc = {0};
        uint64_t registers[PAGE_SIZE / sizeof(uint64_t)] = {0};
        l3_cmt_rmid_t value;
        int ret;

        cmrc.block_size = 1;
        cmrc.clump_size = 4;

        expect_value(__wrap_pqos_mmap_read, address, cmrc.block_base_addr);
        expect_value(__wrap_pqos_mmap_read, size, PAGE_SIZE);
        will_return(__wrap_pqos_mmap_read, registers);
        expect_value(__wrap_pqos_munmap, mem, registers);
        expect_value(__wrap_pqos_munmap, size, PAGE_SIZE);

        ret = get_l3_cmt_rmid_range_v1(&cmrc, 2, 1, &value);

        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        cmrc.clump_stride = PAGE_SIZE;
        expect_value(__wrap_pqos_mmap_read, address, cmrc.block_base_addr);
        expect_value(__wrap_pqos_mmap_read, size, PAGE_SIZE);
        will_return(__wrap_pqos_mmap_read, registers);
        expect_value(__wrap_pqos_munmap, mem, registers);
        expect_value(__wrap_pqos_munmap, size, PAGE_SIZE);

        ret = get_l3_cmt_rmid_range_v1(&cmrc, 4, 4, &value);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

static void
test_mbm_range_rejects_invalid_range(void **state __attribute__((unused)))
{
        struct pqos_erdt_mmrc mmrc = {0};
        uint64_t registers[PAGE_SIZE / sizeof(uint64_t)] = {0};
        l3_mbm_rmid_t value;
        int ret;

        mmrc.reg_block_size = 1;

        ret = get_l3_mbm_region_rmid_range_v1(&mmrc, 0, 2, 1, &value);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        expect_value(__wrap_pqos_mmap_read, address, mmrc.reg_block_base_addr);
        expect_value(__wrap_pqos_mmap_read, size, PAGE_SIZE);
        will_return(__wrap_pqos_mmap_read, registers);
        expect_value(__wrap_pqos_munmap, mem, registers);
        expect_value(__wrap_pqos_munmap, size, PAGE_SIZE);

        ret = get_l3_mbm_region_rmid_range_v1(&mmrc, 0, 0, UINT_MAX, &value);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

static void
test_mba_set_resolves_domain_id(void **state __attribute__((unused)))
{
        struct pqos_cpu_agent_info cpu_agents[2] = {0};
        struct pqos_erdt_info erdt = {0};
        struct pqos_mba requested = {0};
        int ret;

        erdt.max_clos = 4;
        erdt.num_cpu_agents = 2;
        erdt.cpu_agents = cpu_agents;
        cpu_agents[0].rmdd.domain_id = 10;
        cpu_agents[1].rmdd.domain_id = 20;

        requested.class_id = 1;
        requested.domain_id = 20;
        requested.num_mem_regions = 1;
        requested.mem_regions[0].region_num = 0;
        requested.mem_regions[0].bw_ctrl_val[PQOS_BW_CTRL_TYPE_OPT_IDX] = 100;
        requested.mem_regions[0].bw_ctrl_val[PQOS_BW_CTRL_TYPE_MIN_IDX] = -1;
        requested.mem_regions[0].bw_ctrl_val[PQOS_BW_CTRL_TYPE_MAX_IDX] = -1;

        will_return(__wrap__pqos_get_erdt, &erdt);
        expect_value(__wrap_set_mba_optimal_bw_region_clos_v1, marc,
                     &cpu_agents[1].marc);
        expect_value(__wrap_set_mba_optimal_bw_region_clos_v1, region_num, 0);
        expect_value(__wrap_set_mba_optimal_bw_region_clos_v1, clos_number, 1);
        expect_value(__wrap_set_mba_optimal_bw_region_clos_v1, value, 100);
        will_return(__wrap_set_mba_optimal_bw_region_clos_v1, PQOS_RETVAL_OK);

        ret = mmio_mba_set(0, 1, &requested, NULL);

        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_mba_get_ignores_num_clos_input(void **state __attribute__((unused)))
{
        struct pqos_cpu_agent_info cpu_agent = {0};
        struct pqos_erdt_info erdt = {0};
        struct pqos_mrrm_info mrrm = {0};
        struct pqos_mba mba_tab[2] = {0};
        unsigned num_clos = UINT32_MAX;
        const int num_reads = 2 * PQOS_MAX_MEM_REGIONS;
        int ret;

        erdt.max_clos = 2;
        erdt.num_cpu_agents = 1;
        erdt.cpu_agents = &cpu_agent;
        cpu_agent.rmdd.domain_id = 10;
        mrrm.max_memory_regions_supported = PQOS_MAX_MEM_REGIONS + 1;
        mba_tab[0].domain_id = cpu_agent.rmdd.domain_id;

        will_return(__wrap__pqos_get_erdt, &erdt);
        will_return(__wrap__pqos_get_mrrm, &mrrm);
        expect_value_count(__wrap_get_mba_optimal_bw_region_clos_v1, marc,
                           &cpu_agent.marc, num_reads);
        expect_any_count(__wrap_get_mba_optimal_bw_region_clos_v1, region_num,
                         num_reads);
        expect_any_count(__wrap_get_mba_optimal_bw_region_clos_v1, clos_number,
                         num_reads);
        expect_value_count(__wrap_get_mba_min_bw_region_clos_v1, marc,
                           &cpu_agent.marc, num_reads);
        expect_any_count(__wrap_get_mba_min_bw_region_clos_v1, region_num,
                         num_reads);
        expect_any_count(__wrap_get_mba_min_bw_region_clos_v1, clos_number,
                         num_reads);
        expect_value_count(__wrap_get_mba_max_bw_region_clos_v1, marc,
                           &cpu_agent.marc, num_reads);
        expect_any_count(__wrap_get_mba_max_bw_region_clos_v1, region_num,
                         num_reads);
        expect_any_count(__wrap_get_mba_max_bw_region_clos_v1, clos_number,
                         num_reads);

        ret = mmio_mba_get(0, 2, &num_clos, mba_tab);

        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(num_clos, 2);
        assert_int_equal(mba_tab[1].domain_id, cpu_agent.rmdd.domain_id);
        assert_int_equal(mba_tab[0].class_id, 0);
        assert_int_equal(mba_tab[1].class_id, 1);
        assert_int_equal(mba_tab[0].num_mem_regions, PQOS_MAX_MEM_REGIONS);
        assert_int_equal(mba_tab[1].num_mem_regions, PQOS_MAX_MEM_REGIONS);
}

static void
assert_io_overflow_invalidates_baseline(const enum pqos_mon_event event)
{
        struct pqos_device_agent_info dev_agent = {0};
        struct pqos_erdt_info erdt = {0};
        struct pqos_channels_domains channels_domains = {0};
        struct pqos_mon_poll_ctx ctx = {0};
        struct pqos_mon_data_internal intl = {0};
        struct pqos_mon_data group = {0};
        pqos_channel_t channel_id = 1;
        uint16_t domain_id = 2;
        uint16_t domain_id_idx = 0;
        int ret;

        dev_agent.ibrd.reg_block_size = 1;
        dev_agent.ibrd.bw_reg_clump_size = 1;
        dev_agent.ibrd.miss_reg_clump_size = 1;
        erdt.num_dev_agents = 1;
        erdt.dev_agents = &dev_agent;
        channels_domains.num_channel_ids = 1;
        channels_domains.channel_ids = &channel_id;
        channels_domains.domain_ids = &domain_id;
        channels_domains.domain_id_idxs = &domain_id_idx;
        ctx.channel_id = channel_id;
        intl.hw.ctx = &ctx;
        intl.hw.num_ctx = 1;
        intl.valid_io_total_read = 1;
        intl.valid_io_miss_read = 1;
        group.intl = &intl;

        will_return(__wrap__pqos_get_erdt, &erdt);
        will_return(__wrap__pqos_get_channels_domains, &channels_domains);
        if (event == PQOS_MON_EVENT_IO_TOTAL_MEM_BW) {
                expect_value(__wrap_get_total_iol3_mbm_rmid_range_v1, ibrd,
                             &dev_agent.ibrd);
                expect_value(__wrap_get_total_iol3_mbm_rmid_range_v1,
                             rmid_first, 0);
                expect_value(__wrap_get_total_iol3_mbm_rmid_range_v1, rmid_last,
                             0);
                will_return(__wrap_get_total_iol3_mbm_rmid_range_v1,
                            PQOS_RETVAL_OK);
        } else {
                expect_value(__wrap_get_miss_iol3_mbm_rmid_range_v1, ibrd,
                             &dev_agent.ibrd);
                expect_value(__wrap_get_miss_iol3_mbm_rmid_range_v1, rmid_first,
                             0);
                expect_value(__wrap_get_miss_iol3_mbm_rmid_range_v1, rmid_last,
                             0);
                will_return(__wrap_get_miss_iol3_mbm_rmid_range_v1,
                            PQOS_RETVAL_OK);
        }

        ret = mmio_mon_read_counter(&group, event);

        assert_int_equal(ret, PQOS_RETVAL_OVERFLOW);
        if (event == PQOS_MON_EVENT_IO_TOTAL_MEM_BW)
                assert_false(intl.valid_io_total_read);
        else
                assert_false(intl.valid_io_miss_read);
}

static void
test_io_overflow_invalidates_baseline(void **state __attribute__((unused)))
{
        assert_io_overflow_invalidates_baseline(PQOS_MON_EVENT_IO_TOTAL_MEM_BW);
        assert_io_overflow_invalidates_baseline(PQOS_MON_EVENT_IO_MISS_MEM_BW);
}

int
main(void)
{
        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_cpu_cmt_range_crosses_clump),
            cmocka_unit_test(test_io_cmt_range_crosses_page),
            cmocka_unit_test(test_cmt_range_rejects_invalid_range),
            cmocka_unit_test(test_mbm_range_rejects_invalid_range),
            cmocka_unit_test(test_mba_set_resolves_domain_id),
            cmocka_unit_test(test_mba_get_ignores_num_clos_input),
            cmocka_unit_test(test_io_overflow_invalidates_baseline)};

        return cmocka_run_group_tests(tests, NULL, NULL);
}
