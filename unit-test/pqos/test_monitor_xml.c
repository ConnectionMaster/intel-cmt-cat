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
#define _GNU_SOURCE

#include "mock_test.h"
#include "monitor_xml.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static enum pqos_mon_event selected_events;
static enum monitor_llc_format llc_format;
static int core_mode;
static int mixed_mode;

enum pqos_mon_event
monitor_get_events(void)
{
        return selected_events;
}

enum monitor_llc_format
monitor_get_llc_format(void)
{
        return llc_format;
}

int
monitor_core_mode(void)
{
        return core_mode;
}

int
monitor_process_mode(void)
{
        return 0;
}

int
monitor_iordt_mode(void)
{
        return 0;
}

int
monitor_uncore_mode(void)
{
        return 0;
}

int
monitor_mixed_mode(void)
{
        return mixed_mode;
}

double
monitor_utils_get_value(const struct pqos_mon_data *const group,
                        const enum pqos_mon_event event)
{
        (void)group;
        (void)event;

        return 1.0;
}

double
monitor_utils_get_region_value(const struct pqos_mon_data *const group,
                               const enum pqos_mon_event event,
                               const int region_num)
{
        (void)group;

        if (event == PQOS_MON_EVENT_TMEM_BW)
                return 10.0 + region_num;
        if (event == PQOS_MON_EVENT_L3_OCCUP)
                return 1.0;
        if (event == PQOS_MON_EVENT_IO_L3_OCCUP)
                return 20.0;
        if (event == PQOS_MON_EVENT_IO_TOTAL_MEM_BW)
                return 30.0;
        if (event == PQOS_MON_EVENT_IO_MISS_MEM_BW)
                return 40.0;

        return 2.0;
}

int
monitor_utils_get_pid_cores(const struct pqos_mon_data *mon_data,
                            char *cores_s,
                            const int len)
{
        (void)mon_data;
        (void)cores_s;
        (void)len;

        return -1;
}

static char *
render_row(const struct pqos_mon_data *data, const int region_aware)
{
        FILE *fp;
        char *xml = NULL;
        size_t xml_size = 0;

        fp = open_memstream(&xml, &xml_size);
        assert_non_null(fp);

        if (region_aware)
                monitor_xml_region_row(fp, "2026<&", data);
        else
                monitor_xml_row(fp, "2026<&", data);
        assert_int_equal(fclose(fp), 0);
        assert_true(xml_size > 0);

        return xml;
}

static void
test_xml_region_core_values(void **state)
{
        struct pqos_mon_data data = {0};
        char context[] = "core<&";
        char *xml;

        selected_events = (enum pqos_mon_event)(PQOS_MON_EVENT_L3_OCCUP |
                                                PQOS_MON_EVENT_LMEM_BW |
                                                PQOS_MON_EVENT_TMEM_BW);
        llc_format = LLC_FORMAT_KILOBYTES;
        core_mode = 0;
        mixed_mode = 1;
        data.event = selected_events;
        data.context = context;
        data.num_cores = 1;
        data.regions.num_mem_regions = 2;
        data.regions.region_num[0] = 0;
        data.regions.region_num[1] = 3;

        xml = render_row(&data, 1);
        assert_non_null(strstr(xml, "<mbm_local_MB>1.0"));
        assert_non_null(strstr(xml, "<time>2026&lt;&amp;</time>"));
        assert_non_null(strstr(xml, "<core>core&lt;&amp;</core>"));
        assert_non_null(strstr(xml, "<l3_occupancy_kB>1.0"));
        assert_non_null(strstr(xml, "<mbm_total_region_0_MB>10.0"));
        assert_non_null(strstr(xml, "<mbm_total_region_3_MB>13.0"));
        free(xml);
        (void)state;
}

static void
test_xml_region_io_values(void **state)
{
        struct pqos_mon_data data = {0};
        char context[] = "0000:00:01.0@0";
        char *xml;

        selected_events = (enum pqos_mon_event)(PQOS_MON_EVENT_IO_L3_OCCUP |
                                                PQOS_MON_EVENT_IO_TOTAL_MEM_BW |
                                                PQOS_MON_EVENT_IO_MISS_MEM_BW);
        llc_format = LLC_FORMAT_PERCENT;
        core_mode = 0;
        mixed_mode = 1;
        data.event = selected_events;
        data.context = context;
        data.num_channels = 1;

        xml = render_row(&data, 1);
        assert_non_null(strstr(xml, "<channel>0000:00:01.0@0</channel>"));
        assert_non_null(strstr(xml, "<io_l3_occupancy_percent>20.0"));
        assert_non_null(strstr(xml, "<io_mbm_total_MB>30.0"));
        assert_non_null(strstr(xml, "<io_mbm_miss_MB>40.0"));
        free(xml);
        (void)state;
}

static void
test_xml_row_handles_all_events(void **state)
{
        struct pqos_mon_data data = {0};
        char context[] = "0";
        char *xml;

        selected_events = (enum pqos_mon_event) - 1;
        llc_format = LLC_FORMAT_KILOBYTES;
        core_mode = 1;
        mixed_mode = 0;
        data.event = selected_events;
        data.context = context;
        data.num_cores = 1;

        xml = render_row(&data, 0);
        assert_true(strlen(xml) > 256);
        assert_non_null(strstr(xml, "<io_mbm_miss_MB>1.0"));
        assert_non_null(strstr(xml, "</record>"));
        free(xml);
        (void)state;
}

int
main(void)
{
        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_xml_region_core_values),
            cmocka_unit_test(test_xml_region_io_values),
            cmocka_unit_test(test_xml_row_handles_all_events),
        };

        return cmocka_run_group_tests(tests, NULL, NULL);
}
