/*
 * sound/soc/amlogic/auge/spdif_hw.c
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

#include <sound/soc.h>

#include "iomap.h"
#include "spdif_hw.h"
#include "ddr_mngr.h"

#include <linux/amlogic/media/sound/aout_notify.h>

/*#define G12A_PTM*/
/*#define G12A_PTM_LB_INTERNAL*/

void aml_spdif_enable(
	struct aml_audio_controller *actrl,
	int stream,
	int index,
	bool is_enable)
{
	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		unsigned int offset, reg;

		offset = EE_AUDIO_SPDIFOUT_B_CTRL0 - EE_AUDIO_SPDIFOUT_CTRL0;
		reg = EE_AUDIO_SPDIFOUT_CTRL0 + offset * index;
		aml_audiobus_update_bits(actrl,
			reg, 1<<31, is_enable<<31);
	} else {
		aml_audiobus_update_bits(actrl,
			EE_AUDIO_SPDIFIN_CTRL0, 1<<31, is_enable<<31);
#ifdef G12A_PTM_LB_INTERNAL
		if (index == 0)
		aml_audiobus_update_bits(actrl,
			EE_AUDIO_SPDIFIN_CTRL0, 0x3<<4, 0x1<<4);
#endif
	}
}

void aml_spdif_arb_config(struct aml_audio_controller *actrl)
{
	/* config ddr arb */
	aml_audiobus_write(actrl, EE_AUDIO_ARB_CTRL, 1<<31|0xff<<0);
}

int aml_spdifin_status_check(struct aml_audio_controller *actrl)
{
	unsigned int val;

	val = aml_audiobus_read(actrl,
		EE_AUDIO_SPDIFIN_STAT0);

	/* pr_info("\t--- spdif handles status0 %#x\n", val); */

	aml_audiobus_update_bits(actrl,
			EE_AUDIO_SPDIFIN_CTRL0,
			1<<26,
			1<<26);
	aml_audiobus_update_bits(actrl,
			EE_AUDIO_SPDIFIN_CTRL0,
			1<<26,
			0);

	return val;
}

void aml_spdif_fifo_reset(
	struct aml_audio_controller *actrl,
	int stream, int index)
{
	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		/* reset afifo */
		unsigned int offset, reg;

		offset = EE_AUDIO_SPDIFOUT_B_CTRL0 - EE_AUDIO_SPDIFOUT_CTRL0;
		reg = EE_AUDIO_SPDIFOUT_CTRL0 + offset * index;
		aml_audiobus_update_bits(actrl,
				reg, 3<<28, 0);
		aml_audiobus_update_bits(actrl,
				reg, 1<<29, 1<<29);
		aml_audiobus_update_bits(actrl,
				reg, 1<<28, 1<<28);
	} else {
		/* reset afifo */
		aml_audiobus_update_bits(actrl,
				EE_AUDIO_SPDIFIN_CTRL0, 3<<28, 0);
		aml_audiobus_update_bits(actrl,
				EE_AUDIO_SPDIFIN_CTRL0, 1<<29, 1<<29);
		aml_audiobus_update_bits(actrl,
				EE_AUDIO_SPDIFIN_CTRL0, 1<<28, 1<<28);
	}
}

void aml_spdif_fifo_ctrl(
	struct aml_audio_controller *actrl,
	int bitwidth,
	int stream,
	int index,
	unsigned int fifo_id)
{
	unsigned int frddr_type, toddr_type;

	switch (bitwidth) {
	case 8:
		frddr_type = 0;
		toddr_type = 0;
		break;
	case 16:
		frddr_type = 1;
		toddr_type = 1;
		break;
	case 24:
		frddr_type = 4;
		toddr_type = 4;
		break;
	case 32:
		frddr_type = 3;
		toddr_type = 3;
		break;
	default:
		pr_err("runtime format invalid bitwidth: %d\n",
			bitwidth);
		return;
	}

	pr_info("%s, bit depth:%d, frddr type:%d, toddr:type:%d\n",
	__func__, bitwidth, frddr_type, toddr_type);

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		unsigned int offset, reg;

		/* mask lane 0 L/R channels */
		offset = EE_AUDIO_SPDIFOUT_B_CTRL0 - EE_AUDIO_SPDIFOUT_CTRL0;
		reg = EE_AUDIO_SPDIFOUT_CTRL0 + offset * index;
		aml_audiobus_update_bits(actrl,
			reg,
			0x1<<29|0x1<<28|0x3<<21|0x1<<20|0x1<<19|0xff<<4,
			1<<29|1<<28|0x0<<21|0<<20|0<<19|0x3<<4);

		offset = EE_AUDIO_SPDIFOUT_B_CTRL1 - EE_AUDIO_SPDIFOUT_CTRL1;
		reg = EE_AUDIO_SPDIFOUT_CTRL1 + offset * index;
		aml_audiobus_update_bits(actrl,
			reg,
			0x3 << 24 | 0x1f << 8 | 0x7 << 4,
			fifo_id << 24 | (bitwidth - 1) << 8 | frddr_type<<4);

		offset = EE_AUDIO_SPDIFOUT_B_SWAP - EE_AUDIO_SPDIFOUT_SWAP;
		reg = EE_AUDIO_SPDIFOUT_SWAP + offset * index;
		aml_audiobus_write(actrl,
			reg,
			1<<4);
	} else {
		unsigned int lsb;

		if (bitwidth <= 24)
			lsb = 28 - bitwidth;
		else
			lsb = 4;

		// 250M
#ifdef G12A_PTM
		aml_audiobus_write(actrl,
			EE_AUDIO_SPDIFIN_CTRL1,
			25000 << 0);
#else
		aml_audiobus_write(actrl,
			EE_AUDIO_SPDIFIN_CTRL1,
			0xff << 20 | 25000 << 0);
#endif
		aml_audiobus_write(actrl,
			EE_AUDIO_SPDIFIN_CTRL2,
			140 << 20 | 100 << 10 | 86 << 0);

		aml_audiobus_write(actrl,
			EE_AUDIO_SPDIFIN_CTRL3,
			83 << 20 | 60 << 10 | 30 << 0);

		aml_audiobus_write(actrl,
			EE_AUDIO_SPDIFIN_CTRL4,
			(81<<24) | /* reg_sample_mode0_timer */
			(61<<16) | /* reg_sample_mode1_timer */
			(44<<8) | /* reg_sample_mode2_timer*/
			(42<<0)
			);

#ifdef G12A_PTM
		aml_audiobus_write(actrl,
			EE_AUDIO_SPDIFIN_CTRL5,
			(40<<24) |
			(20<<16) |
			(10<<8) |
			(0<<0)
			);
#else
		aml_audiobus_write(actrl,
			EE_AUDIO_SPDIFIN_CTRL5,
			(40<<24) | /* reg_sample_mode4_timer	  = 5[31:24]; */
			(20<<16) | /* reg_sample_mode5_timer	  = 5[23:16]; */
			(9<<8) |  /* reg_sample_mode6_timer   = 5[15:8]; */
			(0<<0)	   /* reg_sample_mode7_timer	  = 5[7:0]; */
			);
#endif

		aml_audiobus_update_bits(actrl,
			EE_AUDIO_SPDIFIN_CTRL0,
			0x3<<24|1<<12,
			3<<24|1<<12);
	}

}

int spdifin_get_mode(void)
{
	int mode_val = audiobus_read(EE_AUDIO_SPDIFIN_STAT0);

	mode_val >>= 28;
	mode_val &= 0x7;

	return mode_val;
}

int spdif_get_channel_status(int reg)
{
	return audiobus_read(reg);
}

void spdifin_set_channel_status(int ch, int bits)
{
	int ch_status_sel = (ch << 3 | bits) & 0xf;

	/*which channel status would be got*/
	audiobus_update_bits(EE_AUDIO_SPDIFIN_CTRL0,
		0xf << 8,
		ch_status_sel << 8);
}

void aml_spdifout_select_aed(bool enable, int spdifout_id)
{
	unsigned int offset, reg;

	/* select eq_drc output */
	offset = EE_AUDIO_SPDIFOUT_B_CTRL1 - EE_AUDIO_SPDIFOUT_CTRL1;
	reg = EE_AUDIO_SPDIFOUT_CTRL1 + offset * spdifout_id;
	audiobus_update_bits(reg, 0x1 << 31, enable << 31);
}

void aml_spdifout_get_aed_info(int spdifout_id,
	int *bitwidth, int *frddrtype)
{
	unsigned int reg, offset, val;

	offset = EE_AUDIO_SPDIFOUT_B_CTRL1
			- EE_AUDIO_SPDIFOUT_CTRL1;
	reg = EE_AUDIO_SPDIFOUT_CTRL1 + offset * spdifout_id;

	val = audiobus_read(reg);
	if (bitwidth)
		*bitwidth = (val >> 8) & 0x1f;
	if (frddrtype)
		*frddrtype = (val >> 4) & 0x7;
}

/*value for spdif_index is only 0, 1 */
void spdifoutb_to_hdmitx_ctrl(int spdif_index)
{
	audiobus_write(EE_AUDIO_TOHDMITX_CTRL0,
		1 << 31
		| 1 << 3 /* spdif_clk_cap_inv */
		| 0 << 2 /* spdif_clk_inv */
		| spdif_index << 1 /* spdif_out_b */
		| spdif_index << 0 /* spdif_clk_b */
	);
}

void spdifout_clk_ctrl(int spdif_id, bool is_enable)
{
	unsigned int offset, reg;

	offset = EE_AUDIO_CLK_SPDIFOUT_B_CTRL - EE_AUDIO_CLK_SPDIFOUT_CTRL;
	reg = EE_AUDIO_CLK_SPDIFOUT_CTRL + offset * spdif_id;

	/* select : mpll 0, 24m, so spdif clk:6m */
	audiobus_write(reg, is_enable << 31 | 0x0 << 24 | 0x3 << 0);
}

void spdifout_fifo_ctrl(int spdif_id, int fifo_id, int bitwidth)
{
	unsigned int frddr_type;
	unsigned int offset, reg;

	switch (bitwidth) {
	case 8:
		frddr_type = 0;
		break;
	case 16:
		frddr_type = 1;
		break;
	case 24:
		frddr_type = 4;
		break;
	case 32:
		frddr_type = 3;
		break;
	default:
		pr_err("runtime format invalid bitwidth: %d\n",
			bitwidth);
		return;
	}

	pr_info("spdif_%s fifo ctrl, frddr:%d type:%d, %d bits\n",
		(spdif_id == 0) ? "a":"b",
		fifo_id,
		frddr_type,
		bitwidth);

	/* mask lane 0 L/R channels */
	offset = EE_AUDIO_SPDIFOUT_B_CTRL0 - EE_AUDIO_SPDIFOUT_CTRL0;
	reg = EE_AUDIO_SPDIFOUT_CTRL0 + offset * spdif_id;
	audiobus_update_bits(reg,
		0x3<<21|0x1<<20|0x1<<19|0xff<<4,
		0x0<<21|0<<20|0<<19|0x3<<4);

	offset = EE_AUDIO_SPDIFOUT_B_CTRL1 - EE_AUDIO_SPDIFOUT_CTRL1;
	reg = EE_AUDIO_SPDIFOUT_CTRL1 + offset * spdif_id;
	audiobus_update_bits(reg,
		0x3 << 24 | 0x1f << 8 | 0x7 << 4,
		fifo_id << 24 | (bitwidth - 1) << 8 | frddr_type<<4);

	offset = EE_AUDIO_SPDIFOUT_B_SWAP - EE_AUDIO_SPDIFOUT_SWAP;
	reg = EE_AUDIO_SPDIFOUT_SWAP + offset * spdif_id;
	audiobus_write(reg, 1<<4);

	/* reset afifo */
	offset = EE_AUDIO_SPDIFOUT_B_CTRL0 - EE_AUDIO_SPDIFOUT_CTRL0;
	reg = EE_AUDIO_SPDIFOUT_CTRL0 + offset * spdif_id;
	audiobus_update_bits(reg, 3<<28, 0);
	audiobus_update_bits(reg, 1<<29, 1<<29);
	audiobus_update_bits(reg, 1<<28, 1<<28);
}

static void spdifout_mute(int spdif_id)
{
	unsigned int offset, reg;

	/* mute */
	offset = EE_AUDIO_SPDIFOUT_B_CTRL0 - EE_AUDIO_SPDIFOUT_CTRL0;
	reg = EE_AUDIO_SPDIFOUT_CTRL0 + offset * spdif_id;
	audiobus_update_bits(reg, 0x3 << 21, 0x3 << 21);
}

static bool spdifout_is_enable(int spdif_id)
{
	unsigned int offset, reg, val;

	offset = EE_AUDIO_SPDIFOUT_B_CTRL0 - EE_AUDIO_SPDIFOUT_CTRL0;
	reg = EE_AUDIO_SPDIFOUT_CTRL0 + offset * spdif_id;
	val = audiobus_read(reg);

	return ((val >> 31) == 1);
}

void spdifout_enable(int spdif_id, bool is_enable)
{
	unsigned int offset, reg;

	pr_info("spdif_%s is set to %s\n",
		(spdif_id == 0) ? "a":"b",
		is_enable ? "enable":"disable");

	offset = EE_AUDIO_SPDIFOUT_B_CTRL0 - EE_AUDIO_SPDIFOUT_CTRL0;
	reg = EE_AUDIO_SPDIFOUT_CTRL0 + offset * spdif_id;

	if (!is_enable) {
		/* share buffer, spdif should be active, so mute it */
		audiobus_update_bits(reg, 0x3 << 21, 0x3 << 21);
		return;
	}

	audiobus_update_bits(reg, 1<<31, is_enable<<31);
}

void spdifout_samesource_set(int spdif_index, int fifo_id,
	int bitwidth, bool is_enable)
{
	int spdif_id;

	if (spdif_index == 1)
		spdif_id = 1;
	else
		spdif_id = 0;

	/* clk for spdif_b is always on */
	if (!spdif_id)
		spdifout_clk_ctrl(spdif_id, is_enable);

	if (is_enable)
		spdifout_fifo_ctrl(spdif_id, fifo_id, bitwidth);
}

int spdifin_get_sample_rate(void)
{
	unsigned int val;

	val = audiobus_read(EE_AUDIO_SPDIFIN_STAT0);

	return (val >> 28) & 0x7;
}

int spdifin_get_audio_type(void)
{
	unsigned int val;

	/* set ch_status_sel to read Pc*/
	audiobus_update_bits(EE_AUDIO_SPDIFIN_CTRL0, 0xf << 8, 0x6 << 8);

	val = audiobus_read(EE_AUDIO_SPDIFIN_STAT1);

	return (val >> 16) & 0xff;
}

void spdif_set_channel_status_info(
	struct iec958_chsts *chsts, int spdif_id)
{
	unsigned int offset, reg;

	/* "ch status" = reg_chsts0~B */
	offset = EE_AUDIO_SPDIFOUT_B_CTRL0 - EE_AUDIO_SPDIFOUT_CTRL0;
	reg = EE_AUDIO_SPDIFOUT_CTRL0 + offset * spdif_id;
	audiobus_update_bits(reg, 0x1 << 24, 0x0 << 24);

	offset = EE_AUDIO_SPDIFOUT_B_CHSTS0 - EE_AUDIO_SPDIFOUT_CHSTS0;
	reg = EE_AUDIO_SPDIFOUT_CHSTS0 + offset * spdif_id;
	audiobus_write(reg, chsts->chstat1_l << 16 | chsts->chstat0_l);

	offset = EE_AUDIO_SPDIFOUT_B_CHSTS1 - EE_AUDIO_SPDIFOUT_CHSTS1;
	reg = EE_AUDIO_SPDIFOUT_CHSTS1 + offset * spdif_id;
	audiobus_write(reg, chsts->chstat1_l << 16 | chsts->chstat0_l);

	offset = EE_AUDIO_SPDIFOUT_B_CHSTS2 - EE_AUDIO_SPDIFOUT_CHSTS2;
	reg = EE_AUDIO_SPDIFOUT_CHSTS2 + offset * spdif_id;
	audiobus_write(reg, chsts->chstat1_l << 16 | chsts->chstat0_l);

	offset = EE_AUDIO_SPDIFOUT_B_CHSTS3 - EE_AUDIO_SPDIFOUT_CHSTS3;
	reg = EE_AUDIO_SPDIFOUT_CHSTS3 + offset * spdif_id;
	audiobus_write(reg, chsts->chstat1_l << 16 | chsts->chstat0_l);

	offset = EE_AUDIO_SPDIFOUT_B_CHSTS4 - EE_AUDIO_SPDIFOUT_CHSTS4;
	reg = EE_AUDIO_SPDIFOUT_CHSTS4 + offset * spdif_id;
	audiobus_write(reg, chsts->chstat1_l << 16 | chsts->chstat0_l);

	offset = EE_AUDIO_SPDIFOUT_B_CHSTS5 - EE_AUDIO_SPDIFOUT_CHSTS5;
	reg = EE_AUDIO_SPDIFOUT_CHSTS5 + offset * spdif_id;
	audiobus_write(reg, chsts->chstat1_l << 16 | chsts->chstat0_l);

	offset = EE_AUDIO_SPDIFOUT_B_CHSTS6 - EE_AUDIO_SPDIFOUT_CHSTS6;
	reg = EE_AUDIO_SPDIFOUT_CHSTS6 + offset * spdif_id;
	audiobus_write(reg, chsts->chstat1_r << 16 | chsts->chstat0_r);

	offset = EE_AUDIO_SPDIFOUT_B_CHSTS7 - EE_AUDIO_SPDIFOUT_CHSTS7;
	reg = EE_AUDIO_SPDIFOUT_CHSTS7 + offset * spdif_id;
	audiobus_write(reg, chsts->chstat1_r << 16 | chsts->chstat0_r);

	offset = EE_AUDIO_SPDIFOUT_B_CHSTS8 - EE_AUDIO_SPDIFOUT_CHSTS8;
	reg = EE_AUDIO_SPDIFOUT_CHSTS8 + offset * spdif_id;
	audiobus_write(reg, chsts->chstat1_r << 16 | chsts->chstat0_r);

	offset = EE_AUDIO_SPDIFOUT_B_CHSTS9 - EE_AUDIO_SPDIFOUT_CHSTS9;
	reg = EE_AUDIO_SPDIFOUT_CHSTS9 + offset * spdif_id;
	audiobus_write(reg, chsts->chstat1_r << 16 | chsts->chstat0_r);

	offset = EE_AUDIO_SPDIFOUT_B_CHSTSA - EE_AUDIO_SPDIFOUT_CHSTSA;
	reg = EE_AUDIO_SPDIFOUT_CHSTSA + offset * spdif_id;
	audiobus_write(reg, chsts->chstat1_r << 16 | chsts->chstat0_r);

	offset = EE_AUDIO_SPDIFOUT_B_CHSTSB - EE_AUDIO_SPDIFOUT_CHSTSB;
	reg = EE_AUDIO_SPDIFOUT_CHSTSB + offset * spdif_id;
	audiobus_write(reg, chsts->chstat1_r << 16 | chsts->chstat0_r);
}

void spdifout_play_with_zerodata(unsigned int spdif_id)
{
	pr_info("%s, spdif id:%d enable:%d\n",
		__func__,
		spdif_id,
		spdifout_is_enable(spdif_id));

	if (!spdifout_is_enable(spdif_id)) {
		unsigned int frddr_index = 0;
		unsigned int bitwidth = 32;
		unsigned int sample_rate = 48000;
		unsigned int src0_sel = 4; /* spdif b */
		struct iec958_chsts chsts;
		struct snd_pcm_substream substream;
		struct snd_pcm_runtime runtime;

		substream.runtime = &runtime;
		runtime.rate = 48000;
		runtime.format = SNDRV_PCM_FORMAT_S16_LE;
		runtime.channels = 2;
		runtime.sample_bits = 16;

		/* check whether fix to spdif a */
		if (spdif_id == 0)
			src0_sel = 3;

		/* spdif ctrl */
		spdifout_fifo_ctrl(spdif_id, frddr_index, bitwidth);

		/* channel status info */
		spdif_get_channel_status_info(&chsts, sample_rate);
		spdif_set_channel_status_info(&chsts, spdif_id);

		/* notify hdmitx audio */
		spdifoutb_to_hdmitx_ctrl(spdif_id);
		aout_notifier_call_chain(0x1, &substream);

		frddr_init_default(frddr_index, src0_sel);

		spdifout_enable(spdif_id, true);

		spdifout_mute(spdif_id);
	}
}
