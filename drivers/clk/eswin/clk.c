// SPDX-License-Identifier: GPL-2.0
/*
 * ESWIN Clk Provider Driver
 *
 * Copyright 2024, Beijing ESWIN Computing Technology Co., Ltd.. All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * Authors: HuangYiFeng<huangyifeng@eswincomputing.com>
 */

#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/util_macros.h>
#include <dt-bindings/clock/eic7700-clock.h>

#include "clk.h"

static struct clk_hw *eswin_clk_find_parent(struct eswin_clock_data *data, char *parent_name)
{
	int i;
	struct clk *clks;

	for (i = 0; i < data->clk_data.clk_num; i++) {
		clks = data->clk_data.clks[i];
		if (NULL == clks) {
			continue;
		}
		if (!strcmp(__clk_get_name(clks), parent_name)) {
			return __clk_get_hw(clks);
		}
	}
	return NULL;
}

struct eswin_clock_data *eswin_clk_init(struct platform_device *pdev,
					     int nr_clks)
{
	struct eswin_clock_data *clk_data;
	struct clk **clk_table;
	void __iomem *base;
	struct device *parent;

	parent = pdev->dev.parent;
	if (!parent) {
		dev_err(&pdev->dev, "no parent\n");
		goto err;
	}

	base = of_iomap(parent->of_node, 0);
	if (!base) {
		dev_err(&pdev->dev,"failed to map clock registers\n");
		goto err;
	}
	clk_data = kzalloc(sizeof(*clk_data), GFP_KERNEL);
	if (!clk_data)
		goto err;

	clk_data->base = base;
	clk_table = kcalloc(nr_clks, sizeof(*clk_table), GFP_KERNEL);
	if (!clk_table)
		goto err_data;

	clk_data->clk_data.clks = clk_table;
	clk_data->clk_data.clk_num = nr_clks;
	clk_data->numa_id = dev_to_node(parent);
	spin_lock_init(&clk_data->lock);

	of_clk_add_provider(pdev->dev.of_node, of_clk_src_onecell_get, &clk_data->clk_data);
	return clk_data;

err_data:
	kfree(clk_data);
err:
	return NULL;
}
EXPORT_SYMBOL_GPL(eswin_clk_init);

int eswin_clk_register_fixed_rate(const struct eswin_fixed_rate_clock *clks,
					 int nums, struct eswin_clock_data *data)
{
	struct clk *clk;
	int i;

	for (i = 0; i < nums; i++) {
		char *name = kzalloc(strlen(clks[i].name) + 2 * sizeof(char) + sizeof(int),
			GFP_KERNEL );
		if (data->numa_id < 0) {
			sprintf(name, "%s", clks[i].name);
		} else {
			sprintf(name, "d%d_%s", data->numa_id, clks[i].name);
		}
		clk = clk_register_fixed_rate(NULL, name, clks[i].parent_name,
				clks[i].flags, clks[i].fixed_rate);
		if (IS_ERR(clk)) {
			pr_err("%s: failed to register clock %s\n", __func__, name);
			kfree(name);
			goto err;
		}
		data->clk_data.clks[clks[i].id] = clk;
		kfree(name);
	}

	return 0;
err:
	while (i--)
		clk_unregister_fixed_rate(data->clk_data.clks[clks[i].id]);

	return PTR_ERR(clk);
}
EXPORT_SYMBOL_GPL(eswin_clk_register_fixed_rate);

static int eswin_calc_pll(u32 *frac_val, u32 *postdiv1_val,
				 u32 *fbdiv_val, u32 *refdiv_val, u64 rate,
				 const struct eswin_clk_pll *clk)
{
	int ret = 0;

	switch (clk->id) {
		case EIC7700_APLL_FOUT1:
			switch (rate) {
				case APLL_LOW_FREQ:
					*frac_val = 10603200;
					*postdiv1_val = 0;
					*fbdiv_val = 37;
					*refdiv_val = 1;
					break;
				case APLL_HIGH_FREQ:
				default:
					*frac_val = 14092861;
					*postdiv1_val = 0;
					*fbdiv_val = 163;
					*refdiv_val = 1;
					break;
			}
			break;
		case EIC7700_PLL_CPU:
			switch (rate) {
				case CLK_FREQ_1800M:
					*frac_val = 0;
					*postdiv1_val = 0;
					*fbdiv_val = 300;
					*refdiv_val = 1;
					break;
				case CLK_FREQ_1700M:
					*frac_val = 5592405;
					*postdiv1_val = 0;
					*fbdiv_val = 283;
					*refdiv_val = 1;
					break;
				case CLK_FREQ_1600M:
					*frac_val = 11184810;
					*postdiv1_val = 0;
					*fbdiv_val = 266;
					*refdiv_val = 1;
					break;
				case CLK_FREQ_1500M:
					*frac_val = 0;
					*postdiv1_val = 0;
					*fbdiv_val = 250;
					*refdiv_val = 1;
					break;
				case CLK_FREQ_1300M:
					*frac_val = 11184810;
					*postdiv1_val = 0;
					*fbdiv_val = 216;
					*refdiv_val = 1;
					break;
				case CLK_FREQ_1200M:
					*frac_val = 0;
					*postdiv1_val = 0;
					*fbdiv_val = 200;
					*refdiv_val = 1;
					break;
				case CLK_FREQ_1000M:
					*frac_val = 11184810;
					*postdiv1_val = 0;
					*fbdiv_val = 166;
					*refdiv_val = 1;
					break;
				case CLK_FREQ_900M:
					*frac_val = 0;
					*postdiv1_val = 0;
					*fbdiv_val = 150;
					*refdiv_val = 1;
					break;
				case CLK_FREQ_800M:
					*frac_val = 5592405;
					*postdiv1_val = 0;
					*fbdiv_val = 133;
					*refdiv_val = 1;
					break;
				case CLK_FREQ_700M:
					*frac_val = 11184810;
					*postdiv1_val = 0;
					*fbdiv_val = 116;
					*refdiv_val = 1;
					break;
				case CLK_FREQ_600M:
					*frac_val = 0;
					*postdiv1_val = 0;
					*fbdiv_val = 100;
					*refdiv_val = 1;
					break;
				case CLK_FREQ_500M:
					*frac_val = 5592405;
					*postdiv1_val = 0;
					*fbdiv_val = 83;
					*refdiv_val = 1;
					break;
				case CLK_FREQ_400M:
					*frac_val = 11184810;
					*postdiv1_val = 0;
					*fbdiv_val = 66;
					*refdiv_val = 1;
					break;
				case CLK_FREQ_200M:
					*frac_val = 5592405;
					*postdiv1_val = 0;
					*fbdiv_val = 33;
					*refdiv_val = 1;
					break;
				case CLK_FREQ_100M:
					*frac_val = 11184810;
					*postdiv1_val = 0;
					*fbdiv_val = 16;
					*refdiv_val = 1;
					break;
				case CLK_FREQ_1400M:
				default:
					*frac_val = 5592405;
					*postdiv1_val = 0;
					*fbdiv_val = 233;
					*refdiv_val = 1;
					break;
			}
			break;
		default:
			ret = -EINVAL;
			pr_err("%s %d, Invalid pll set req, rate %lld, clk id %d\n", __func__, __LINE__, rate, clk->id);
			break;
	}
	return ret;
}

#define to_pll_clk(_hw) container_of(_hw, struct eswin_clk_pll, hw)
static int clk_pll_set_rate(struct clk_hw *hw,
			    unsigned long rate,
			    unsigned long parent_rate)
{
	struct eswin_clk_pll *clk = to_pll_clk(hw);
	u32 frac_val = 0, postdiv1_val, fbdiv_val, refdiv_val;
	u32 val;
	int ret;
	struct clk *clk_cpu_mux = NULL;
	struct clk *clk_cpu_lp_pll = NULL;
	struct clk *clk_cpu_pll = NULL;
	int try_count = 0;
	bool lock_flag = false;
	char clk_cpu_mux_name[50] = {0};
	char clk_cpu_lp_pll_name[50] = {0};
	char clk_cpu_pll_name[50] = {0};

	ret = eswin_calc_pll(&frac_val, &postdiv1_val, &fbdiv_val, &refdiv_val, (u64)rate, clk);
	if (ret) {
		return ret;
	}

	/*
	  we must switch the cpu to other clk before we change the cpu pll
	*/
	if (EIC7700_PLL_CPU == clk->id) {
		if (clk->numa_id < 0) {
			sprintf(clk_cpu_mux_name, "%s", "mux_u_cpu_root_3mux1_gfree");
			sprintf(clk_cpu_lp_pll_name, "%s", "clk_clk_u84_core_lp");
			sprintf(clk_cpu_pll_name, "%s", "clk_pll_cpu");
		} else {
			sprintf(clk_cpu_mux_name, "d%d_%s", clk->numa_id, "mux_u_cpu_root_3mux1_gfree");
			sprintf(clk_cpu_lp_pll_name, "d%d_%s", clk->numa_id, "clk_clk_u84_core_lp");
			sprintf(clk_cpu_pll_name, "d%d_%s", clk->numa_id, "clk_pll_cpu");
		}

		clk_cpu_mux = __clk_lookup(clk_cpu_mux_name);
		if (!clk_cpu_mux) {
			pr_err("%s %d, failed to get %s\n",__func__,__LINE__, clk_cpu_mux_name);
			return -EINVAL;
		}
		clk_cpu_lp_pll = __clk_lookup(clk_cpu_lp_pll_name);
		if (!clk_cpu_lp_pll) {
			pr_err("%s %d, failed to get %s\n",__func__,__LINE__, clk_cpu_lp_pll_name);
			return -EINVAL;
		}
		clk_cpu_pll = __clk_lookup(clk_cpu_pll_name);
		if (!clk_cpu_pll) {
			pr_err("%s %d, failed to get %s\n",__func__,__LINE__, clk_cpu_pll_name);
			return -EINVAL;
		}

		ret = clk_set_parent(clk_cpu_mux, clk_cpu_lp_pll);
		if (ret) {
			pr_err("%s %d, faild to switch %s to %s, ret %d\n",__func__,__LINE__, clk_cpu_mux_name,
				clk_cpu_lp_pll_name, ret);
			return -EPERM;
		}
	}

	/*first disable pll */
	val = readl_relaxed(clk->ctrl_reg0);
	val &= ~(((1 << clk->pllen_width) - 1) << clk->pllen_shift);
	val |= 0 << clk->pllen_shift;
	writel_relaxed(val, clk->ctrl_reg0);

	val = readl_relaxed(clk->ctrl_reg0);
	val &= ~(((1 << clk->fbdiv_width) - 1) << clk->fbdiv_shift);
	val &= ~(((1 << clk->refdiv_width) - 1) << clk->refdiv_shift);
	val |= refdiv_val << clk->refdiv_shift;
	val |= fbdiv_val << clk->fbdiv_shift;
	writel_relaxed(val, clk->ctrl_reg0);

	val = readl_relaxed(clk->ctrl_reg1);
	val &= ~(((1 << clk->frac_width) - 1) << clk->frac_shift);
	val |= frac_val << clk->frac_shift;
	writel_relaxed(val, clk->ctrl_reg1);

	val = readl_relaxed(clk->ctrl_reg2);
	val &= ~(((1 << clk->postdiv1_width) - 1) << clk->postdiv1_shift);
	val |= postdiv1_val << clk->postdiv1_shift;
	writel_relaxed(val, clk->ctrl_reg2);

	/*at last, enable pll */
	val = readl_relaxed(clk->ctrl_reg0);
	val &= ~(((1 << clk->pllen_width) - 1) << clk->pllen_shift);
	val |= 1 << clk->pllen_shift;
	writel_relaxed(val, clk->ctrl_reg0);

	/*
	  usually the pll wil lock in 50us
	*/
	do {
		usleep_range(refdiv_val * 80, refdiv_val * 80 * 2);
		val = readl_relaxed(clk->status_reg);
		if (val & 1 << clk->lock_shift) {
			lock_flag = true;
			break;
		}
	} while (try_count++ < 10);

	if (false == lock_flag) {
		pr_err("%s %d, faild to lock the cpu pll, cpu will work on low power pll\n",__func__,__LINE__);
		return -EBUSY;
	}
	if (EIC7700_PLL_CPU == clk->id) {
		ret = clk_set_parent(clk_cpu_mux, clk_cpu_pll);
		if (ret) {
			pr_err("%s %d, faild to switch %s to %s, ret %d\n",__func__,__LINE__,
				clk_cpu_mux_name, clk_cpu_pll_name, ret);
			return -EPERM;
		}
	}
	return  0;
}

static unsigned long clk_pll_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct eswin_clk_pll *clk = to_pll_clk(hw);
	u64 frac_val, fbdiv_val, refdiv_val;
	u32 postdiv1_val;
	u32 val;
	u64 rate;

	val = readl_relaxed(clk->ctrl_reg0);
	val = val >> clk->fbdiv_shift;
	val &= ((1 << clk->fbdiv_width) - 1);
	fbdiv_val = val;

	val = readl_relaxed(clk->ctrl_reg0);
	val = val >> clk->refdiv_shift;
	val &= ((1 << clk->refdiv_width) - 1);
	refdiv_val = val;

	val = readl_relaxed(clk->ctrl_reg1);
	val = val >> clk->frac_shift;
	val &= ((1 << clk->frac_width) - 1);
	frac_val = val;

	val = readl_relaxed(clk->ctrl_reg2);
	val = val >> clk->postdiv1_shift;
	val &= ((1 << clk->postdiv1_width) - 1);
	postdiv1_val = val;

	switch (clk->id) {
		case EIC7700_APLL_FOUT1:
			switch (frac_val) {
				case 14092861:
					rate = APLL_HIGH_FREQ;
					break;
				case 10603200:
					rate = APLL_LOW_FREQ;
					break;
				default:
					pr_err("%s %d, clk id %d, unknow frac_val %llu\n", __func__, __LINE__, clk->id, frac_val);
					rate = 0;
					break;
			}
			break;
		case EIC7700_PLL_CPU:
			switch (fbdiv_val) {
				case 300:
					rate = CLK_FREQ_1800M;
					break;
				case 283:
					rate = CLK_FREQ_1700M;
					break;
				case 266:
					rate = CLK_FREQ_1600M;
					break;
				case 250:
					rate = CLK_FREQ_1500M;
					break;
				case 216:
					rate = CLK_FREQ_1300M;
					break;
				case 200:
					rate = CLK_FREQ_1200M;
					break;
				case 166:
					rate = CLK_FREQ_1000M;
					break;
				case 150:
					rate = CLK_FREQ_900M;
					break;
				case 133:
					rate = CLK_FREQ_800M;
					break;
				case 116:
					rate = CLK_FREQ_700M;
					break;
				case 100:
					rate = CLK_FREQ_600M;
					break;
				case 83:
					rate = CLK_FREQ_500M;
					break;
				case 66:
					rate = CLK_FREQ_400M;
					break;
				case 33:
					rate = CLK_FREQ_200M;
					break;
				case 16:
					rate = CLK_FREQ_100M;
					break;
				case 233:
					rate = CLK_FREQ_1400M;
					break;
				default:
					pr_err("%s %d, clk id %d, unknow fbdiv_val %llu\n", __func__, __LINE__, clk->id, fbdiv_val);
					rate = 0;
					break;
			}
			break;
		default:
			pr_err("%s %d, unknow clk id %d\n", __func__, __LINE__, clk->id);
			rate = 0;
			break;
	}
	return rate;
}

static long clk_pll_round_rate(struct clk_hw *hw, unsigned long rate,
					unsigned long *parent_rate)
{
	struct eswin_clk_pll *clk = to_pll_clk(hw);
	int index;
	u64 round_rate = 0;

	/*Must be sorted in ascending order*/
	u64 apll_clk[] = {APLL_LOW_FREQ, APLL_HIGH_FREQ};
	u64 cpu_pll_clk[] = {CLK_FREQ_100M, CLK_FREQ_200M, CLK_FREQ_400M, CLK_FREQ_500M, CLK_FREQ_600M, CLK_FREQ_700M,
				CLK_FREQ_800M, CLK_FREQ_900M, CLK_FREQ_1000M, CLK_FREQ_1200M, CLK_FREQ_1300M,
				CLK_FREQ_1400M, CLK_FREQ_1500M, CLK_FREQ_1600M, CLK_FREQ_1700M, CLK_FREQ_1800M};

	switch (clk->id) {
		case EIC7700_APLL_FOUT1:
			index = find_closest(rate, apll_clk, ARRAY_SIZE(apll_clk));
			round_rate = apll_clk[index];
			break;
		case EIC7700_PLL_CPU:
			index = find_closest(rate, cpu_pll_clk, ARRAY_SIZE(cpu_pll_clk));
			round_rate = cpu_pll_clk[index];
			break;
		default:
			pr_err("%s %d, unknow clk id %d\n", __func__, __LINE__, clk->id);
			round_rate = 0;
			break;
	}
	return round_rate;
}

static const struct clk_ops eswin_clk_pll_ops = {
	.set_rate = clk_pll_set_rate,
	.recalc_rate = clk_pll_recalc_rate,
	.round_rate = clk_pll_round_rate,
};

void eswin_clk_register_pll(struct eswin_pll_clock *clks,
		int nums, struct eswin_clock_data *data, struct device *dev)
{
	void __iomem *base = data->base;
	struct eswin_clk_pll *p_clk = NULL;
	struct clk *clk = NULL;
	struct clk_init_data init;
	int i;

	p_clk = devm_kzalloc(dev, sizeof(*p_clk) * nums, GFP_KERNEL);

	if (!p_clk)
		return;

	for (i = 0; i < nums; i++) {
		char *name = kzalloc(strlen(clks[i].name)
			+ 2 * sizeof(char) + sizeof(int), GFP_KERNEL);
		const char *parent_name = clks[i].parent_name ? kzalloc(strlen(clks[i].parent_name)
			+ 2 * sizeof(char) + sizeof(int), GFP_KERNEL) : NULL;
		if (data->numa_id < 0) {
			sprintf(name, "%s", clks[i].name);
			if (parent_name) {
				sprintf((char *)parent_name, "%s", clks[i].parent_name);
			}
		} else {
			sprintf(name, "d%d_%s", data->numa_id, clks[i].name);
			if (parent_name) {
				sprintf((char *)parent_name, "d%d_%s", data->numa_id, clks[i].parent_name);
			}
		}

		init.name = name;
		init.flags = 0;
		init.parent_names = parent_name ? &parent_name : NULL;
		init.num_parents = parent_name ? 1 : 0;
		init.ops = &eswin_clk_pll_ops;

		p_clk->id = clks[i].id;
		p_clk->numa_id = data->numa_id;
		p_clk->ctrl_reg0 = base + clks[i].ctrl_reg0;
		p_clk->pllen_shift = clks[i].pllen_shift;
		p_clk->pllen_width = clks[i].pllen_width;
		p_clk->refdiv_shift = clks[i].refdiv_shift;
		p_clk->refdiv_width = clks[i].refdiv_width;
		p_clk->fbdiv_shift = clks[i].fbdiv_shift;
		p_clk->fbdiv_width = clks[i].fbdiv_width;

		p_clk->ctrl_reg1 = base + clks[i].ctrl_reg1;
		p_clk->frac_shift = clks[i].frac_shift;
		p_clk->frac_width = clks[i].frac_width;

		p_clk->ctrl_reg2 = base + clks[i].ctrl_reg2;
		p_clk->postdiv1_shift = clks[i].postdiv1_shift;
		p_clk->postdiv1_width = clks[i].postdiv1_width;
		p_clk->postdiv2_shift = clks[i].postdiv2_shift;
		p_clk->postdiv2_width = clks[i].postdiv2_width;

		p_clk->status_reg = base + clks[i].status_reg;
		p_clk->lock_shift = clks[i].lock_shift;
		p_clk->lock_width = clks[i].lock_width;

		p_clk->hw.init = &init;

		clk = clk_register(dev, &p_clk->hw);
		if (IS_ERR(clk)) {
			devm_kfree(dev, p_clk);
			dev_err(dev, "%s: failed to register clock %s\n", __func__, clks[i].name);
			continue;
		}

		data->clk_data.clks[clks[i].id] = clk;
		p_clk++;
		kfree(name);
		if (parent_name) {
			kfree(parent_name);
		}
	}
}
EXPORT_SYMBOL_GPL(eswin_clk_register_pll);

int eswin_clk_register_fixed_factor(const struct eswin_fixed_factor_clock *clks,
					   int nums,
					   struct eswin_clock_data *data)
{
	struct clk *clk;
	int i;

	for (i = 0; i < nums; i++) {
		char *name = kzalloc(strlen(clks[i].name)
			+ 2 * sizeof(char) + sizeof(int), GFP_KERNEL );
		char *parent_name = kzalloc(strlen(clks[i].parent_name)
			+ 2 * sizeof(char) + sizeof(int), GFP_KERNEL );
		if (data->numa_id < 0) {
			sprintf(name, "%s", clks[i].name);
			sprintf(parent_name, "%s", clks[i].parent_name);
		} else {
			sprintf(name, "d%d_%s", data->numa_id, clks[i].name);
			sprintf(parent_name, "d%d_%s", data->numa_id, clks[i].parent_name);
		}

		clk = clk_register_fixed_factor(NULL, name,
						parent_name,
						clks[i].flags, clks[i].mult,
						clks[i].div);
		if (IS_ERR(clk)) {
			pr_err("%s: failed to register clock %s\n", __func__, name);
			kfree(name);
			kfree(parent_name);
			goto err;
		}
		data->clk_data.clks[clks[i].id] = clk;

		kfree(name);
		kfree(parent_name);
	}

	return 0;

err:
	while (i--)
		clk_unregister_fixed_factor(data->clk_data.clks[clks[i].id]);

	return PTR_ERR(clk);
}
EXPORT_SYMBOL_GPL(eswin_clk_register_fixed_factor);

int eswin_clk_register_mux(const struct eswin_mux_clock *clks,
				  int nums, struct eswin_clock_data *data)
{
	struct clk *clk;
	void __iomem *base = data->base;
	int i;
	int j;

	for (i = 0; i < nums; i++) {
		char *name = kzalloc(strlen(clks[i].name)
			+ 2 * sizeof(char) + sizeof(int), GFP_KERNEL );

		char **parent_names = kzalloc(sizeof(char *) * clks[i].num_parents,
			GFP_KERNEL );
		if (data->numa_id < 0) {
			sprintf(name, "%s", clks[i].name);
		} else {
			sprintf(name, "d%d_%s", data->numa_id, clks[i].name);
		}
		for (j = 0; j < clks[i].num_parents; j++) {
			parent_names[j] = kzalloc(strlen(clks[i].parent_names[j])
				+ 2 * sizeof(char) + sizeof(int), GFP_KERNEL );
			if (data->numa_id < 0) {
				sprintf(parent_names[j], "%s", clks[i].parent_names[j]);
			} else {
				sprintf(parent_names[j], "d%d_%s",
					data->numa_id, clks[i].parent_names[j]);
			}
		}
		clk = clk_register_mux_table(NULL, name,
				(const char * const*)parent_names,
				clks[i].num_parents, clks[i].flags,
				base + clks[i].offset, clks[i].shift,
				clks[i].mask, clks[i].mux_flags,
				clks[i].table, &data->lock);
		if (IS_ERR(clk)) {
			pr_err("%s: failed to register clock %s\n", __func__, clks[i].name);
			kfree(name);
			for (j = 0; j < clks[i].num_parents; j++) {
				kfree(parent_names[j]);
			}
			kfree(parent_names);
			goto err;
		}

		if (clks[i].alias)
			clk_register_clkdev(clk, clks[i].alias, NULL);

		data->clk_data.clks[clks[i].id] = clk;

		kfree(name);
		for (j = 0; j < clks[i].num_parents; j++) {
			kfree(parent_names[j]);
		}
		kfree(parent_names);
	}
	return 0;

err:
	while (i--)
		clk_unregister_mux(data->clk_data.clks[clks[i].id]);

	return PTR_ERR(clk);
}
EXPORT_SYMBOL_GPL(eswin_clk_register_mux);

int eswin_clk_register_divider(const struct eswin_divider_clock *clks,
				      int nums, struct eswin_clock_data *data)
{
	struct clk *clk;
	void __iomem *base = data->base;
	int i;
	struct clk_hw *clk_hw;
	struct clk_hw *parent_hw;
	struct clk_parent_data	parent_data;

	for (i = 0; i < nums; i++) {
		char *name = kzalloc(strlen(clks[i].name)
			+ 2 * sizeof(char) + sizeof(int), GFP_KERNEL );
		char *parent_name = kzalloc(strlen(clks[i].parent_name)
			+ 2 * sizeof(char) + sizeof(int), GFP_KERNEL );
		if (data->numa_id < 0) {
			sprintf(name, "%s", clks[i].name);
			sprintf(parent_name, "%s", clks[i].parent_name);
		} else {
			sprintf(name, "d%d_%s", data->numa_id, clks[i].name);
			sprintf(parent_name, "d%d_%s", data->numa_id, clks[i].parent_name);
		}
		parent_hw = eswin_clk_find_parent(data, parent_name);
		parent_data.name = parent_name;
		parent_data.hw = parent_hw;
		parent_data.fw_name = NULL;
		clk_hw = clk_hw_register_divider_table_parent_data(NULL, name,
						&parent_data,
						clks[i].flags,
						base + clks[i].offset,
						clks[i].shift, clks[i].width,
						clks[i].div_flags,
						clks[i].table,
						&data->lock);
		if (IS_ERR(clk_hw)) {
			pr_err("%s: failed to register clock %s\n", __func__, clks[i].name);
			kfree(name);
			kfree(parent_name);
			goto err;
		}
		clk = clk_hw->clk;
		if (clks[i].alias)
			clk_register_clkdev(clk, clks[i].alias, NULL);

		data->clk_data.clks[clks[i].id] = clk;
		kfree(name);
		kfree(parent_name);
	}
	return 0;

err:
	while (i--)
		clk_unregister_divider(data->clk_data.clks[clks[i].id]);

	return PTR_ERR(clk);
}
EXPORT_SYMBOL_GPL(eswin_clk_register_divider);

int eswin_clk_register_gate(const struct eswin_gate_clock *clks,
				       int nums, struct eswin_clock_data *data)
{
	struct clk *clk;
	void __iomem *base = data->base;
	int i;
	struct clk_hw *clk_hw;
	struct clk_hw *parent_hw;
	struct clk_parent_data	parent_data;

	for (i = 0; i < nums; i++) {
		char *name = kzalloc(strlen(clks[i].name)
			+ 2 * sizeof(char) + sizeof(int), GFP_KERNEL);
		char *parent_name = kzalloc(strlen(clks[i].parent_name)
			+ 2 * sizeof(char) + sizeof(int), GFP_KERNEL);
		if (data->numa_id < 0) {
			sprintf(name, "%s", clks[i].name);
			sprintf(parent_name, "%s", clks[i].parent_name);
		} else {
			sprintf(name, "d%d_%s", data->numa_id, clks[i].name);
			sprintf(parent_name, "d%d_%s", data->numa_id, clks[i].parent_name);
		}
		parent_hw = eswin_clk_find_parent(data, parent_name);
		parent_data.name = parent_name;
		parent_data.hw = parent_hw;
		parent_data.fw_name = NULL;
		clk_hw = clk_hw_register_gate_parent_data(NULL, name,
				&parent_data,
				clks[i].flags,
				base + clks[i].offset,
				clks[i].bit_idx,
				clks[i].gate_flags,
				&data->lock);
		if (IS_ERR(clk_hw)) {
			pr_err("%s: failed to register clock %s\n",__func__, clks[i].name);
			kfree(name);
			kfree(parent_name);
			goto err;
		}
		clk = clk_hw->clk;
		if (clks[i].alias)
			clk_register_clkdev(clk, clks[i].alias, NULL);

		data->clk_data.clks[clks[i].id] = clk;
		kfree(name);
		kfree(parent_name);
	}
	return 0;

err:
	while (i--)
		clk_unregister_gate(data->clk_data.clks[clks[i].id]);

	return PTR_ERR(clk);
}
EXPORT_SYMBOL_GPL(eswin_clk_register_gate);

static const struct clk_ops clk_dummpy_ops = {

};

static struct clk *eswin_register_clk(struct eswin_clock_data *data,
				     struct device *dev, const char *name,
				      const char *parent_name,
				      unsigned long flags,
				      spinlock_t *lock)
{
	struct eswin_clock *eclk;
	struct clk *clk;
	struct clk_init_data init;
	struct clk_parent_data	parent_data;
	struct clk_hw 		*parent_hw;

	eclk = kzalloc(sizeof(*eclk), GFP_KERNEL );
	if (!eclk)
		return ERR_PTR(-ENOMEM);

	init.ops = &clk_dummpy_ops;

	init.name = name;
	init.flags = flags;
	init.parent_names = NULL;
	init.num_parents = (parent_name ? 1 : 0);
	init.parent_data = &parent_data;

	parent_hw = eswin_clk_find_parent(data, (char *)parent_name);
	parent_data.name = parent_name;
	parent_data.hw = parent_hw;
	parent_data.fw_name = NULL;

	eclk->hw.init = &init;

	clk = clk_register(dev, &eclk->hw);
	if (IS_ERR(clk))
		kfree(eclk);

	return clk;
}

int eswin_clk_register_clk(const struct eswin_clock *clks,
				       int nums, struct eswin_clock_data *data)
{
	struct clk *clk;
	int 	i;

	for (i = 0; i < nums; i++) {
		char *name = kzalloc(strlen(clks[i].name)
			+ 2 * sizeof(char) + sizeof(int), GFP_KERNEL );
		char *parent_name = kzalloc(strlen(clks[i].parent_name)
			+ 2 * sizeof(char) + sizeof(int), GFP_KERNEL );
		if (data->numa_id < 0) {
			sprintf(name, "%s", clks[i].name);
			sprintf(parent_name, "%s", clks[i].parent_name);
		} else {
			sprintf(name, "d%d_%s", data->numa_id, clks[i].name);
			sprintf(parent_name, "d%d_%s", data->numa_id, clks[i].parent_name);
		}
		clk = eswin_register_clk(data, NULL, name, parent_name,
					clks[i].flags, &data->lock);
		if (IS_ERR(clk)) {
			pr_err("%s: failed to register clock %s\n", __func__, clks[i].name);
			kfree(name);
			kfree(parent_name);
			goto err;
		}

		if (clks[i].alias)
			clk_register_clkdev(clk, clks[i].alias, NULL);

		data->clk_data.clks[clks[i].id] = clk;
		kfree(name);
		kfree(parent_name);
	}
	return 0;
err:
	while (i--)
		clk_unregister_gate(data->clk_data.clks[clks[i].id]);

	return PTR_ERR(clk);
}
EXPORT_SYMBOL_GPL(eswin_clk_register_clk);
