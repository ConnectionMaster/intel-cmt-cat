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

#include "erdt.h"
#include "test.h"

#define DASE (ACPI_ERDT_STRUCT_DASE_HEADER_LENGTH + 2)
#define CPU                                                                    \
        (sizeof(struct acpi_table_erdt_rmdd) +                                 \
         sizeof(struct acpi_table_erdt_cacd) + 4)
#define DEV                                                                    \
        (sizeof(struct acpi_table_erdt_rmdd) +                                 \
         sizeof(struct acpi_table_erdt_dacd) + DASE)
#define SIZE (sizeof(struct acpi_table_erdt_header) + CPU + DEV)
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
init(struct fixture *f)
{
        struct acpi_table_erdt *t;
        struct acpi_table_erdt_rmdd *cpu, *dev;
        struct acpi_table_erdt_cacd *cacd;
        struct acpi_table_erdt_dacd *dacd;
        struct acpi_table_erdt_dase *dase;
        uint32_t id = 0x1234;

        memset(f, 0, sizeof(*f));
        t = (void *)f->data;
        f->table.erdt = t;
        t->header.header.revision = ACPI_ERDT_REVISION;
        t->header.header.length = SIZE;
        t->header.max_clos = 16;
        cpu = (void *)t->erdt_sub_structures;
        cpu->type = 0;
        cpu->length = CPU;
        cpu->flags = 1;
        cpu->domain_id = 7;
        cpu->max_rmids = 3;
        cacd = (void *)(cpu + 1);
        cacd->type = 1;
        cacd->length = sizeof(*cacd) + 4;
        cacd->rmdd_domain_id = 7;
        memcpy(cacd->enumeration_ids, &id, 4);
        dev = (void *)((uint8_t *)cpu + cpu->length);
        dev->type = 0;
        dev->length = DEV;
        dev->flags = 2;
        dev->domain_id = 9;
        dacd = (void *)(dev + 1);
        dacd->type = 2;
        dacd->length = sizeof(*dacd) + DASE;
        dacd->rmdd_domain_id = 9;
        dase = dacd->dase;
        dase->type = 1;
        dase->length = DASE;
        dase->segment_number = 2;
        dase->path[0] = 4;
        dase->path[1] = 1;
}
static void
expect_table(struct fixture *f)
{
        expect_function_call(__wrap_acpi_init);
        will_return(__wrap_acpi_init, PQOS_RETVAL_OK);
        expect_function_call(__wrap_acpi_get_sig);
        expect_string(__wrap_acpi_get_sig, sig, ACPI_TABLE_SIG_ERDT);
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
        struct pqos_cpuinfo cpu = {0};
        struct pqos_erdt_info *info = NULL;

        init(&f);
        expect_table(&f);
        assert_int_equal(erdt_init(&cap, &cpu, &info), PQOS_RETVAL_OK);
        assert_int_equal(info->num_cpu_agents, 1);
        assert_int_equal(info->num_dev_agents, 1);
        assert_int_equal(info->cpu_agents[0].cacd.enumeration_ids[0], 0x1234);
        assert_int_equal(info->dev_agents[0].dacd.dase[0].path[0], 4);
        erdt_fini();
        erdt_fini();
}
static void
test_errors(void **state __attribute__((unused)))
{
        struct fixture f;
        struct pqos_cap cap = {0};
        struct pqos_cpuinfo cpu = {0};
        struct pqos_erdt_info *info = NULL;
        struct acpi_table_erdt_rmdd *rmdd;

        assert_int_equal(erdt_init(NULL, &cpu, &info), PQOS_RETVAL_PARAM);
        assert_int_equal(erdt_init(&cap, NULL, &info), PQOS_RETVAL_PARAM);
        expect_function_call(__wrap_acpi_init);
        will_return(__wrap_acpi_init, PQOS_RETVAL_OK);
        expect_function_call(__wrap_acpi_get_sig);
        expect_string(__wrap_acpi_get_sig, sig, ACPI_TABLE_SIG_ERDT);
        will_return(__wrap_acpi_get_sig, NULL);
        expect_function_call(__wrap_acpi_fini);
        will_return(__wrap_acpi_fini, PQOS_RETVAL_OK);
        assert_int_equal(erdt_init(&cap, &cpu, &info), PQOS_RETVAL_RESOURCE);
        init(&f);
        f.table.erdt->header.header.revision++;
        expect_table(&f);
        expect_function_call(__wrap_acpi_fini);
        will_return(__wrap_acpi_fini, PQOS_RETVAL_OK);
        assert_int_equal(erdt_init(&cap, &cpu, &info), PQOS_RETVAL_ERROR);
        init(&f);
        rmdd = (void *)f.table.erdt->erdt_sub_structures;
        rmdd->flags = 4;
        expect_table(&f);
        expect_function_call(__wrap_acpi_fini);
        will_return(__wrap_acpi_fini, PQOS_RETVAL_OK);
        assert_int_equal(erdt_init(&cap, &cpu, &info), PQOS_RETVAL_ERROR);
}
int
main(void)
{
        const struct CMUnitTest tests[] = {cmocka_unit_test(test_valid),
                                           cmocka_unit_test(test_errors)};
        return cmocka_run_group_tests(tests, NULL, NULL);
}
