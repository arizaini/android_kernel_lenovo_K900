/*
 * platform_mxt224.c: mxt224 platform data initilization file
 *
 * (C) Copyright 2008 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/lnw_gpio.h>
#include <asm/intel-mid.h>
#include <linux/atmel_mxt224.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/atmel_qt602240.h>
#include "platform_mxt224.h"

#define ATMEL_GPIO_IRQ	62
#define MRST_IRQ_OFFSET 0x100
static  int mxt_reset_gpio;

void mxt_init_platform_hw(void)
{
	int err;
	printk("wyh enter mxt_init_platform_hw.\n");
	mxt_reset_gpio = 58;
	err = gpio_request(mxt_reset_gpio, "MaxTouch-reset");
	if (err < 0)
		printk(KERN_ERR "Failed to request GPIO%d (MaxTouch-reset) err=%d\n",
			mxt_reset_gpio, err);

	err = gpio_direction_output(mxt_reset_gpio, 0);
	if (err)
		printk(KERN_ERR "Failed to change direction, err=%d\n", err);

	/* maXTouch wants 40mSec minimum after reset to get organized */
	gpio_set_value(mxt_reset_gpio, 1);
	msleep(40);
}

static struct atmel_i2c_platform_data mfld_mxt_platform_data = 
{
	.version = 0x020,
	.abs_x_min = 0,
	.abs_x_max = 720,
	.abs_y_min = 0,
	.abs_y_max = 1280,
	.abs_pressure_min = 0,
	.abs_pressure_max = 255,
	.abs_width_min = 0,
	.abs_width_max = 20, 
	.gpio_irq = ATMEL_GPIO_IRQ,
	.power = mxt_init_platform_hw,

	/*
	.config_T6 = {0, 0, 0, 0, 0, 0}, 
	.config_T7 = {255, 255, 10},
	.config_T8 = {36, 0, 20, 20, 0, 0, 10, 40, 20, 192},
	.config_T9 = {139, 0, 0, 19, 11, 0, 16, 70, 3, 7, 0, 5, 1, 0, 5, 10, 20, 16, 0x46, 5, 0xcf, 2, 0, 0, 0, 0, 0, 0, 0, 0, 20, 19, 0, 0, 0}, 
	.config_T15 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 
	.config_T18 = {0, 0},
	.config_T19 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 
	.config_T23 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 
	.config_T25 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 

	.config_T38 = {0, 0, 0, 0, 0, 0, 0, 0},
	.config_T40 = {0, 0, 0, 0, 0},
	.config_T42 = {0, 0, 0, 0, 0, 0, 0, 0},
	.config_T46 = {0, 3, 32, 48, 0, 0, 0, 0, 0},
	.config_T47 = {1, 20, 60, 5, 2, 50, 40, 0, 0, 40},
	.config_T48 = {1, 132, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 16, 8, 0, 0, 100, 10, 80, 10, 0, 20, 5, 0, 38, 0, 5, 0, 0, 0, 0, 0, 0, 0, 15, 15, 154, 58, 0, 80, 100, 15, 3, 0, 0, 0, 0, 0, 0, 0, 0, 19, 0},
	*/
	.config_T6 = {0, 0, 0, 0, 0, 0},  
	.config_T7 = {255, 255, 10}, 
	.config_T8 = {28, 0, 5, 5, 0, 0, 10, 40, 30, 230},
	.config_T9 = {139, 0, 0, 19, 11, 0, 16, 45, 3, 7, 0, 5, 2, 1, 5, 10, 20, 16, 0x4c, 5, 0xcf, 2, 1, 1, 0x0A, 0x0A, 0xAC, 0x26, 0xAC, 0x42, 0x1E, 0x14, 0x38, 0x34, 0}, 
	.config_T15 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},  
	.config_T18 = {0, 0},  
	.config_T19 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},  
	.config_T23 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},  
	.config_T25 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},  

	.config_T38 = {0, 0, 0, 0, 0, 0, 0, 0},  
	.config_T40 = {0, 0, 0, 0, 0},  
	.config_T42 = {0, 0, 0, 0, 0, 0, 0, 0},  
	.config_T46 = {0, 3, 32, 48, 0, 0, 0, 0, 0},  
	.config_T47 = {0, 20, 60, 5, 2, 50, 40, 0, 0, 40}, 
	.config_T48 = {3, 128, 194, 0, 0, 0, 0, 0, 2, 5, 0x10, 60, 0, 16, 16, 0, 0, 0x64, 10, 80, 10, 0, 20, 5, 0, 38, 0, 5, 0, 0, 0, 0, 0, 0, 0, 65, 3, 1, 1, 0, 5, 15, 20, 0, 0, 0, 0, 0x99, 0x37, 0xAC, 0x4B, 0x1E, 20, 2}, 
	.config_T55 = {1, 40, 0, 0},
	.object_crc = {0x1f, 0x2f, 0xe3},
};
void *mxt224_platform_data(void *info)
{
	struct i2c_board_info *i2c_info = (struct i2c_board_info *) info;
	int intr = 0;

	printk("In %s.", __func__);
	intr = get_gpio_by_name("TS0-intr");
	if (intr == -1) {
		printk(KERN_ERR "Missing GPIO(s) for MXT.\n");
		return NULL;
	}

	/* On nCDK EB2.0, Atmel max touch reset is on
	GP_CORE_033 = 33 + 96 = 129 */
	mxt_reset_gpio = 58; /* TODO: Use SFI, when SFI is available */

//	i2c_info->irq = intr + MRST_IRQ_OFFSET;
	return &mfld_mxt_platform_data;
}
struct i2c_board_info mxt224e_i2c_info[] __initdata = {   
		{
                I2C_BOARD_INFO("mxt224e", 0x4a),
			 	.platform_data = &mfld_mxt_platform_data,
				.irq =ATMEL_GPIO_IRQ + MRST_IRQ_OFFSET,
		},
};   

