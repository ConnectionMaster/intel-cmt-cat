/*
 * BSD LICENSE
 *
 * Copyright(c) 2023-2026 Intel Corporation. All rights reserved.
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

#include "mrrm.h"
#include "test.h"

#define SIZE (sizeof(struct mrrm_header) + sizeof(struct mrrm_mre_list) + 8)
struct fixture {
        _Alignas(uint64_t) uint8_t data[SIZE];
        struct acpi_table table;
};
int
__wrap_acpi_init(void)
{
        function_called();
        return mock_type(int);
}
int
__wrap_acpi_fini(void)
{
        function_called();
        return mock_type(int);
}
struct acpi_table *
__wrap_acpi_get_sig(const char *sig)
{
        function_called();
        check_expected(sig);
        return mock_ptr_type(struct acpi_table *);
}
void
__wrap_acpi_print(struct acpi_table *table)
{
        function_called();
        check_expected_ptr(table);
}
void
__wrap_acpi_free(struct acpi_table *table)
{
        function_called();
        check_expected_ptr(table);
}
static void
init(struct fixture *f, int dynamic)
{
        struct acpi_table_mrrm *t;
        struct mrrm_mre_list *m;
        uint64_t reg = 0x1000;

        memset(f, 0, sizeof(*f));
        t = (void *)f->data;
        f->table.mrrm = t;
        t->header.header.revision = ACPI_MRRM_REVISION;
        t->header.header.length = sizeof(struct mrrm_header) + sizeof(*m);
        t->header.max_memory_regions_supported = 4;
        t->header.flags = dynamic;
        m = t->mre;
        m->type = ACPI_MRRM_MRE_TYPE;
        m->length = sizeof(*m);
        m->base_address_low = 0x100000;
        m->local_region_id = 2;
        if (dynamic) {
                m->length += sizeof(reg);
                t->header.header.length += sizeof(reg);
                memcpy(m->region_id_programming_registers, &reg, sizeof(reg));
        }
}
static void
expect_table(struct fixture *f)
{
        expect_function_call(__wrap_acpi_init);
        will_return(__wrap_acpi_init, PQOS_RETVAL_OK);
        expect_function_call(__wrap_acpi_get_sig);
        expect_string(__wrap_acpi_get_sig, sig, ACPI_TABLE_SIG_MRRM);
        will_return(__wrap_acpi_get_sig, &f->table);
        expect_function_call(__wrap_acpi_print);
        expect_value(__wrap_acpi_print, table, &f->table);
        expect_function_call(__wrap_acpi_free);
        expect_value(__wrap_acpi_free, table, &f->table);
}
static void
test_valid(void **state __attribute__((unused)))
{
        struct fixture f;
        struct pqos_cap cap = {0};
        struct pqos_mrrm_info *info = NULL;
        uint64_t reg;

        init(&f, 0);
        expect_table(&f);
        assert_int_equal(mrrm_init(&cap, &info), PQOS_RETVAL_OK);
        assert_int_equal(info->num_mres, 1);
        assert_int_equal(info->mre[0].local_region_id, 2);
        mrrm_fini();
        init(&f, 1);
        expect_table(&f);
        assert_int_equal(mrrm_init(&cap, &info), PQOS_RETVAL_OK);
        memcpy(&reg, info->mre[0].programming_regs, sizeof(reg));
        assert_int_equal(reg, 0x1000);
        mrrm_fini();
        mrrm_fini();
}
static void
test_max_memory_regions_reported(void **state __attribute__((unused)))
{
        struct fixture f;
        struct pqos_cap cap = {0};
        struct pqos_mrrm_info *info = NULL;
        struct acpi_table_mrrm *t;

        /*
         * The platform value is reported as it is, including a count above
         * PQOS_MAX_MEM_REGIONS. Limiting it to the regions an interface can
         * address is the job of mmio_get_num_mem_regions(), covered by
         * test_mmio.
         */
        init(&f, 0);
        t = (void *)f.data;
        t->header.max_memory_regions_supported = PQOS_MAX_MEM_REGIONS + 1;
        expect_table(&f);
        assert_int_equal(mrrm_init(&cap, &info), PQOS_RETVAL_OK);
        assert_int_equal(info->max_memory_regions_supported,
                         PQOS_MAX_MEM_REGIONS + 1);
        mrrm_fini();

        init(&f, 0);
        t = (void *)f.data;
        t->header.max_memory_regions_supported = 2;
        expect_table(&f);
        assert_int_equal(mrrm_init(&cap, &info), PQOS_RETVAL_OK);
        assert_int_equal(info->max_memory_regions_supported, 2);
        mrrm_fini();
}

static void
test_errors(void **state __attribute__((unused)))
{
        struct fixture f;
        struct pqos_cap cap = {0};
        struct pqos_mrrm_info *info = NULL;

        assert_int_equal(mrrm_init(NULL, &info), PQOS_RETVAL_PARAM);
        assert_int_equal(mrrm_init(&cap, NULL), PQOS_RETVAL_PARAM);
        init(&f, 0);
        f.table.mrrm->header.header.revision++;
        expect_table(&f);
        expect_function_call(__wrap_acpi_fini);
        will_return(__wrap_acpi_fini, 0);
        assert_int_equal(mrrm_init(&cap, &info), PQOS_RETVAL_ERROR);
        init(&f, 0);
        f.table.mrrm->mre[0].length--;
        expect_table(&f);
        expect_function_call(__wrap_acpi_fini);
        will_return(__wrap_acpi_fini, 0);
        assert_int_equal(mrrm_init(&cap, &info), PQOS_RETVAL_ERROR);
}
int
main(void)
{
        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_valid),
            cmocka_unit_test(test_max_memory_regions_reported),
            cmocka_unit_test(test_errors)};
        return cmocka_run_group_tests(tests, NULL, NULL);
}
