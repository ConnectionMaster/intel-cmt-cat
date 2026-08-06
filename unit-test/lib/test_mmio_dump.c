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

#include "mmio_dump.h"
#include "test.h"

#include <limits.h>
#include <stdint.h>

uint8_t *
__wrap_pqos_mmap_write(uint64_t address, const uint64_t size)
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

static void
test_dump_mmio_range_remaining(void **state __attribute__((unused)))
{
        uint8_t registers[8] = {0};
        int ret;

        expect_value(__wrap_pqos_mmap_write, address, 0x1008);
        expect_value(__wrap_pqos_mmap_write, size, sizeof(registers));
        will_return(__wrap_pqos_mmap_write, registers);
        expect_value(__wrap_pqos_munmap, mem, registers);
        expect_value(__wrap_pqos_munmap, size, sizeof(registers));

        ret = dump_mmio_range(0x1000, 16, 1, 0, 8, 1, 0);

        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_dump_mmio_range_invalid(void **state __attribute__((unused)))
{
        int ret;

        ret = dump_mmio_range(0, UINT64_MAX, ULONG_MAX, 1, 8, 1, 0);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ret = dump_mmio_range(0, 8, 8, 1, 1, 1, 0);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ret = dump_mmio_range(UINT64_MAX, 2, 1, 1, 1, 1, 0);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ret = dump_mmio_range(0, 8, 0, 1, 4, 1, 0);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

static void
test_mmio_dump_invalid(void **state __attribute__((unused)))
{
        struct pqos_erdt_info erdt = {0};
        struct pqos_mmio_dump dump = {0};
        int ret;

        ret = mmio_dump(NULL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        will_return(__wrap__pqos_get_erdt, &erdt);
        ret = mmio_dump(&dump);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

int
main(void)
{
        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_dump_mmio_range_remaining),
            cmocka_unit_test(test_dump_mmio_range_invalid),
            cmocka_unit_test(test_mmio_dump_invalid)};

        return cmocka_run_group_tests(tests, NULL, NULL);
}
