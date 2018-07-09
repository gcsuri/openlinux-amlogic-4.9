/*
 * drivers/amlogic/media/vout/backlight/bl_extern/bl_extern.c
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/amlogic/i2c-amlogic.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/amlogic/media/vout/lcd/aml_bl_extern.h>
#include <linux/amlogic/media/vout/lcd/lcd_unifykey.h>
#include <linux/amlogic/media/vout/lcd/aml_bl.h>
#include "bl_extern.h"

unsigned int bl_extern_brightness;
static struct aml_bl_extern_driver_s bl_extern_driver;

static int bl_extern_set_level(unsigned int level)
{
	bl_extern_brightness = level & 0xff;

	if (bl_extern_driver.device_bri_update)
		bl_extern_driver.device_bri_update(level);

	return 0;
}

static int bl_extern_power_on(void)
{
	int ret = 0;

	BLEX("%s\n", __func__);

	if (bl_extern_driver.device_power_on)
		bl_extern_driver.device_power_on();

	/* restore bl level */
	bl_extern_set_level(bl_extern_brightness);

	return ret;
}
static int bl_extern_power_off(void)
{
	int ret = 0;

	BLEX("%s\n", __func__);

	if (bl_extern_driver.device_power_off)
		bl_extern_driver.device_power_off();

	return ret;
}

static struct aml_bl_extern_driver_s bl_extern_driver = {
	.power_on = bl_extern_power_on,
	.power_off = bl_extern_power_off,
	.set_level = bl_extern_set_level,
	.config_print = NULL,
	.device_power_on = NULL,
	.device_power_off = NULL,
	.device_bri_update = NULL,
	.config = {
		.index = BL_EXTERN_INDEX_INVALID,
		.name = "none",
		.type = BL_EXTERN_MAX,
		.i2c_addr = 0xff,
		.i2c_bus = BL_EXTERN_I2C_BUS_MAX,
		.dim_min = 10,
		.dim_max = 255,
	},
};

struct aml_bl_extern_driver_s *aml_bl_extern_get_driver(void)
{
	return &bl_extern_driver;
}

static unsigned char bl_extern_get_i2c_bus_str(const char *str)
{
	unsigned char i2c_bus;

	if (strncmp(str, "i2c_bus_ao", 10) == 0)
		i2c_bus = AML_I2C_MASTER_AO;
	else if (strncmp(str, "i2c_bus_a", 9) == 0)
		i2c_bus = AML_I2C_MASTER_A;
	else if (strncmp(str, "i2c_bus_b", 9) == 0)
		i2c_bus = AML_I2C_MASTER_B;
	else if (strncmp(str, "i2c_bus_c", 9) == 0)
		i2c_bus = AML_I2C_MASTER_C;
	else if (strncmp(str, "i2c_bus_d", 9) == 0)
		i2c_bus = AML_I2C_MASTER_D;
	else {
		i2c_bus = AML_I2C_MASTER_A;
		BLEXERR("invalid i2c_bus: %s\n", str);
	}

	return i2c_bus;
}

static void bl_extern_config_print(void)
{
	struct aml_bl_extern_driver_s *bl_extern = aml_bl_extern_get_driver();
	struct aml_bl_extern_i2c_dev_s *i2c_dev = aml_bl_extern_i2c_get_dev();

	BLEX("%s:\n", __func__);
	switch (bl_extern->config.type) {
	case BL_EXTERN_I2C:
		pr_info("index:     %d\n"
			"name:          %s\n"
			"type:          i2c(%d)\n"
			"i2c_addr:      0x%02x\n"
			"i2c_bus:       %d\n"
			"dim_min:       %d\n"
			"dim_max:       %d\n",
			bl_extern->config.index,
			bl_extern->config.name,
			bl_extern->config.type,
			bl_extern->config.i2c_addr,
			bl_extern->config.i2c_bus,
			bl_extern->config.dim_min,
			bl_extern->config.dim_max);
		if (i2c_dev) {
			pr_info("i2c_dev_name:      %s\n"
				"i2c_client_name:   %s\n"
				"i2c_client_addr:   0x%02x\n",
				i2c_dev->name,
				i2c_dev->client->name,
				i2c_dev->client->addr);
		} else {
			pr_info("invalid i2c device\n");
		}
		break;
	case BL_EXTERN_SPI:
		break;
	case BL_EXTERN_MIPI:
		pr_info("index:     %d\n"
			"name:          %s\n"
			"type:          mipi(%d)\n"
			"dim_min:       %d\n"
			"dim_max:       %d\n",
			bl_extern->config.index,
			bl_extern->config.name,
			bl_extern->config.type,
			bl_extern->config.dim_min,
			bl_extern->config.dim_max);

		break;
	default:
		break;
	}
}

static int bl_extern_config_from_dts(struct device_node *np, int index)
{
	char be_propname[20];
	struct device_node *child;
	const char *str;
	unsigned int temp[5], val;
	int ret = 0;
	struct aml_bl_extern_driver_s *bl_extern = aml_bl_extern_get_driver();

	/* get device config */
	sprintf(be_propname, "extern_%d", index);
	BLEX("load: %s\n", be_propname);
	child = of_get_child_by_name(np, be_propname);
	if (child == NULL) {
		BLEXERR("failed to get %s\n", be_propname);
		return -1;
	}
	ret = of_property_read_u32_array(child, "index", &temp[0], 1);
	if (ret)
		BLEXERR("failed to get index, exit\n");
	else {
		if (temp[0] == index)
			bl_extern->config.index = temp[0];
		else {
			BLEXERR("index not match, exit\n");
			return -1;
			}
		}
	ret = of_property_read_string(child, "extern_name", &str);
	if (ret) {
		BLEXERR("failed to get bl_extern_name\n");
		str = "bl_extern_name";
	}
	strcpy(bl_extern->config.name, str);

	ret = of_property_read_u32(child, "type", &val);
	if (ret) {
		BLEXERR("failed to get type\n");
	} else {
		bl_extern->config.type = val;
		BLEX("type: %d\n", bl_extern->config.type);
	}
	if (bl_extern->config.type >= BL_EXTERN_MAX) {
		BLEXERR("type num is out of support\n");
		return -1;
	}

	ret = of_property_read_u32_array(child, "dim_max_min", &temp[0], 2);
	if (ret) {
		BLEXERR("failed to get dim_max_min\n");
		bl_extern->config.dim_max = 255;
		bl_extern->config.dim_min = 10;
	} else {
		bl_extern->config.dim_max = temp[0];
		bl_extern->config.dim_min = temp[1];
	}

	switch (bl_extern->config.type) {
	case BL_EXTERN_I2C:
		ret = of_property_read_u32(child, "i2c_address", &val);
		if (ret) {
			BLEXERR("failed to get i2c_address\n");
		} else {
			bl_extern->config.i2c_addr = (unsigned char)val;
		}
		if (lcd_debug_print_flag) {
			BLEX("%s i2c_address=0x%02x\n",
				bl_extern->config.name,
				bl_extern->config.i2c_addr);
		}

		ret = of_property_read_string(child, "i2c_bus", &str);
		if (ret) {
			BLEXERR("failed to get i2c_bus\n");
		} else {
			bl_extern->config.i2c_bus =
				bl_extern_get_i2c_bus_str(str);
		}
		if (lcd_debug_print_flag) {
			BLEX("%s i2c_bus=%s[%d]\n",
				bl_extern->config.name, str,
				bl_extern->config.i2c_bus);
		}
		break;
	case BL_EXTERN_SPI:
		break;
	case BL_EXTERN_MIPI:
		break;
	default:
		break;
	}
	return 0;
}

static int bl_extern_add_driver(void)
{
	int ret = 0;
	struct bl_extern_config_s *extconf = &bl_extern_driver.config;

	if (strcmp(extconf->name, "i2c_lp8556") == 0) {
#ifdef CONFIG_AMLOGIC_BL_EXTERN_I2C_LP8556
		ret = i2c_lp8556_probe();
#endif
		goto bl_extern_add_driver_next;
	} else if (strcmp(extconf->name, "mipi_lt070me05") == 0) {
#ifdef CONFIG_AMLOGIC_BL_EXTERN_MIPI_LT070ME05
		ret = mipi_lt070me05_probe();
#endif
		goto bl_extern_add_driver_next;
	} else {
		BLEXERR("invalid device name: %s\n", extconf->name);
		ret = -1;
	}

bl_extern_add_driver_next:
	if (ret) {
		BLEXERR("add device driver failed %s(%d)\n",
			extconf->name, extconf->index);
	} else {
		BLEX("add device driver %s(%d)\n",
			extconf->name, extconf->index);
	}

	return ret;
}

static int bl_extern_remove_driver(void)
{
	int ret = 0;
	struct bl_extern_config_s *extconf = &bl_extern_driver.config;

	if (strcmp(extconf->name, "i2c_lp8556") == 0) {
#ifdef CONFIG_AMLOGIC_BL_EXTERN_I2C_LP8556
		ret = i2c_lp8556_remove();
#endif
		goto bl_extern_remove_driver_next;
	} else if (strcmp(extconf->name, "mipi_lt070me05") == 0) {
#ifdef CONFIG_AMLOGIC_BL_EXTERN_MIPI_LT070ME05
		ret = mipi_lt070me05_remove();
#endif
		goto bl_extern_remove_driver_next;
	} else {
		BLEXERR("invalid device name: %s\n", extconf->name);
		ret = -1;
	}

bl_extern_remove_driver_next:
	if (ret) {
		BLEXERR("remove device driver failed %s(%d)\n",
			extconf->name, extconf->index);
	} else {
		BLEX("remove device driver %s(%d)\n",
			extconf->name, extconf->index);
	}

	return ret;
}

int aml_bl_extern_device_load(int index)
{
	int ret = 0;

	bl_extern_config_from_dts(bl_extern_driver.dev->of_node,
		index);
	bl_extern_add_driver();
	bl_extern_driver.config_print = bl_extern_config_print;
	BLEX("%s OK\n", __func__);

	return ret;
}

int aml_bl_extern_probe(struct platform_device *pdev)
{
	int ret = 0;

	bl_extern_driver.dev = &pdev->dev;
	BLEX("%s OK\n", __func__);

	return ret;
}

static int aml_bl_extern_remove(struct platform_device *pdev)
{
	int ret = 0;

	bl_extern_remove_driver();

	BLEX("%s OK\n", __func__);
	return ret;
}

#ifdef CONFIG_OF
static const struct of_device_id aml_bl_extern_dt_match[] = {
	{
		.compatible = "amlogic, bl_extern",
	},
	{},
};
#endif

static struct platform_driver aml_bl_extern_driver = {
	.probe  = aml_bl_extern_probe,
	.remove = aml_bl_extern_remove,
	.driver = {
		.name  = "bl_extern",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = aml_bl_extern_dt_match,
#endif
	},
};

static int __init aml_bl_extern_init(void)
{
	int ret;

	if (lcd_debug_print_flag)
		BLEX("%s\n", __func__);

	ret = platform_driver_register(&aml_bl_extern_driver);
	if (ret) {
		BLEXERR("driver register failed\n");
		return -ENODEV;
	}
	return ret;
}

static void __exit aml_bl_extern_exit(void)
{
	platform_driver_unregister(&aml_bl_extern_driver);
}

module_init(aml_bl_extern_init);
module_exit(aml_bl_extern_exit);

MODULE_AUTHOR("AMLOGIC");
MODULE_DESCRIPTION("bl extern driver");
MODULE_LICENSE("GPL");

