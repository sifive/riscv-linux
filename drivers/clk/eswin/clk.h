/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright 2025, Beijing ESWIN Computing Technology Co., Ltd..
 * All rights reserved.
 *
 * Authors:
 *	Yifeng Huang <huangyifeng@eswincomputing.com>
 *	Xuyang Dong <dongxuyang@eswincomputing.com>
 */

#ifndef __ESWIN_CLK_H__
#define __ESWIN_CLK_H__

#include <linux/clk-provider.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>

#define APLL_HIGH_FREQ 983040000
#define APLL_LOW_FREQ 225792000
#define PLL_HIGH_FREQ 1800000000
#define PLL_LOW_FREQ 24000000

struct eswin_clock_data {
	void __iomem *base;
	spinlock_t lock; /* protect register read-modify-write cycle */
	struct clk_hw_onecell_data clk_data;
};

struct eswin_divider_clock {
	unsigned int id;
	const char *name;
	const char *parent_name;
	unsigned long flags;
	unsigned long offset;
	u8 shift;
	u8 width;
	u8 div_flags;
};

struct eswin_fixed_rate_clock {
	unsigned int id;
	char *name;
	const char *parent_name;
	unsigned long flags;
	unsigned long rate;
};

struct eswin_fixed_factor_clock {
	unsigned int id;
	char *name;
	const char *parent_name;
	unsigned long mult;
	unsigned long div;
	unsigned long flags;
};

struct eswin_gate_clock {
	unsigned int id;
	const char *name;
	const char *parent_name;
	unsigned long flags;
	unsigned long offset;
	u8 bit_idx;
	u8 gate_flags;
};

struct eswin_mux_clock {
	unsigned int id;
	const char *name;
	const char *const *parent_names;
	u8 num_parents;
	unsigned long flags;
	unsigned long offset;
	u8 shift;
	u8 width;
	u8 mux_flags;
	u32 *table;
};

struct eswin_pll_clock {
	u32 id;
	const char *name;
	const char *parent_name;
	const u32 ctrl_reg0;
	const u8 pllen_shift;
	const u8 pllen_width;
	const u8 refdiv_shift;
	const u8 refdiv_width;
	const u8 fbdiv_shift;
	const u8 fbdiv_width;

	const u32 ctrl_reg1;
	const u8 frac_shift;
	const u8 frac_width;

	const u32 ctrl_reg2;
	const u8 postdiv1_shift;
	const u8 postdiv1_width;
	const u8 postdiv2_shift;
	const u8 postdiv2_width;

	const u32 status_reg;
	const u8 lock_shift;
	const u8 lock_width;
};

struct eswin_clk_pll {
	struct clk_hw hw;
	u32 id;
	void __iomem *ctrl_reg0;
	u8 pllen_shift;
	u8 pllen_width;
	u8 refdiv_shift;
	u8 refdiv_width;
	u8 fbdiv_shift;
	u8 fbdiv_width;

	void __iomem *ctrl_reg1;
	u8 frac_shift;
	u8 frac_width;

	void __iomem *ctrl_reg2;
	u8 postdiv1_shift;
	u8 postdiv1_width;
	u8 postdiv2_shift;
	u8 postdiv2_width;

	void __iomem *status_reg;
	u8 lock_shift;
	u8 lock_width;
};

struct eswin_clock_data *eswin_clk_init(struct device *dev, int nr_clks);
int eswin_clk_register_fixed_rate(const struct eswin_fixed_rate_clock *clks,
				  int nums, struct eswin_clock_data *data,
				  struct device *dev);
void eswin_clk_register_pll(const struct eswin_pll_clock *clks, int nums,
			    struct eswin_clock_data *data, struct device *dev);
int eswin_clk_register_fixed_factor(const struct eswin_fixed_factor_clock *clks,
				    int nums, struct eswin_clock_data *data,
				    struct device *dev);
int eswin_clk_register_mux(const struct eswin_mux_clock *clks, int nums,
			   struct eswin_clock_data *data, struct device *dev);
int eswin_clk_register_mux_tbl(const struct eswin_mux_clock *clks,
			       int nums, struct eswin_clock_data *data,
			       struct device *dev);
int eswin_clk_register_divider(const struct eswin_divider_clock *clks,
			       int nums, struct eswin_clock_data *data,
			       struct device *dev);
int eswin_clk_register_gate(const struct eswin_gate_clock *clks, int nums,
			    struct eswin_clock_data *data, struct device *dev);

#define PNAME(x) static const char *const x[] __initconst

#define EIC7700_DIV(_id, _name, _pname, _flags, _offset, _shift, _width, \
		    _dflags)						\
	{								\
		.id		= _id,					\
		.name		= _name,				\
		.parent_name	= _pname,				\
		.flags		= _flags,				\
		.offset		= _offset,				\
		.shift		= _shift,				\
		.width		= _width,				\
		.div_flags	= _dflags,				\
	}

#define EIC7700_FACTOR(_id, _name, _pname, _mult, _div, _flags)	\
	{							\
		.id		= _id,				\
		.name		= _name,			\
		.parent_name	= _pname,			\
		.mult		= _mult,			\
		.div		= _div,				\
		.flags		= _flags,			\
	}

#define EIC7700_FIXED(_id, _name, _pname, _flags, _rate)	\
{								\
	.id		= _id,					\
	.name		= _name,				\
	.parent_name	= _pname,				\
	.flags		= _flags,				\
	.rate		= _rate,				\
}

#define EIC7700_GATE(_id, _name, _pname, _flags, _offset, _idx, _gflags) \
	{								\
		.id		= _id,					\
		.name		= _name,				\
		.parent_name	= _pname,				\
		.flags		= _flags,				\
		.offset		= _offset,				\
		.bit_idx	= _idx,					\
		.gate_flags	= _gflags,				\
	}

#define EIC7700_MUX(_id, _name, _pnames, _num_parents, _flags, _offset,	\
		    _shift, _width, _mflags)				\
	{								\
		.id		= _id,					\
		.name		= _name,				\
		.parent_names	= _pnames,				\
		.num_parents	= _num_parents,				\
		.flags		= _flags,				\
		.offset		= _offset,				\
		.shift		= _shift,				\
		.width		= _width,				\
		.mux_flags	= _mflags,				\
	}

#define EIC7700_MUX_TBL(_id, _name, _pnames, _num_parents, _flags, _offset, \
			_shift, _width, _mflags, _table)		\
	{								\
		.id		= _id,					\
		.name		= _name,				\
		.parent_names	= _pnames,				\
		.num_parents	= _num_parents,				\
		.flags		= _flags,				\
		.offset		= _offset,				\
		.shift		= _shift,				\
		.width		= _width,				\
		.mux_flags	= _mflags,				\
		.table		= _table,				\
	}

#define EIC7700_PLL(_id, _name, _pname, _reg0, _en_shift, _en_width,	\
		    _ref_shift, _ref_width, _fb_shift, _fb_width, _reg1,\
		    _frac_shift, _frac_width, _reg2, _post1_shift,	\
		    _post1_width, _post2_shift, _post2_width, _reg,	\
		    _lock_shift, _lock_width)				\
	{								\
		.id		= _id,					\
		.name		= _name,				\
		.parent_name	= _pname,				\
		.ctrl_reg0	= _reg0,				\
		.pllen_shift	= _en_shift,				\
		.pllen_width	= _en_width,				\
		.refdiv_shift	= _ref_shift,				\
		.refdiv_width	= _ref_width,				\
		.fbdiv_shift	= _fb_shift,				\
		.fbdiv_width	= _fb_width,				\
		.ctrl_reg1	= _reg1,				\
		.frac_shift	= _frac_shift,				\
		.frac_width	= _frac_width,				\
		.ctrl_reg2	= _reg2,				\
		.postdiv1_shift	= _post1_shift,				\
		.postdiv1_width	= _post1_width,				\
		.postdiv2_shift	= _post2_shift,				\
		.postdiv2_width	= _post2_width,				\
		.status_reg	= _reg,					\
		.lock_shift	= _lock_shift,				\
		.lock_width	= _lock_width,				\
	}

#endif /* __ESWIN_CLK_H__ */
