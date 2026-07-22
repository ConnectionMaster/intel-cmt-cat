/*
 * BSD LICENSE
 *
 * Copyright(c) 2022-2026 Intel Corporation. All rights reserved.
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

#include "monitor_xml.h"

#include "common.h"
#include "monitor.h"
#include "monitor_utils.h"

#include <string.h>

static const char *xml_root_open = "<records>";
static const char *xml_root_close = "</records>";
static const char *xml_child_open = "<record>";
static const char *xml_child_close = "</record>";

#define INVALID_REGION_NUM -1

static const struct {
        enum pqos_mon_event event;
        const char *node_name;
        const char *percent_node_name;
        const char *format;
} output[] = {
    {.event = PQOS_PERF_EVENT_IPC, .node_name = "ipc", .format = "%.2f"},
    {.event = PQOS_PERF_EVENT_LLC_MISS,
     .node_name = "llc_misses",
     .format = "%.0f"},
    {.event = PQOS_PERF_EVENT_LLC_REF,
     .node_name = "llc_references",
     .format = "%.0f"},
    {.event = PQOS_MON_EVENT_L3_OCCUP,
     .node_name = "l3_occupancy_kB",
     .percent_node_name = "l3_occupancy_percent",
     .format = "%.1f"},
    {.event = PQOS_MON_EVENT_LMEM_BW,
     .node_name = "mbm_local_MB",
     .format = "%.1f"},
    {.event = PQOS_MON_EVENT_RMEM_BW,
     .node_name = "mbm_remote_MB",
     .format = "%.1f"},
    {.event = PQOS_MON_EVENT_TMEM_BW,
     .node_name = "mbm_total_MB",
     .format = "%.1f"},
    {.event = PQOS_MON_EVENT_IO_L3_OCCUP,
     .node_name = "io_l3_occupancy_kB",
     .percent_node_name = "io_l3_occupancy_percent",
     .format = "%.1f"},
    {.event = PQOS_MON_EVENT_IO_TOTAL_MEM_BW,
     .node_name = "io_mbm_total_MB",
     .format = "%.1f"},
    {.event = PQOS_MON_EVENT_IO_MISS_MEM_BW,
     .node_name = "io_mbm_miss_MB",
     .format = "%.1f"},
    {.event = PQOS_PERF_EVENT_LLC_MISS_PCIE_READ,
     .node_name = "llc_misses_read",
     .format = "%.0f"},
    {.event = PQOS_PERF_EVENT_LLC_MISS_PCIE_WRITE,
     .node_name = "llc_misses_write",
     .format = "%.0f"},
    {.event = PQOS_PERF_EVENT_LLC_REF_PCIE_READ,
     .node_name = "llc_references_read",
     .format = "%.0f"},
    {.event = PQOS_PERF_EVENT_LLC_REF_PCIE_WRITE,
     .node_name = "llc_references_write",
     .format = "%.0f"},
    {.event = PQOS_MON_EVENT_CORE_ENERGY,
     .node_name = "core_energy_J",
     .format = "%.3f"},
    {.event = PQOS_MON_EVENT_ACTIVITY,
     .node_name = "activity",
     .format = "%.3f"},
    {.event = PQOS_MON_EVENT_POWER, .node_name = "power_W", .format = "%.3f"},
};

void
monitor_xml_begin(FILE *fp, const int num_mem_regions, const int *region_num)
{
        UNUSED_ARG(num_mem_regions);
        UNUSED_ARG(region_num);

        ASSERT(fp != NULL);

        fprintf(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n%s\n",
                xml_root_open);
}

void
monitor_xml_header(FILE *fp,
                   const char *timestamp,
                   const int num_mem_regions,
                   const int *region_num)
{
        UNUSED_ARG(fp);
        UNUSED_ARG(timestamp);
        UNUSED_ARG(num_mem_regions);
        UNUSED_ARG(region_num);
}

/**
 * @brief Print an escaped XML text node
 *
 * @param fp output stream
 * @param node_name XML node name
 * @param value node text
 */
static void
print_xml_text(FILE *fp, const char *node_name, const char *value)
{
        const unsigned char *p = (const unsigned char *)value;

        fprintf(fp, "\t<%s>", node_name);
        while (*p != '\0') {
                switch (*p) {
                case '&':
                        fputs("&amp;", fp);
                        break;
                case '<':
                        fputs("&lt;", fp);
                        break;
                case '>':
                        fputs("&gt;", fp);
                        break;
                case '"':
                        fputs("&quot;", fp);
                        break;
                case '\'':
                        fputs("&apos;", fp);
                        break;
                default:
                        fputc(*p, fp);
                        break;
                }
                p++;
        }
        fprintf(fp, "</%s>\n", node_name);
}

static void
print_xml_value(FILE *fp,
                const char *format,
                const double value,
                const int is_monitored,
                const int is_present,
                const char *node_name)
{
        if (is_monitored) {
                fprintf(fp, "\t<%s>", node_name);
                fprintf(fp, format, value);
                fprintf(fp, "</%s>\n", node_name);
        } else if (is_present)
                fprintf(fp, "\t<%s></%s>\n", node_name, node_name);
}

static const char *
get_node_name(const unsigned idx, const enum monitor_llc_format format)
{
        if (format == LLC_FORMAT_PERCENT &&
            output[idx].percent_node_name != NULL)
                return output[idx].percent_node_name;

        return output[idx].node_name;
}

static void
print_xml_context(FILE *fp, const struct pqos_mon_data *mon_data)
{
        const char *node_name = NULL;

        if (monitor_mixed_mode())
                node_name = mon_data->num_cores > 0 ? "core" : "channel";
        else if (monitor_core_mode())
                node_name = "core";
        else if (monitor_process_mode())
                node_name = "pid";
        else if (monitor_iordt_mode())
                node_name = "channel";
        else if (monitor_uncore_mode())
                node_name = "socket";

        if (node_name == NULL) {
                fprintf(stderr, "Unable to determine XML monitoring context\n");
                return;
        }
        print_xml_text(fp, node_name, (const char *)mon_data->context);

        if (monitor_process_mode()) {
                char core_list[1024] = {0};

                if (monitor_utils_get_pid_cores(mon_data, core_list,
                                                sizeof(core_list)) != 0)
                        strncpy(core_list, "err", sizeof(core_list) - 1);
                print_xml_text(fp, "core", core_list);
        }
}

static void
monitor_xml_common_row(FILE *fp,
                       const char *timestamp,
                       const struct pqos_mon_data *mon_data,
                       const int region_aware)
{
        enum pqos_mon_event events = monitor_get_events();
        enum monitor_llc_format format = monitor_get_llc_format();
        unsigned i;

        ASSERT(fp != NULL);
        ASSERT(timestamp != NULL);
        ASSERT(mon_data != NULL);
        ASSERT(mon_data->context != NULL);

        fprintf(fp, "%s\n", xml_child_open);
        print_xml_text(fp, "time", timestamp);
        print_xml_context(fp, mon_data);

#ifdef PQOS_RMID_CUSTOM
        {
                enum pqos_interface iface;
                pqos_rmid_t rmid = 0;
                int ret = pqos_inter_get(&iface);
                const enum pqos_interface expected_iface =
                    region_aware ? PQOS_INTER_MMIO : PQOS_INTER_MSR;

                if (ret == PQOS_RETVAL_OK && iface == expected_iface) {
                        if (mon_data->num_cores > 0)
                                ret = pqos_mon_assoc_get(mon_data->cores[0],
                                                         &rmid);
                        else if (mon_data->num_channels > 0)
                                ret = pqos_mon_assoc_get_channel(
                                    mon_data->channels[0], &rmid);
                        else
                                ret = PQOS_RETVAL_ERROR;

                        print_xml_value(fp, "%.0f", (double)rmid,
                                        ret == PQOS_RETVAL_OK, 1, "rmid");
                }
        }
#endif

        for (i = 0; i < DIM(output); i++) {
                const char *node_name = get_node_name(i, format);

                if (region_aware && output[i].event == PQOS_MON_EVENT_TMEM_BW) {
                        int region_idx;

                        for (region_idx = 0;
                             region_idx < mon_data->regions.num_mem_regions;
                             region_idx++) {
                                char region_node[48];
                                const int region =
                                    mon_data->regions.region_num[region_idx];
                                double value = monitor_utils_get_region_value(
                                    mon_data, output[i].event, region);

                                snprintf(region_node, sizeof(region_node),
                                         "mbm_total_region_%d_MB", region);
                                print_xml_value(
                                    fp, output[i].format, value,
                                    mon_data->event & output[i].event,
                                    events & output[i].event, region_node);
                        }
                        continue;
                }

                {
                        double value = region_aware
                                           ? monitor_utils_get_region_value(
                                                 mon_data, output[i].event,
                                                 INVALID_REGION_NUM)
                                           : monitor_utils_get_value(
                                                 mon_data, output[i].event);

                        print_xml_value(fp, output[i].format, value,
                                        mon_data->event & output[i].event,
                                        events & output[i].event, node_name);
                }
        }

        fprintf(fp, "%s\n", xml_child_close);
}

void
monitor_xml_row(FILE *fp,
                const char *timestamp,
                const struct pqos_mon_data *mon_data)
{
        monitor_xml_common_row(fp, timestamp, mon_data, 0);
}

void
monitor_xml_region_row(FILE *fp,
                       const char *timestamp,
                       const struct pqos_mon_data *mon_data)
{
        monitor_xml_common_row(fp, timestamp, mon_data, 1);
}

void
monitor_xml_footer(FILE *fp)
{
        UNUSED_ARG(fp);
}

void
monitor_xml_end(FILE *fp)
{
        ASSERT(fp != NULL);

        fprintf(fp, "%s\n", xml_root_close);
}
