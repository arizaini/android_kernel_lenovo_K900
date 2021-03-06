/*
 * platform_lm3559.c: lm3559 platform data initilization file
 *
 * (C) Copyright 2012 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/init.h>
#include <linux/types.h>
#include <asm/intel-mid.h>

#include <media/lm3559.h>

#include "platform_lm3559.h"

void *lm3559_platform_data_func(void *info)
{
	static struct lm3559_platform_data platform_data;

	platform_data.gpio_reset  = get_gpio_by_name("GP_FLASH_RESET");
	platform_data.gpio_strobe = get_gpio_by_name("GP_FLASH_STROBE");
	platform_data.gpio_torch  = get_gpio_by_name("GP_FLASH_TORCH");

	if (platform_data.gpio_reset == -1) {
		pr_err("%s: Unable to find GP_FLASH_RESET\n", __func__);
		return NULL;
	}
	if (platform_data.gpio_strobe == -1) {
		pr_err("%s: Unable to find GP_FLASH_STROBE\n", __func__);
		return NULL;
	}
	if (platform_data.gpio_torch == -1) {
		pr_err("%s: Unable to find GP_FLASH_TORCH\n", __func__);
		return NULL;
	}

	/* Set to TX2 mode, then ENVM/TX2 pin is a power amplifier sync input:
	 * ENVM/TX pin asserted, flash forced into torch;
	 * ENVM/TX pin desserted, flash set back;
	 */
	platform_data.envm_tx2 = 1;
	platform_data.tx2_polarity = 0;

	/* set peak current limit to be 1000mA */
	platform_data.current_limit = 0;

	return &platform_data;
}
