/*
 * sound/soc/amlogic/auge/resample_hw.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include "resample_hw.h"

/*Cnt_ctrl = mclk/fs_out-1 ; fest 256fs */
#define RESAMPLE_CNT_CONTROL 255

static u32 resample_coef_parameters_table[7][5] = {
	/*coef of 32K, fc = 9000, Q:0.55, G= 14.00, */
	{0x0137fd9a, 0x033fe4a2, 0x0029da1f, 0x001a66fb, 0x00075562},
	/*coef of 44.1K, fc = 14700, Q:0.55, G= 14.00, */
	{0x010dac28, 0x03b7f553, 0x0011380c, 0x00479dd8, 0x000f3baf},
	/*coef of 48K, fc = 15000, Q:0.60, G= 11.00, */
	{0x00ea14d7, 0x03c59759, 0x001851f0, 0x00375a09, 0x0010a417},
	/*coef of 88.2K, fc = 26000, Q:0.60, G= 4.00, */
	{0x009dc098, 0x000972c7, 0x000e7582, 0x00277b49, 0x000e2d97},
	/*coef of 96K, fc = 36000, Q:0.50, G= 4.00, */
	{0x0094268c, 0x005d3192, 0x000ea7e2, 0x006a09e6, 0x0015f61a},
	/*no support filter now*/
	{0x00800000, 0x0, 0x0, 0x0, 0x0},
	/*no support filter now*/
	{0x00800000, 0x0, 0x0, 0x0, 0x0},
};

int resample_enable(int input_sr)
{
	u16 Avg_cnt_init = 0;
	unsigned int clk_rate = 167000000;//clk81;

	Avg_cnt_init = (u16)(clk_rate * 4 / input_sr);
	pr_info("clk_rate = %u, input_sr = %d, Avg_cnt_init = %u\n",
		clk_rate, input_sr, Avg_cnt_init);

	audiobus_write(EE_AUDIO_RESAMPLE_CTRL0, (1 << 31));
	audiobus_write(EE_AUDIO_RESAMPLE_CTRL0, 0);
	audiobus_write(EE_AUDIO_RESAMPLE_CTRL0,
				(1 << 28) /* enable */
				| (0 << 26) /* method0 */
				| (RESAMPLE_CNT_CONTROL << 16)
				| Avg_cnt_init);

	return 0;
}

int resample_disable(void)
{
	audiobus_write(EE_AUDIO_RESAMPLE_CTRL0, 0);

	return 0;
}

int resample_set_hw_param(int index)
{
	int i;

	for (i = 0; i < 5; i++) {
		audiobus_write((EE_AUDIO_RESAMPLE_COEF0 + i),
			resample_coef_parameters_table[index][i]);
	}

	audiobus_update_bits(EE_AUDIO_RESAMPLE_CTRL2,
			1 << 25, 1 << 25);

	return 0;
}

void resample_src_select(int src)
{
	audiobus_update_bits(EE_AUDIO_RESAMPLE_CTRL0,
		0x3 << 29,
		src << 29);
}

void resample_format_set(int ch_num, int bits)
{
	audiobus_write(EE_AUDIO_RESAMPLE_CTRL3,
		ch_num << 8 | (bits - 1) << 0);
}
