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

#include "mmio_dump_rmids.h"
#include "test.h"

#include <string.h>

const struct pqos_erdt_info *
__wrap__pqos_get_erdt(void)
{
        return mock_ptr_type(const struct pqos_erdt_info *);
}

static void
test_uint64_to_binary(void **state __attribute__((unused)))
{
        char binary[65];
        unsigned i;

        memset(binary, 'x', sizeof(binary));
        uint64_to_binary(0x8000000000000001ULL, binary);

        assert_int_equal(strlen(binary), 64);
        assert_int_equal(binary[0], '1');
        for (i = 1; i < 63; i++)
                assert_int_equal(binary[i], '0');
        assert_int_equal(binary[63], '1');
        assert_int_equal(binary[64], '\0');
}

static void
test_mmio_dump_rmids_null(void **state __attribute__((unused)))
{
        int ret;

        ret = mmio_dump_rmids(NULL);

        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

static void
test_mmio_dump_rmids_unknown_domain(void **state __attribute__((unused)))
{
        struct pqos_erdt_info erdt = {0};
        uint16_t domain_id = 1;
        pqos_rmid_t rmid = 1;
        struct pqos_mmio_dump_rmids dump = {.num_domain_ids = 1,
                                            .domain_ids = &domain_id,
                                            .num_rmids = 1,
                                            .rmids = &rmid,
                                            .rmid_type =
                                                MMIO_DUMP_RMID_TYPE_CMT};
        int ret;

        will_return(__wrap__pqos_get_erdt, &erdt);
        ret = mmio_dump_rmids(&dump);

        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

static void
test_mmio_dump_rmids_invalid(void **state __attribute__((unused)))
{
        struct pqos_erdt_info erdt = {0};
        struct pqos_mmio_dump_rmids dump = {0};
        int ret;

        will_return(__wrap__pqos_get_erdt, &erdt);
        ret = mmio_dump_rmids(&dump);

        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

int
main(void)
{
        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_uint64_to_binary),
            cmocka_unit_test(test_mmio_dump_rmids_null),
            cmocka_unit_test(test_mmio_dump_rmids_unknown_domain),
            cmocka_unit_test(test_mmio_dump_rmids_invalid)};

        return cmocka_run_group_tests(tests, NULL, NULL);
}
