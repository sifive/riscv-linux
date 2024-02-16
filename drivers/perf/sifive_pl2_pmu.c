// SPDX-License-Identifier: GPL-2.0
/*
 * SiFive Private L2 Cache PMU driver
 *
 * Copyright (C) 2018-2024 SiFive, Inc.
 */

#include <linux/cpu_pm.h>
#include <linux/cpumask.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/perf_event.h>
#include <linux/platform_device.h>
#include <linux/refcount.h>

#define PL2_SELECT_OFFSET		0x2000
#define PL2_CLIENT_FILTER_OFFSET	0x2800
#define PL2_COUNTER_OFFSET		0x3000

#define PL2_PMU_MAX_COUNTERS		64

struct sifive_pl2_pmu_event {
	void __iomem			*base;
	DECLARE_BITMAP(used_mask, PL2_PMU_MAX_COUNTERS);
	unsigned int			cpu;
	int				n_counters;
	struct perf_event		*events[] __counted_by(n_counters);
};

struct sifive_pl2_pmu {
	struct pmu			pmu;
	struct notifier_block		cpu_pm_nb;
	refcount_t			refcount;
	struct sifive_pl2_pmu_event	*__percpu *event;
};

#define to_pl2_pmu(p) (container_of(p, struct sifive_pl2_pmu, pmu))

static DEFINE_MUTEX(g_mutex);
static struct sifive_pl2_pmu *g_pl2_pmu;

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
 * - events, used by perf user space and other tools to create events symbolically, e.g.:
 *     perf stat -a -e sifive_pl2_pmu/event=inner_put_partial_data_hit/ ls
 *     perf stat -a -e sifive_pl2_pmu/event=0x101/ ls
 * - formats, used by perf user space and other tools to configure events
 */

/* events */
static ssize_t sifive_pl2_pmu_event_show(struct device *dev, struct device_attribute *attr,
					 char *page)
{
	struct perf_pmu_events_attr *pmu_attr;

	pmu_attr = container_of(attr, struct perf_pmu_events_attr, attr);
	return sysfs_emit(page, "event=0x%02llx\n", pmu_attr->id);
}

#define SET_EVENT_SELECT(_event, _set)	(BIT_ULL((_event) + 8) | (_set))
#define PL2_PMU_EVENT_ATTR(_name, _event, _set) \
	PMU_EVENT_ATTR_ID(_name, sifive_pl2_pmu_event_show, SET_EVENT_SELECT(_event, _set))

enum pl2_pmu_event_set1 {
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
	INNER_RELEASE_DATA_TTOT,
	INNER_PROBE_BLOCK_TOT,
	INNER_PROBE_BLOCK_TOB,
	INNER_PROBE_BLOCK_TON,
	INNER_PROBE_PERM_TON,
	INNER_PROBE_ACK_TTOB,
	INNER_PROBE_ACK_TTON,
	INNER_PROBE_ACK_BTON,
	INNER_PROBE_ACK_TTOT,
	INNER_PROBE_ACK_BTOB,
	INNER_PROBE_ACK_NTON,
	INNER_PROBE_ACK_DATA_TTOB,
	INNER_PROBE_ACK_DATA_TTON,
	INNER_PROBE_ACK_DATA_TTOT,
	PL2_PMU_MAX_EVENT1_IDX
};

enum pl2_pmu_event_set2 {
	INNER_PUT_FULL_DATA_HIT = 0,
	INNER_PUT_PARTIAL_DATA_HIT,
	INNER_ATOMIC_DATA_HIT,
	INNER_GET_HIT,
	INNER_PREFETCH_READ_HIT,
	INNER_ACQUIRE_BLOCK_NTOB_HIT,
	INNER_ACQUIRE_PERM_NTOT_HIT,
	INNER_RELEASE_TTOB_HIT,
	INNER_RELEASE_DATA_TTOB_HIT,
	OUTER_PROBE_BLOCK_TOT_HIT,
	INNER_PUT_FULL_DATA_HIT_SHARED,
	INNER_PUT_PARTIAL_DATA_HIT_SHARED,
	INNER_ATOMIC_DATA_HIT_SHARED,
	INNER_GET_HIT_SHARED,
	INNER_PREFETCH_READ_HIT_SHARED,
	INNER_ACQUIRE_BLOCK_NTOB_HIT_SHARED,
	INNER_ACQUIRE_PERM_NTOT_HIT_SHARED,
	INNER_RELEASE_TTOB_HIT_SHARED,
	INNER_RELEASE_DATA_TTOB_HIT_SHARED,
	OUTER_PROBE_BLOCK_TOT_HIT_SHARED,
	OUTER_PROBE_BLOCK_TOT_HIT_DIRTY,
	PL2_PMU_MAX_EVENT2_IDX
};

enum pl2_pmu_event_set3 {
	OUTER_PUT_FULL_DATA = 0,
	OUTER_PUT_PARTIAL_DATA,
	OUTER_ATOMIC_DATA,
	OUTER_GET,
	OUTER_PREFETCH_READ,
	OUTER_PREFETCH_WRITE,
	OUTER_ACQUIRE_BLOCK_NTOB,
	OUTER_ACQUIRE_BLOCK_NTOT,
	OUTER_ACQUIRE_BLOCK_BTOT,
	OUTER_ACQUIRE_PERM_NTOT,
	OUTER_ACQUIRE_PERM_BTOT,
	OUTER_RELEARE_TTOB,
	OUTER_RELEARE_TTON,
	OUTER_RELEARE_BTON,
	OUTER_RELEARE_DATA_TTOB,
	OUTER_RELEARE_DATA_TTON,
	OUTER_RELEARE_DATA_BTON,
	OUTER_RELEARE_DATA_TTOT,
	OUTER_PROBE_BLOCK_TOT,
	OUTER_PROBE_BLOCK_TOB,
	OUTER_PROBE_BLOCK_TON,
	OUTER_PROBE_PERM_TON,
	OUTER_PROBE_ACK_TTOB,
	OUTER_PROBE_ACK_TTON,
	OUTER_PROBE_ACK_BTON,
	OUTER_PROBE_ACK_TTOT,
	OUTER_PROBE_ACK_BTOB,
	OUTER_PROBE_ACK_NTON,
	OUTER_PROBE_ACK_DATA_TTOB,
	OUTER_PROBE_ACK_DATA_TTON,
	OUTER_PROBE_ACK_DATA_TTOT,
	PL2_PMU_MAX_EVENT3_IDX
};

enum pl2_pmu_event_set4 {
	INNER_HINT_HITS_MSHR = 0,
	INNER_READ_HITS_MSHR,
	INNER_WRITE_HITS_MSHR,
	INNER_READ_REPLAY,
	INNER_WRITE_REPLAY,
	OUTER_PROBE_REPLAY,
	REPLAY,
	SLEEP_BY_MISS_QUEUE,
	SLEEP_BY_EVICT_QUEUE,
	SLEEP_FOR_BACK_PROBE,
	SLEEP,
	PL2_PMU_MAX_EVENT4_IDX
};

enum pl2_pmu_event_set5 {
	READ_SLEEP_TIMER_EXPIRE = 0,
	READ_OLDEST_TIMER_EXPIRE,
	WRITE_SLEEP_TIMER_EXPIRE,
	WRITE_OLDEST_TIMER_EXPIRE,
	READ_SLEEP,
	READ_DIR_UPDATE_WAKEUP,
	READ_MISS_QUEUE_WAKEUP,
	READ_EVICT_QUEUE_WAKEUP,
	READ_SLEEP_TIMER_WAKEUP,
	WRITE_SLEEP,
	WRITE_DIR_UPDATE_WAKEUP,
	WRITE_MISS_QUEUE_WAKEUP,
	WRITE_EVICT_QUEUE_WAKEUP,
	WRITE_SLEEP_TIMER_WAKEUP,
	PL2_PMU_MAX_EVENT5_IDX
};

static struct attribute *sifive_pl2_pmu_events[] = {
	PL2_PMU_EVENT_ATTR(inner_put_full_data, INNER_PUT_FULL_DATA, 1),
	PL2_PMU_EVENT_ATTR(inner_put_partial_data, INNER_PUT_PARTIAL_DATA, 1),
	PL2_PMU_EVENT_ATTR(inner_atomic_data, INNER_ATOMIC_DATA, 1),
	PL2_PMU_EVENT_ATTR(inner_get, INNER_GET, 1),
	PL2_PMU_EVENT_ATTR(inner_prefetch_read, INNER_PREFETCH_READ, 1),
	PL2_PMU_EVENT_ATTR(inner_prefetch_write, INNER_PREFETCH_WRITE, 1),
	PL2_PMU_EVENT_ATTR(inner_acquire_block_ntob, INNER_ACQUIRE_BLOCK_NTOB, 1),
	PL2_PMU_EVENT_ATTR(inner_acquire_block_ntot, INNER_ACQUIRE_BLOCK_NTOT, 1),
	PL2_PMU_EVENT_ATTR(inner_acquire_block_btot, INNER_ACQUIRE_BLOCK_BTOT, 1),
	PL2_PMU_EVENT_ATTR(inner_acquire_perm_ntot, INNER_ACQUIRE_PERM_NTOT, 1),
	PL2_PMU_EVENT_ATTR(inner_acquire_perm_btot, INNER_ACQUIRE_PERM_BTOT, 1),
	PL2_PMU_EVENT_ATTR(inner_release_ttob, INNER_RELEASE_TTOB, 1),
	PL2_PMU_EVENT_ATTR(inner_release_tton, INNER_RELEASE_TTON, 1),
	PL2_PMU_EVENT_ATTR(inner_release_bton, INNER_RELEASE_BTON, 1),
	PL2_PMU_EVENT_ATTR(inner_release_data_ttob, INNER_RELEASE_DATA_TTOB, 1),
	PL2_PMU_EVENT_ATTR(inner_release_data_tton, INNER_RELEASE_DATA_TTON, 1),
	PL2_PMU_EVENT_ATTR(inner_release_data_bton, INNER_RELEASE_DATA_BTON, 1),
	PL2_PMU_EVENT_ATTR(inner_release_data_ttot, INNER_RELEASE_DATA_TTOT, 1),
	PL2_PMU_EVENT_ATTR(inner_probe_block_tot, INNER_PROBE_BLOCK_TOT, 1),
	PL2_PMU_EVENT_ATTR(inner_probe_block_tob, INNER_PROBE_BLOCK_TOB, 1),
	PL2_PMU_EVENT_ATTR(inner_probe_block_ton, INNER_PROBE_BLOCK_TON, 1),
	PL2_PMU_EVENT_ATTR(inner_probe_perm_ton, INNER_PROBE_PERM_TON, 1),
	PL2_PMU_EVENT_ATTR(inner_probe_ack_ttob, INNER_PROBE_ACK_TTOB, 1),
	PL2_PMU_EVENT_ATTR(inner_probe_ack_tton, INNER_PROBE_ACK_TTON, 1),
	PL2_PMU_EVENT_ATTR(inner_probe_ack_bton, INNER_PROBE_ACK_BTON, 1),
	PL2_PMU_EVENT_ATTR(inner_probe_ack_ttot, INNER_PROBE_ACK_TTOT, 1),
	PL2_PMU_EVENT_ATTR(inner_probe_ack_btob, INNER_PROBE_ACK_BTOB, 1),
	PL2_PMU_EVENT_ATTR(inner_probe_ack_nton, INNER_PROBE_ACK_NTON, 1),
	PL2_PMU_EVENT_ATTR(inner_probe_ack_data_ttob, INNER_PROBE_ACK_DATA_TTOB, 1),
	PL2_PMU_EVENT_ATTR(inner_probe_ack_data_tton, INNER_PROBE_ACK_DATA_TTON, 1),
	PL2_PMU_EVENT_ATTR(inner_probe_ack_data_ttot, INNER_PROBE_ACK_DATA_TTOT, 1),

	PL2_PMU_EVENT_ATTR(inner_put_full_data_hit, INNER_PUT_FULL_DATA_HIT, 2),
	PL2_PMU_EVENT_ATTR(inner_put_partial_data_hit, INNER_PUT_PARTIAL_DATA_HIT, 2),
	PL2_PMU_EVENT_ATTR(inner_atomic_data_hit, INNER_ATOMIC_DATA_HIT, 2),
	PL2_PMU_EVENT_ATTR(inner_get_hit, INNER_GET_HIT, 2),
	PL2_PMU_EVENT_ATTR(inner_prefetch_read_hit, INNER_PREFETCH_READ_HIT, 2),
	PL2_PMU_EVENT_ATTR(inner_acquire_block_ntob_hit, INNER_ACQUIRE_BLOCK_NTOB_HIT, 2),
	PL2_PMU_EVENT_ATTR(inner_acquire_perm_ntot_hit, INNER_ACQUIRE_PERM_NTOT_HIT, 2),
	PL2_PMU_EVENT_ATTR(inner_release_ttob_hit, INNER_RELEASE_TTOB_HIT, 2),
	PL2_PMU_EVENT_ATTR(inner_release_data_ttob_hit, INNER_RELEASE_DATA_TTOB_HIT, 2),
	PL2_PMU_EVENT_ATTR(outer_probe_block_tot_hit, OUTER_PROBE_BLOCK_TOT_HIT, 2),
	PL2_PMU_EVENT_ATTR(inner_put_full_data_hit_shared, INNER_PUT_FULL_DATA_HIT_SHARED, 2),
	PL2_PMU_EVENT_ATTR(inner_put_partial_data_hit_shared, INNER_PUT_PARTIAL_DATA_HIT_SHARED, 2),
	PL2_PMU_EVENT_ATTR(inner_atomic_data_hit_shared, INNER_ATOMIC_DATA_HIT_SHARED, 2),
	PL2_PMU_EVENT_ATTR(inner_get_hit_shared, INNER_GET_HIT_SHARED, 2),
	PL2_PMU_EVENT_ATTR(inner_prefetch_read_hit_shared, INNER_PREFETCH_READ_HIT_SHARED, 2),
	PL2_PMU_EVENT_ATTR(inner_acquire_block_ntob_hit_shared,
			   INNER_ACQUIRE_BLOCK_NTOB_HIT_SHARED, 2),
	PL2_PMU_EVENT_ATTR(inner_acquire_perm_ntot_hit_shared,
			   INNER_ACQUIRE_PERM_NTOT_HIT_SHARED, 2),
	PL2_PMU_EVENT_ATTR(inner_release_ttob_hit_shared, INNER_RELEASE_TTOB_HIT_SHARED, 2),
	PL2_PMU_EVENT_ATTR(inner_release_data_ttob_hit_shared,
			   INNER_RELEASE_DATA_TTOB_HIT_SHARED, 2),
	PL2_PMU_EVENT_ATTR(outer_probe_block_tot_hit_shared, OUTER_PROBE_BLOCK_TOT_HIT_SHARED, 2),
	PL2_PMU_EVENT_ATTR(outer_probe_block_tot_hit_dirty, OUTER_PROBE_BLOCK_TOT_HIT_DIRTY, 2),

	PL2_PMU_EVENT_ATTR(outer_put_full_data, OUTER_PUT_FULL_DATA, 3),
	PL2_PMU_EVENT_ATTR(outer_put_partial_data, OUTER_PUT_PARTIAL_DATA, 3),
	PL2_PMU_EVENT_ATTR(outer_atomic_data, OUTER_ATOMIC_DATA, 3),
	PL2_PMU_EVENT_ATTR(outer_get, OUTER_GET, 3),
	PL2_PMU_EVENT_ATTR(outer_prefetch_read, OUTER_PREFETCH_READ, 3),
	PL2_PMU_EVENT_ATTR(outer_prefetch_write, OUTER_PREFETCH_WRITE, 3),
	PL2_PMU_EVENT_ATTR(outer_acquire_block_ntob, OUTER_ACQUIRE_BLOCK_NTOB, 3),
	PL2_PMU_EVENT_ATTR(outer_acquire_block_ntot, OUTER_ACQUIRE_BLOCK_NTOT, 3),
	PL2_PMU_EVENT_ATTR(outer_acquire_block_btot, OUTER_ACQUIRE_BLOCK_BTOT, 3),
	PL2_PMU_EVENT_ATTR(outer_acquire_perm_ntot, OUTER_ACQUIRE_PERM_NTOT, 3),
	PL2_PMU_EVENT_ATTR(outer_acquire_perm_btot, OUTER_ACQUIRE_PERM_BTOT, 3),
	PL2_PMU_EVENT_ATTR(outer_release_ttob, OUTER_RELEARE_TTOB, 3),
	PL2_PMU_EVENT_ATTR(outer_release_tton, OUTER_RELEARE_TTON, 3),
	PL2_PMU_EVENT_ATTR(outer_release_bton, OUTER_RELEARE_BTON, 3),
	PL2_PMU_EVENT_ATTR(outer_release_data_ttob, OUTER_RELEARE_DATA_TTOB, 3),
	PL2_PMU_EVENT_ATTR(outer_release_data_tton, OUTER_RELEARE_DATA_TTON, 3),
	PL2_PMU_EVENT_ATTR(outer_release_data_bton, OUTER_RELEARE_DATA_BTON, 3),
	PL2_PMU_EVENT_ATTR(outer_release_data_ttot, OUTER_RELEARE_DATA_TTOT, 3),
	PL2_PMU_EVENT_ATTR(outer_probe_block_tot, OUTER_PROBE_BLOCK_TOT, 3),
	PL2_PMU_EVENT_ATTR(outer_probe_block_tob, OUTER_PROBE_BLOCK_TOB, 3),
	PL2_PMU_EVENT_ATTR(outer_probe_block_ton, OUTER_PROBE_BLOCK_TON, 3),
	PL2_PMU_EVENT_ATTR(outer_probe_perm_ton, OUTER_PROBE_PERM_TON, 3),
	PL2_PMU_EVENT_ATTR(outer_probe_ack_ttob, OUTER_PROBE_ACK_TTOB, 3),
	PL2_PMU_EVENT_ATTR(outer_probe_ack_tton, OUTER_PROBE_ACK_TTON, 3),
	PL2_PMU_EVENT_ATTR(outer_probe_ack_bton, OUTER_PROBE_ACK_BTON, 3),
	PL2_PMU_EVENT_ATTR(outer_probe_ack_ttot, OUTER_PROBE_ACK_TTOT, 3),
	PL2_PMU_EVENT_ATTR(outer_probe_ack_btob, OUTER_PROBE_ACK_BTOB, 3),
	PL2_PMU_EVENT_ATTR(outer_probe_ack_nton, OUTER_PROBE_ACK_NTON, 3),
	PL2_PMU_EVENT_ATTR(outer_probe_ack_data_ttob, OUTER_PROBE_ACK_DATA_TTOB, 3),
	PL2_PMU_EVENT_ATTR(outer_probe_ack_data_tton, OUTER_PROBE_ACK_DATA_TTON, 3),
	PL2_PMU_EVENT_ATTR(outer_probe_ack_data_ttot, OUTER_PROBE_ACK_DATA_TTOT, 3),

	PL2_PMU_EVENT_ATTR(inner_hint_hits_mshr, INNER_HINT_HITS_MSHR, 4),
	PL2_PMU_EVENT_ATTR(inner_read_hits_mshr, INNER_READ_HITS_MSHR, 4),
	PL2_PMU_EVENT_ATTR(inner_write_hits_mshr, INNER_WRITE_HITS_MSHR, 4),
	PL2_PMU_EVENT_ATTR(inner_read_replay, INNER_READ_REPLAY, 4),
	PL2_PMU_EVENT_ATTR(inner_write_replay, INNER_WRITE_REPLAY, 4),
	PL2_PMU_EVENT_ATTR(outer_probe_replay, OUTER_PROBE_REPLAY, 4),
	PL2_PMU_EVENT_ATTR(replay, REPLAY, 4),
	PL2_PMU_EVENT_ATTR(sleep_by_miss_queue, SLEEP_BY_MISS_QUEUE, 4),
	PL2_PMU_EVENT_ATTR(sleep_by_evict_queue, SLEEP_BY_EVICT_QUEUE, 4),
	PL2_PMU_EVENT_ATTR(sleep_for_back_probe, SLEEP_FOR_BACK_PROBE, 4),
	PL2_PMU_EVENT_ATTR(sleep, SLEEP, 4),

	PL2_PMU_EVENT_ATTR(read_sleep_timer_expire, READ_SLEEP_TIMER_EXPIRE, 5),
	PL2_PMU_EVENT_ATTR(read_oldest_timer_expire, READ_OLDEST_TIMER_EXPIRE, 5),
	PL2_PMU_EVENT_ATTR(write_sleep_timer_expire, WRITE_SLEEP_TIMER_EXPIRE, 5),
	PL2_PMU_EVENT_ATTR(write_oldest_timer_expire, WRITE_OLDEST_TIMER_EXPIRE, 5),
	PL2_PMU_EVENT_ATTR(read_sleep, READ_SLEEP, 5),
	PL2_PMU_EVENT_ATTR(read_dir_update_wakeup, READ_DIR_UPDATE_WAKEUP, 5),
	PL2_PMU_EVENT_ATTR(read_miss_queue_wakeup, READ_MISS_QUEUE_WAKEUP, 5),
	PL2_PMU_EVENT_ATTR(read_evict_queue_wakeup, READ_EVICT_QUEUE_WAKEUP, 5),
	PL2_PMU_EVENT_ATTR(read_sleep_timer_wakeup, READ_SLEEP_TIMER_WAKEUP, 5),
	PL2_PMU_EVENT_ATTR(write_sleep, WRITE_SLEEP, 5),
	PL2_PMU_EVENT_ATTR(write_dir_update_wakeup, WRITE_DIR_UPDATE_WAKEUP, 5),
	PL2_PMU_EVENT_ATTR(write_miss_queue_wakeup, WRITE_MISS_QUEUE_WAKEUP, 5),
	PL2_PMU_EVENT_ATTR(write_evict_queue_wakeup, WRITE_EVICT_QUEUE_WAKEUP, 5),
	PL2_PMU_EVENT_ATTR(write_sleep_timer_wakeup, WRITE_SLEEP_TIMER_WAKEUP, 5),
	NULL
};

static struct attribute_group sifive_pl2_pmu_events_group = {
	.name = "events",
	.attrs = sifive_pl2_pmu_events,
};

/* formats */
PMU_FORMAT_ATTR(event, "config:0-63");

static struct attribute *sifive_pl2_pmu_formats[] = {
	&format_attr_event.attr,
	NULL,
};

static struct attribute_group sifive_pl2_pmu_format_group = {
	.name = "format",
	.attrs = sifive_pl2_pmu_formats,
};

/*
 * Per PMU device attribute groups
 */

static const struct attribute_group *sifive_pl2_pmu_attr_grps[] = {
	&sifive_pl2_pmu_events_group,
	&sifive_pl2_pmu_format_group,
	NULL,
};

/*
 * Event Initialization
 */

static int sifive_pl2_pmu_event_init(struct perf_event *event)
{
	struct hw_perf_event *hwc = &event->hw;
	u64 config = event->attr.config;
	u64 ev_type = config >> 8;
	u64 set = config & 0xff;

	/* Check if this is a valid set and event */
	switch (set) {
	case 1:
		if (ev_type >= BIT_ULL(PL2_PMU_MAX_EVENT1_IDX))
			return -ENOENT;
		break;
	case 2:
		if (ev_type >= BIT_ULL(PL2_PMU_MAX_EVENT2_IDX))
			return -ENOENT;
		break;
	case 3:
		if (ev_type >= BIT_ULL(PL2_PMU_MAX_EVENT3_IDX))
			return -ENOENT;
		break;
	case 4:
		if (ev_type >= BIT_ULL(PL2_PMU_MAX_EVENT4_IDX))
			return -ENOENT;
		break;
	case 5:
		if (ev_type >= BIT_ULL(PL2_PMU_MAX_EVENT5_IDX))
			return -ENOENT;
		break;
	default:
		return -ENOENT;
	}

	/* Do not allocate the hardware counter yet */
	hwc->idx = -1;
	hwc->config = config;

	return 0;
}

/*
 * pmu->read: read and update the counter
 */
static void sifive_pl2_pmu_read(struct perf_event *event)
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
static void sifive_pl2_pmu_start(struct perf_event *event, int flags)
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
static void sifive_pl2_pmu_stop(struct perf_event *event, int flags)
{
	struct hw_perf_event *hwc = &event->hw;

	if (hwc->state & PERF_HES_STOPPED)
		return;

	/* Disable this counter to count events */
	writeq(0, (void *)hwc->config_base);
	sifive_pl2_pmu_read(event);

	hwc->state = PERF_HES_STOPPED | PERF_HES_UPTODATE;
}

/*
 * pmu->add: add the event to the PMU
 */
static int sifive_pl2_pmu_add(struct perf_event *event, int flags)
{
	struct sifive_pl2_pmu *pl2_pmu = to_pl2_pmu(event->pmu);
	struct sifive_pl2_pmu_event *ptr = *this_cpu_ptr(pl2_pmu->event);
	struct hw_perf_event *hwc = &event->hw;
	int idx;

	/* Find an available counter idx to use for this event */
	do {
		idx = find_first_zero_bit(ptr->used_mask, ptr->n_counters);
		if (idx >= ptr->n_counters)
			return -EAGAIN;
	} while (test_and_set_bit(idx, ptr->used_mask));

	hwc->config_base = (unsigned long)ptr->base + PL2_SELECT_OFFSET + 8 * idx;
	hwc->event_base = (unsigned long)ptr->base + PL2_COUNTER_OFFSET + 8 * idx;
	hwc->idx = idx;
	hwc->state = PERF_HES_STOPPED | PERF_HES_UPTODATE;

	ptr->events[idx] = event;

	if (flags & PERF_EF_START)
		sifive_pl2_pmu_start(event, PERF_EF_RELOAD);

	perf_event_update_userpage(event);

	return 0;
}

/*
 * pmu->del: delete the event from the PMU
 */
static void sifive_pl2_pmu_del(struct perf_event *event, int flags)
{
	struct sifive_pl2_pmu *pl2_pmu = to_pl2_pmu(event->pmu);
	struct sifive_pl2_pmu_event *ptr = *this_cpu_ptr(pl2_pmu->event);
	struct hw_perf_event *hwc = &event->hw;
	int idx = hwc->idx;

	/* Stop and release this counter */
	sifive_pl2_pmu_stop(event, PERF_EF_UPDATE);

	ptr->events[idx] = NULL;
	clear_bit(idx, ptr->used_mask);

	perf_event_update_userpage(event);
}

/*
 * pmu->filter: check if the PMU can be used with a CPU
 */
static bool sifive_pl2_pmu_filter(struct pmu *pmu, int cpu)
{
	struct sifive_pl2_pmu *pl2_pmu = to_pl2_pmu(pmu);
	struct sifive_pl2_pmu_event *ptr = *this_cpu_ptr(pl2_pmu->event);

	/* Filter out CPUs with no PL2 instance (no percpu data allocated) */
	return !ptr;
}

/*
 * Driver initialization
 */

static void sifive_pl2_pmu_hw_init(const struct sifive_pl2_pmu_event *ptr)
{
	/* Disable the client filter (not supported by this driver) */
	writeq(0, ptr->base + PL2_CLIENT_FILTER_OFFSET);
}

static int sifive_pl2_pmu_pm_notify(struct notifier_block *nb, unsigned long cmd, void *v)
{
	struct sifive_pl2_pmu *pl2_pmu = container_of(nb, struct sifive_pl2_pmu, cpu_pm_nb);
	struct sifive_pl2_pmu_event *ptr = *this_cpu_ptr(pl2_pmu->event);
	struct perf_event *event;

	if (!ptr || bitmap_empty(ptr->used_mask, PL2_PMU_MAX_COUNTERS))
		return NOTIFY_OK;

	for (int idx = 0; idx < ptr->n_counters; idx++) {
		event = ptr->events[idx];
		if (!event)
			continue;

		switch (cmd) {
		case CPU_PM_ENTER:
			/* Stop and update the counter */
			sifive_pl2_pmu_stop(event, PERF_EF_UPDATE);
			break;
		case CPU_PM_ENTER_FAILED:
		case CPU_PM_EXIT:
			/* Restore and enable the counter */
			sifive_pl2_pmu_start(event, PERF_EF_RELOAD);
			break;
		default:
			break;
		}
	}

	return NOTIFY_OK;
}

static int sifive_pl2_pmu_pm_register(struct sifive_pl2_pmu *pl2_pmu)
{
	if (!IS_ENABLED(CONFIG_CPU_PM))
		return 0;

	pl2_pmu->cpu_pm_nb.notifier_call = sifive_pl2_pmu_pm_notify;
	return cpu_pm_register_notifier(&pl2_pmu->cpu_pm_nb);
}

static void sifive_pl2_pmu_pm_unregister(struct sifive_pl2_pmu *pl2_pmu)
{
	if (!IS_ENABLED(CONFIG_CPU_PM))
		return;

	cpu_pm_unregister_notifier(&pl2_pmu->cpu_pm_nb);
}

static struct sifive_pl2_pmu *sifive_pl2_pmu_get(void)
{
	struct sifive_pl2_pmu *pl2_pmu;
	int ret;

	guard(mutex)(&g_mutex);

	pl2_pmu = g_pl2_pmu;
	if (pl2_pmu) {
		refcount_inc(&pl2_pmu->refcount);
		return pl2_pmu;
	}

	pl2_pmu = kzalloc(sizeof(*pl2_pmu), GFP_KERNEL);
	if (!pl2_pmu)
		return ERR_PTR(-ENOMEM);

	pl2_pmu->pmu = (struct pmu) {
		.attr_groups	= sifive_pl2_pmu_attr_grps,
		.capabilities	= PERF_PMU_CAP_NO_EXCLUDE | PERF_PMU_CAP_NO_INTERRUPT,
		.task_ctx_nr	= perf_sw_context,
		.event_init	= sifive_pl2_pmu_event_init,
		.add		= sifive_pl2_pmu_add,
		.del		= sifive_pl2_pmu_del,
		.start		= sifive_pl2_pmu_start,
		.stop		= sifive_pl2_pmu_stop,
		.read		= sifive_pl2_pmu_read,
		.filter		= sifive_pl2_pmu_filter,
	};

	refcount_set(&pl2_pmu->refcount, 1);

	pl2_pmu->event = alloc_percpu(typeof(*pl2_pmu->event));
	if (!pl2_pmu->event) {
		ret = -ENOMEM;
		goto err_free;
	}

	ret = sifive_pl2_pmu_pm_register(pl2_pmu);
	if (ret)
		goto err_free_percpu;

	ret = perf_pmu_register(&pl2_pmu->pmu, "sifive_pl2_pmu", -1);
	if (ret) {
		pr_err("%s: Failed to register PMU: %d\n", __func__, ret);
		goto err_unregister_pm;
	}

	g_pl2_pmu = pl2_pmu;

	return pl2_pmu;

err_unregister_pm:
	sifive_pl2_pmu_pm_unregister(pl2_pmu);
err_free_percpu:
	free_percpu(pl2_pmu->event);
err_free:
	kfree(pl2_pmu);

	return ERR_PTR(ret);
}

static void sifive_pl2_pmu_put(void)
{
	struct sifive_pl2_pmu *pl2_pmu;

	guard(mutex)(&g_mutex);

	pl2_pmu = g_pl2_pmu;
	if (!refcount_dec_and_test(&pl2_pmu->refcount))
		return;

	g_pl2_pmu = NULL;
	perf_pmu_unregister(&pl2_pmu->pmu);
	sifive_pl2_pmu_pm_unregister(pl2_pmu);
	free_percpu(pl2_pmu->event);
	kfree(pl2_pmu);
}

static int sifive_pl2_pmu_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev_of_node(dev);
	struct sifive_pl2_pmu_event *ptr;
	struct sifive_pl2_pmu *pl2_pmu;
	unsigned int cpu;
	u32 n_counters;
	int ret;

	/* Instances without a sifive,perfmon-counters property do not contain a PMU */
	ret = of_property_read_u32(np, "sifive,perfmon-counters", &n_counters);
	if (ret || !n_counters)
		return -ENODEV;

	/* Determine the CPU affinity of this PL2 instance */
	for_each_possible_cpu(cpu) {
		struct device_node *cache_node, *cpu_node;

		cpu_node = of_cpu_device_node_get(cpu);
		if (!cpu_node)
			continue;

		cache_node = of_parse_phandle(cpu_node, "next-level-cache", 0);
		of_node_put(cpu_node);
		if (!cache_node)
			continue;

		of_node_put(cache_node);
		if (cache_node == np)
			break;
	}
	if (cpu >= nr_cpu_ids)
		return -ENODEV;

	ptr = devm_kzalloc(dev, struct_size(ptr, events, n_counters), GFP_KERNEL);
	if (!ptr)
		return -ENOMEM;

	platform_set_drvdata(pdev, ptr);

	ptr->cpu = cpu;
	ptr->n_counters = n_counters;

	ptr->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(ptr->base))
		return PTR_ERR(ptr->base);

	sifive_pl2_pmu_hw_init(ptr);

	pl2_pmu = sifive_pl2_pmu_get();
	if (IS_ERR(pl2_pmu))
		return PTR_ERR(pl2_pmu);

	*per_cpu_ptr(pl2_pmu->event, cpu) = ptr;

	return 0;
}

static void sifive_pl2_pmu_remove(struct platform_device *pdev)
{
	struct sifive_pl2_pmu_event *ptr = platform_get_drvdata(pdev);

	*per_cpu_ptr(g_pl2_pmu->event, ptr->cpu) = NULL;
	sifive_pl2_pmu_put();
}

static const struct of_device_id sifve_pl2_pmu_of_match[] = {
	{ .compatible = "sifive,pl2cache1" },
	{}
};
MODULE_DEVICE_TABLE(of, sifve_pl2_pmu_of_match);

static struct platform_driver sifive_pl2_pmu_driver = {
	.probe	= sifive_pl2_pmu_probe,
	.remove_new	= sifive_pl2_pmu_remove,
	.driver	= {
		.name		= "sifive_pl2_pmu",
		.of_match_table	= sifve_pl2_pmu_of_match,
	},
};
module_platform_driver(sifive_pl2_pmu_driver);

MODULE_LICENSE("GPL");
