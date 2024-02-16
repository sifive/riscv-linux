// SPDX-License-Identifier: GPL-2.0
/*
 * SiFive Composable Cache PMU driver
 *
 * Copyright (C) 2022-2024 SiFive, Inc.
 * Copyright (C) Eric Lin <eric.lin@sifive.com>
 *
 */

#include <linux/cpuhotplug.h>
#include <linux/cpumask.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/perf_event.h>
#include <linux/platform_device.h>
#include <linux/property.h>

#define CCACHE_SELECT_OFFSET		0x2000
#define CCACHE_CLIENT_FILTER_OFFSET	0x2800
#define CCACHE_COUNTER_OFFSET		0x3000

#define CCACHE_PMU_MAX_COUNTERS		64

struct sifive_ccache_pmu {
	struct pmu			pmu;
	struct hlist_node		node;
	struct notifier_block		cpu_pm_nb;
	void __iomem			*base;
	DECLARE_BITMAP(used_mask, CCACHE_PMU_MAX_COUNTERS);
	unsigned int			cpu;
	int				n_counters;
	struct perf_event		*events[] __counted_by(n_counters);
};

#define to_ccache_pmu(p) (container_of(p, struct sifive_ccache_pmu, pmu))

#ifndef readq
static inline u64 readq(void __iomem *addr)
{
	return readl(addr) | (((u64)readl(addr + 4)) << 32);
}
#endif

#ifndef writeq
static inline void writeq(u64 v, void __iomem *addr)
{
	writel(lower_32_bits(v), addr);
	writel(upper_32_bits(v), addr + 4);
}
#endif

/*
 * sysfs attributes
 *
 * We export:
 * - cpumask, used by perf user space and other tools to know on which CPUs to create events
 * - events, used by perf user space and other tools to create events symbolically, e.g.:
 *     perf stat -a -e sifive_ccache_pmu/event=inner_put_partial_data_hit/ ls
 *     perf stat -a -e sifive_ccache_pmu/event=0x101/ ls
 * - formats, used by perf user space and other tools to configure events
 */

/* cpumask */
static ssize_t cpumask_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sifive_ccache_pmu *ccache_pmu = dev_get_drvdata(dev);

	if (ccache_pmu->cpu >= nr_cpu_ids)
		return 0;

	return sysfs_emit(buf, "%d\n", ccache_pmu->cpu);
};

static DEVICE_ATTR_RO(cpumask);

static struct attribute *sifive_ccache_pmu_cpumask_attrs[] = {
	&dev_attr_cpumask.attr,
	NULL,
};

static const struct attribute_group sifive_ccache_pmu_cpumask_group = {
	.attrs = sifive_ccache_pmu_cpumask_attrs,
};

/* events */
static ssize_t sifive_ccache_pmu_event_show(struct device *dev, struct device_attribute *attr,
					    char *page)
{
	struct perf_pmu_events_attr *pmu_attr;

	pmu_attr = container_of(attr, struct perf_pmu_events_attr, attr);
	return sysfs_emit(page, "event=0x%02llx\n", pmu_attr->id);
}

#define SET_EVENT_SELECT(_event, _set)	(BIT_ULL((_event) + 8) | (_set))
#define CCACHE_PMU_EVENT_ATTR(_name, _event, _set) \
	PMU_EVENT_ATTR_ID(_name, sifive_ccache_pmu_event_show, SET_EVENT_SELECT(_event, _set))

enum ccache_pmu_event_set1 {
	INNER_PUT_FULL_DATA = 0,
	INNER_PUT_PARTIAL_DATA,
	INNER_ATOMIC_DATA,
	INNER_GET,
	INNER_PREFETCH_READ,
	INNER_PREFETCH_WRITE,
	INNER_ACQUIRE_BLOCK_NTOB,
	INNER_ACQUIRE_BLOCK_NTOT,
	INNER_ACQUIRE_BLOCK_BTOT,
	INNER_ACQUIRE_PERM_NTOT,
	INNER_ACQUIRE_PERM_BTOT,
	INNER_RELEASE_TTOB,
	INNER_RELEASE_TTON,
	INNER_RELEASE_BTON,
	INNER_RELEASE_DATA_TTOB,
	INNER_RELEASE_DATA_TTON,
	INNER_RELEASE_DATA_BTON,
	OUTER_PROBE_BLOCK_TOT,
	OUTER_PROBE_BLOCK_TOB,
	OUTER_PROBE_BLOCK_TON,
	CCACHE_PMU_MAX_EVENT1_IDX
};

enum ccache_pmu_event_set2 {
	INNER_PUT_FULL_DATA_HIT = 0,
	INNER_PUT_PARTIAL_DATA_HIT,
	INNER_ATOMIC_DATA_HIT,
	INNER_GET_HIT,
	INNER_PREFETCH_HIT,
	INNER_ACQUIRE_BLOCK_HIT,
	INNER_ACQUIRE_PERM_HIT,
	INNER_RELEASE_HIT,
	INNER_RELEASE_DATA_HIT,
	OUTER_PROBE_HIT,
	INNER_PUT_FULL_DATA_HIT_SHARED,
	INNER_PUT_PARTIAL_DATA_HIT_SHARED,
	INNER_ATOMIC_DATA_HIT_SHARED,
	INNER_GET_HIT_SHARED,
	INNER_PREFETCH_HIT_SHARED,
	INNER_ACQUIRE_BLOCK_HIT_SHARED,
	INNER_ACQUIRE_PERM_HIT_SHARED,
	OUTER_PROBE_HIT_SHARED,
	OUTER_PROBE_HIT_DIRTY,
	CCACHE_PMU_MAX_EVENT2_IDX
};

enum ccache_pmu_event_set3 {
	OUTER_ACQUIRE_BLOCK_NTOB_MISS = 0,
	OUTER_ACQUIRE_BLOCK_NTOT_MISS,
	OUTER_ACQUIRE_BLOCK_BTOT_MISS,
	OUTER_ACQUIRE_PERM_NTOT_MISS,
	OUTER_ACQUIRE_PERM_BTOT_MISS,
	OUTER_RELEASE_TTOB_EVICTION,
	OUTER_RELEASE_TTON_EVICTION,
	OUTER_RELEASE_BTON_EVICTION,
	OUTER_RELEASE_DATA_TTOB_NOT_APPLICABLE,
	OUTER_RELEASE_DATA_TTON_DIRTY_EVICTION,
	OUTER_RELEASE_DATA_BTON_NOT_APPLICABLE,
	INNER_PROBE_BLOCK_TOT_CODE_MISS_HITS_OTHER_HARTS,
	INNER_PROBE_BLOCK_TOB_LOAD_MISS_HITS_OTHER_HARTS,
	INNER_PROBE_BLOCK_TON_STORE_MISS_HITS_OTHER_HARTS,
	CCACHE_PMU_MAX_EVENT3_IDX
};

enum ccache_pmu_event_set4 {
	INNER_HINT_HITS_INFLIGHT_MISS = 0,
	CCACHE_PMU_MAX_EVENT4_IDX
};

static struct attribute *sifive_ccache_pmu_events[] = {
	/*  pmEventSelect1 */
	CCACHE_PMU_EVENT_ATTR(inner_put_full_data, INNER_PUT_FULL_DATA, 1),
	CCACHE_PMU_EVENT_ATTR(inner_put_partial_data, INNER_PUT_PARTIAL_DATA, 1),
	CCACHE_PMU_EVENT_ATTR(inner_atomic_data, INNER_ATOMIC_DATA, 1),
	CCACHE_PMU_EVENT_ATTR(inner_get, INNER_GET, 1),
	CCACHE_PMU_EVENT_ATTR(inner_prefetch_read, INNER_PREFETCH_READ, 1),
	CCACHE_PMU_EVENT_ATTR(inner_prefetch_write, INNER_PREFETCH_WRITE, 1),
	CCACHE_PMU_EVENT_ATTR(inner_acquire_block_ntob, INNER_ACQUIRE_BLOCK_NTOB, 1),
	CCACHE_PMU_EVENT_ATTR(inner_acquire_block_ntot, INNER_ACQUIRE_BLOCK_NTOT, 1),
	CCACHE_PMU_EVENT_ATTR(inner_acquire_block_btot, INNER_ACQUIRE_BLOCK_BTOT, 1),
	CCACHE_PMU_EVENT_ATTR(inner_acquire_perm_ntot, INNER_ACQUIRE_PERM_NTOT, 1),
	CCACHE_PMU_EVENT_ATTR(inner_acquire_perm_btot, INNER_ACQUIRE_PERM_BTOT, 1),
	CCACHE_PMU_EVENT_ATTR(inner_release_ttob, INNER_RELEASE_TTOB, 1),
	CCACHE_PMU_EVENT_ATTR(inner_release_tton, INNER_RELEASE_TTON, 1),
	CCACHE_PMU_EVENT_ATTR(inner_release_bton, INNER_RELEASE_BTON, 1),
	CCACHE_PMU_EVENT_ATTR(inner_release_data_ttob, INNER_RELEASE_DATA_TTOB, 1),
	CCACHE_PMU_EVENT_ATTR(inner_release_data_tton, INNER_RELEASE_DATA_TTON, 1),
	CCACHE_PMU_EVENT_ATTR(inner_release_data_bton, INNER_RELEASE_DATA_BTON, 1),
	CCACHE_PMU_EVENT_ATTR(outer_probe_block_tot, OUTER_PROBE_BLOCK_TOT, 1),
	CCACHE_PMU_EVENT_ATTR(outer_probe_block_tob, OUTER_PROBE_BLOCK_TOB, 1),
	CCACHE_PMU_EVENT_ATTR(outer_probe_block_ton, OUTER_PROBE_BLOCK_TON, 1),

	/*  pmEventSelect2 */
	CCACHE_PMU_EVENT_ATTR(inner_put_full_data_hit, INNER_PUT_FULL_DATA_HIT, 2),
	CCACHE_PMU_EVENT_ATTR(inner_put_partial_data_hit, INNER_PUT_PARTIAL_DATA_HIT, 2),
	CCACHE_PMU_EVENT_ATTR(inner_atomic_data_hit, INNER_ATOMIC_DATA_HIT, 2),
	CCACHE_PMU_EVENT_ATTR(inner_get_hit, INNER_GET_HIT, 2),
	CCACHE_PMU_EVENT_ATTR(inner_prefetch_hit, INNER_PREFETCH_HIT, 2),
	CCACHE_PMU_EVENT_ATTR(inner_acquire_block_hit, INNER_ACQUIRE_BLOCK_HIT, 2),
	CCACHE_PMU_EVENT_ATTR(inner_acquire_perm_hit, INNER_ACQUIRE_PERM_HIT, 2),
	CCACHE_PMU_EVENT_ATTR(inner_release_hit, INNER_RELEASE_HIT, 2),
	CCACHE_PMU_EVENT_ATTR(inner_release_data_hit, INNER_RELEASE_DATA_HIT, 2),
	CCACHE_PMU_EVENT_ATTR(outer_probe_hit, OUTER_PROBE_HIT, 2),
	CCACHE_PMU_EVENT_ATTR(inner_put_full_data_hit_shared, INNER_PUT_FULL_DATA_HIT_SHARED, 2),
	CCACHE_PMU_EVENT_ATTR(inner_put_partial_data_hit_shared,
			      INNER_PUT_PARTIAL_DATA_HIT_SHARED, 2),
	CCACHE_PMU_EVENT_ATTR(inner_atomic_data_hit_shared, INNER_ATOMIC_DATA_HIT_SHARED, 2),
	CCACHE_PMU_EVENT_ATTR(inner_get_hit_shared, INNER_GET_HIT_SHARED, 2),
	CCACHE_PMU_EVENT_ATTR(inner_prefetch_hit_shared, INNER_PREFETCH_HIT_SHARED, 2),
	CCACHE_PMU_EVENT_ATTR(inner_acquire_block_hit_shared, INNER_ACQUIRE_BLOCK_HIT_SHARED, 2),
	CCACHE_PMU_EVENT_ATTR(inner_acquire_perm_hit_shared, INNER_ACQUIRE_PERM_HIT_SHARED, 2),
	CCACHE_PMU_EVENT_ATTR(outer_probe_hit_shared, OUTER_PROBE_HIT_SHARED, 2),
	CCACHE_PMU_EVENT_ATTR(outer_probe_hit_dirty, OUTER_PROBE_HIT_DIRTY, 2),

	/*  pmEventSelect3 */
	CCACHE_PMU_EVENT_ATTR(outer_acquire_block_ntob_miss, OUTER_ACQUIRE_BLOCK_NTOB_MISS, 3),
	CCACHE_PMU_EVENT_ATTR(outer_acquire_block_ntot_miss, OUTER_ACQUIRE_BLOCK_NTOT_MISS, 3),
	CCACHE_PMU_EVENT_ATTR(outer_acquire_block_btot_miss, OUTER_ACQUIRE_BLOCK_BTOT_MISS, 3),
	CCACHE_PMU_EVENT_ATTR(outer_acquire_perm_ntot_miss, OUTER_ACQUIRE_PERM_NTOT_MISS, 3),
	CCACHE_PMU_EVENT_ATTR(outer_acquire_perm_btot_miss, OUTER_ACQUIRE_PERM_BTOT_MISS, 3),
	CCACHE_PMU_EVENT_ATTR(outer_release_ttob_eviction, OUTER_RELEASE_TTOB_EVICTION, 3),
	CCACHE_PMU_EVENT_ATTR(outer_release_tton_eviction, OUTER_RELEASE_TTON_EVICTION, 3),
	CCACHE_PMU_EVENT_ATTR(outer_release_bton_eviction, OUTER_RELEASE_BTON_EVICTION, 3),
	CCACHE_PMU_EVENT_ATTR(outer_release_data_ttob_not_applicable,
			      OUTER_RELEASE_DATA_TTOB_NOT_APPLICABLE, 3),
	CCACHE_PMU_EVENT_ATTR(outer_release_data_tton_dirty_eviction,
			      OUTER_RELEASE_DATA_TTON_DIRTY_EVICTION, 3),
	CCACHE_PMU_EVENT_ATTR(outer_release_data_bton_not_applicable,
			      OUTER_RELEASE_DATA_BTON_NOT_APPLICABLE, 3),
	CCACHE_PMU_EVENT_ATTR(inner_probe_block_tot_code_miss_hits_other_harts,
			      INNER_PROBE_BLOCK_TOT_CODE_MISS_HITS_OTHER_HARTS, 3),
	CCACHE_PMU_EVENT_ATTR(inner_probe_block_tob_load_miss_hits_other_harts,
			      INNER_PROBE_BLOCK_TOB_LOAD_MISS_HITS_OTHER_HARTS, 3),
	CCACHE_PMU_EVENT_ATTR(inner_probe_block_ton_store_miss_hits_other_harts,
			      INNER_PROBE_BLOCK_TON_STORE_MISS_HITS_OTHER_HARTS, 3),

	/*  pm_event_select4 */
	CCACHE_PMU_EVENT_ATTR(inner_hint_hits_inflight_miss, INNER_HINT_HITS_INFLIGHT_MISS, 4),
	NULL
};

static struct attribute_group sifive_ccache_pmu_events_group = {
	.name = "events",
	.attrs = sifive_ccache_pmu_events,
};

/* formats */
PMU_FORMAT_ATTR(event, "config:0-63");

static struct attribute *sifive_ccache_pmu_formats[] = {
	&format_attr_event.attr,
	NULL,
};

static struct attribute_group sifive_ccache_pmu_format_group = {
	.name = "format",
	.attrs = sifive_ccache_pmu_formats,
};

/*
 * Per PMU device attribute groups
 */

static const struct attribute_group *sifive_ccache_pmu_attr_grps[] = {
	&sifive_ccache_pmu_cpumask_group,
	&sifive_ccache_pmu_events_group,
	&sifive_ccache_pmu_format_group,
	NULL,
};

/*
 * Event Initialization
 */

static int sifive_ccache_pmu_event_init(struct perf_event *event)
{
	struct sifive_ccache_pmu *ccache_pmu = to_ccache_pmu(event->pmu);
	struct hw_perf_event *hwc = &event->hw;
	u64 config = event->attr.config;
	u64 ev_type = config >> 8;
	u64 set = config & 0xff;

	/* Check if this is a valid set and event */
	switch (set) {
	case 1:
		if (ev_type >= BIT_ULL(CCACHE_PMU_MAX_EVENT1_IDX))
			return -ENOENT;
		break;
	case 2:
		if (ev_type >= BIT_ULL(CCACHE_PMU_MAX_EVENT2_IDX))
			return -ENOENT;
		break;
	case 3:
		if (ev_type >= BIT_ULL(CCACHE_PMU_MAX_EVENT3_IDX))
			return -ENOENT;
		break;
	case 4:
		if (ev_type >= BIT_ULL(CCACHE_PMU_MAX_EVENT4_IDX))
			return -ENOENT;
		break;
	default:
		return -ENOENT;
	}

	/* Do not allocate the hardware counter yet */
	hwc->idx = -1;
	hwc->config = config;

	event->cpu = ccache_pmu->cpu;

	return 0;
}

/*
 * pmu->read: read and update the counter
 */
static void sifive_ccache_pmu_read(struct perf_event *event)
{
	struct hw_perf_event *hwc = &event->hw;
	u64 prev_raw_count, new_raw_count;
	u64 oldval;

	do {
		prev_raw_count = local64_read(&hwc->prev_count);
		new_raw_count = readq((void *)hwc->event_base);

		oldval = local64_cmpxchg(&hwc->prev_count, prev_raw_count, new_raw_count);
	} while (oldval != prev_raw_count);

	local64_add(new_raw_count - prev_raw_count, &event->count);
}

/*
 * State transition functions:
 *
 * start()/stop() & add()/del()
 */

/*
 * pmu->start: start the event
 */
static void sifive_ccache_pmu_start(struct perf_event *event, int flags)
{
	struct hw_perf_event *hwc = &event->hw;

	if (WARN_ON_ONCE(!(hwc->state & PERF_HES_STOPPED)))
		return;

	hwc->state = 0;

	/* Set initial value to 0 */
	local64_set(&hwc->prev_count, 0);
	writeq(0, (void *)hwc->event_base);

	/* Enable this counter to count events */
	writeq(hwc->config, (void *)hwc->config_base);
}

/*
 * pmu->stop: stop the counter
 */
static void sifive_ccache_pmu_stop(struct perf_event *event, int flags)
{
	struct hw_perf_event *hwc = &event->hw;

	if (hwc->state & PERF_HES_STOPPED)
		return;

	/* Disable this counter to count events */
	writeq(0, (void *)hwc->config_base);
	sifive_ccache_pmu_read(event);

	hwc->state = PERF_HES_STOPPED | PERF_HES_UPTODATE;
}

/*
 * pmu->add: add the event to the PMU
 */
static int sifive_ccache_pmu_add(struct perf_event *event, int flags)
{
	struct sifive_ccache_pmu *ccache_pmu = to_ccache_pmu(event->pmu);
	struct hw_perf_event *hwc = &event->hw;
	int idx;

	/* Find an available counter idx to use for this event */
	do {
		idx = find_first_zero_bit(ccache_pmu->used_mask, ccache_pmu->n_counters);
		if (idx >= ccache_pmu->n_counters)
			return -EAGAIN;
	} while (test_and_set_bit(idx, ccache_pmu->used_mask));

	hwc->config_base = (unsigned long)ccache_pmu->base + CCACHE_SELECT_OFFSET + 8 * idx;
	hwc->event_base = (unsigned long)ccache_pmu->base + CCACHE_COUNTER_OFFSET + 8 * idx;
	hwc->idx = idx;
	hwc->state = PERF_HES_STOPPED | PERF_HES_UPTODATE;

	ccache_pmu->events[idx] = event;

	if (flags & PERF_EF_START)
		sifive_ccache_pmu_start(event, PERF_EF_RELOAD);

	perf_event_update_userpage(event);

	return 0;
}

/*
 * pmu->del: delete the event from the PMU
 */
static void sifive_ccache_pmu_del(struct perf_event *event, int flags)
{
	struct sifive_ccache_pmu *ccache_pmu = to_ccache_pmu(event->pmu);
	struct hw_perf_event *hwc = &event->hw;
	int idx = hwc->idx;

	/* Stop and release this counter */
	sifive_ccache_pmu_stop(event, PERF_EF_UPDATE);

	ccache_pmu->events[idx] = NULL;
	clear_bit(idx, ccache_pmu->used_mask);

	perf_event_update_userpage(event);
}

/*
 * Driver initialization
 */

static void sifive_ccache_pmu_hw_init(const struct sifive_ccache_pmu *ccache_pmu)
{
	/* Disable the client filter (not supported by this driver) */
	writeq(0, ccache_pmu->base + CCACHE_CLIENT_FILTER_OFFSET);
}

static int sifive_ccache_pmu_online_cpu(unsigned int cpu, struct hlist_node *node)
{
	struct sifive_ccache_pmu *ccache_pmu =
		hlist_entry_safe(node, struct sifive_ccache_pmu, node);

	if (ccache_pmu->cpu >= nr_cpu_ids)
		ccache_pmu->cpu = cpu;

	return 0;
}

static int sifive_ccache_pmu_offline_cpu(unsigned int cpu, struct hlist_node *node)
{
	struct sifive_ccache_pmu *ccache_pmu =
		hlist_entry_safe(node, struct sifive_ccache_pmu, node);

	/* Do nothing if this CPU does not own the events */
	if (cpu != ccache_pmu->cpu)
		return 0;

	/* Pick a random online CPU */
	ccache_pmu->cpu = cpumask_any_but(cpu_online_mask, cpu);
	if (ccache_pmu->cpu >= nr_cpu_ids)
		return 0;

	/* Migrate PMU events from this CPU to the target CPU */
	perf_pmu_migrate_context(&ccache_pmu->pmu, cpu, ccache_pmu->cpu);

	return 0;
}

static int sifive_ccache_pmu_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct sifive_ccache_pmu *ccache_pmu;
	u32 n_counters;
	int ret;

	/* Instances without a sifive,perfmon-counters property do not contain a PMU */
	ret = device_property_read_u32(dev, "sifive,perfmon-counters", &n_counters);
	if (ret || !n_counters)
		return -ENODEV;

	ccache_pmu = devm_kzalloc(dev, struct_size(ccache_pmu, events, n_counters), GFP_KERNEL);
	if (!ccache_pmu)
		return -ENOMEM;

	platform_set_drvdata(pdev, ccache_pmu);

	ccache_pmu->pmu = (struct pmu) {
		.parent		= dev,
		.attr_groups	= sifive_ccache_pmu_attr_grps,
		.capabilities	= PERF_PMU_CAP_NO_EXCLUDE | PERF_PMU_CAP_NO_INTERRUPT,
		.task_ctx_nr	= perf_invalid_context,
		.event_init	= sifive_ccache_pmu_event_init,
		.add		= sifive_ccache_pmu_add,
		.del		= sifive_ccache_pmu_del,
		.start		= sifive_ccache_pmu_start,
		.stop		= sifive_ccache_pmu_stop,
		.read		= sifive_ccache_pmu_read,
	};
	ccache_pmu->cpu = nr_cpu_ids;
	ccache_pmu->n_counters = n_counters;

	ccache_pmu->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(ccache_pmu->base))
		return PTR_ERR(ccache_pmu->base);

	sifive_ccache_pmu_hw_init(ccache_pmu);

	ret = cpuhp_state_add_instance(CPUHP_AP_PERF_RISCV_SIFIVE_CCACHE_ONLINE, &ccache_pmu->node);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to add CPU hotplug instance\n");

	ret = perf_pmu_register(&ccache_pmu->pmu, "sifive_ccache_pmu", -1);
	if (ret) {
		dev_err_probe(dev, ret, "Failed to register PMU\n");
		goto err_remove_instance;
	}

	return 0;

err_remove_instance:
	cpuhp_state_remove_instance(CPUHP_AP_PERF_RISCV_SIFIVE_CCACHE_ONLINE, &ccache_pmu->node);

	return ret;
}

static void sifive_ccache_pmu_remove(struct platform_device *pdev)
{
	struct sifive_ccache_pmu *ccache_pmu = platform_get_drvdata(pdev);

	perf_pmu_unregister(&ccache_pmu->pmu);
	cpuhp_state_remove_instance(CPUHP_AP_PERF_RISCV_SIFIVE_CCACHE_ONLINE, &ccache_pmu->node);
}

static const struct of_device_id sifive_ccache_pmu_of_match[] = {
	{ .compatible = "sifive,ccache0" },
	{}
};
MODULE_DEVICE_TABLE(of, sifive_ccache_pmu_of_match);

static struct platform_driver sifive_ccache_pmu_driver = {
	.probe	= sifive_ccache_pmu_probe,
	.remove_new	= sifive_ccache_pmu_remove,
	.driver	= {
		.name		= "sifive_ccache_pmu",
		.of_match_table	= sifive_ccache_pmu_of_match,
	},
};

static void __exit sifive_ccache_pmu_exit(void)
{
	platform_driver_unregister(&sifive_ccache_pmu_driver);
	cpuhp_remove_multi_state(CPUHP_AP_PERF_RISCV_SIFIVE_CCACHE_ONLINE);
}
module_exit(sifive_ccache_pmu_exit);

static int __init sifive_ccache_pmu_init(void)
{
	int ret;

	ret = cpuhp_setup_state_multi(CPUHP_AP_PERF_RISCV_SIFIVE_CCACHE_ONLINE,
				      "perf/sifive/ccache:online",
				      sifive_ccache_pmu_online_cpu,
				      sifive_ccache_pmu_offline_cpu);
	if (ret)
		return ret;

	ret = platform_driver_register(&sifive_ccache_pmu_driver);
	if (ret)
		goto err_remove_state;

	return 0;

err_remove_state:
	cpuhp_remove_multi_state(CPUHP_AP_PERF_RISCV_SIFIVE_CCACHE_ONLINE);

	return ret;
}
module_init(sifive_ccache_pmu_init);

MODULE_LICENSE("GPL");
