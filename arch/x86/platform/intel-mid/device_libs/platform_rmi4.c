/*
 * platform_s3202.c: s3202 platform data initilization file
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
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/lnw_gpio.h>
#include <linux/gpio.h>
#include <linux/rmi.h>
#include <linux/rmi_platformdata.h>
#include <asm/intel-mid.h>
#include <linux/input.h>
#include <linux/delay.h>
/*#include "platform_s3202.h"*/

#define MRST_IRQ_OFFSET 0x100
#define RMI_INT_GPIO 62
#define GPIO_RMI4_RESET 58
struct syna_gpio_data{
	u16 gpio_number;
	char* gpio_name;
};
static int rmi_touchpad_gpio_setup(void *gpio_data, bool configure)
{
	int err = 0;
	struct syna_gpio_data *data = gpio_data;
	if(configure == true){
		err = gpio_request(data->gpio_number,"rmi4_int");	
		if(err < 0){
			printk("request gpio rmi_int err.\n");
			return err;
		}	
		err = gpio_direction_input(data->gpio_number);
		if(err < 0){
			printk("set rmi_int direction err.\n");
			return err;
		}

		err = gpio_request(GPIO_RMI4_RESET, "Rmi4-reset");
		if (err < 0){
			printk(KERN_ERR "Failed to request GPIO%d (MaxTouch-reset) err=%d\n",
				GPIO_RMI4_RESET, err);
			return err;
		}

		err = gpio_direction_output(GPIO_RMI4_RESET, 0);
		if (err){
			printk(KERN_ERR "Failed to change direction, err=%d\n", err);
			return err;
		}

		/* maXTouch wants 40mSec minimum after reset to get organized */
		gpio_set_value(GPIO_RMI4_RESET, 1);
		msleep(40);
		return err;
	}else{
		gpio_free(data->gpio_number);	
		gpio_free(GPIO_RMI4_RESET);	
		printk("s3202 free requested gpios.\n");
	}

}
static struct syna_gpio_data rmi_gpiodata = {
	.gpio_number = RMI_INT_GPIO,
	.gpio_name = "rmi_int",
};
static unsigned char rmi4_f1a_button_codes[] = {KEY_MENU,KEY_HOMEPAGE,KEY_BACK};
static struct rmi_f1a_button_map rmi4_f1a_button_map = {
	.nbuttons = ARRAY_SIZE(rmi4_f1a_button_codes),
	.map = rmi4_f1a_button_codes,
};
static struct rmi_device_platform_data s3202_i2c_platform_data = {
	.sensor_name = "rmi_i2c_tp",
	.driver_name = "rmi_generic",
	.attn_gpio = RMI_INT_GPIO,
	.attn_polarity = RMI_ATTN_ACTIVE_LOW,
	.gpio_data = &rmi_gpiodata,
	.gpio_config = rmi_touchpad_gpio_setup,
	.reset_delay_ms = 100,
	.f1a_button_map = &rmi4_f1a_button_map,
};
struct i2c_board_info s3202_i2c_info[] __initdata = {   
		{
                I2C_BOARD_INFO("rmi", 0x38),
			 	.platform_data = &s3202_i2c_platform_data,
				.irq = RMI_INT_GPIO + MRST_IRQ_OFFSET,
		},
};   
