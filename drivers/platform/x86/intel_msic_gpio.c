/* MSIC GPIO (access through IPC) driver for Cloverview
 * (C) Copyright 2011 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/ipc_device.h>
#include <linux/gpio.h>
#include <asm/intel_scu_ipc.h>
#include <linux/mfd/intel_msic.h>

#define DRIVER_NAME "msic_gpio"

#define GPIO0LV0CTLO		0x048
#define GPIO0HV0CTLO		0x06D
#define GPIO0LV0CTLI		0x058
#define GPIO0HV0CTLI		0x075

#define CTLO_DOUT_MASK		(1 << 0)
#define CTLO_DOUT_H		(1 << 0)
#define CTLO_DOUT_L		(0 << 0)
#define CTLO_DIR_MASK		(1 << 5)
#define CTLO_DIR_O		(1 << 5)
#define CTLO_DIR_I		(0 << 5)
#define CTLO_OUT_DEF		(0x38)
#define CTLO_IN_DEF		(0x18)

#define CTLI_DIN_MASK		(1 << 0)

struct msic_gpio {
	struct gpio_chip chip;
	int ngpio_lv; /* number of low voltage gpio */
};

static struct msic_gpio msic_gpio;

static int msic_gpio_direction_input(struct gpio_chip *chip, unsigned offset)
{
	struct msic_gpio *mg = &msic_gpio;

	u16 ctlo = offset < mg->ngpio_lv ? GPIO0LV0CTLO + offset
			: GPIO0HV0CTLO + (offset - mg->ngpio_lv);

	return intel_scu_ipc_iowrite8(ctlo, CTLO_IN_DEF);
}

static int msic_gpio_direction_output(struct gpio_chip *chip,
			unsigned offset, int value)
{
	struct msic_gpio *mg = &msic_gpio;

	u16 ctlo = offset < mg->ngpio_lv ? GPIO0LV0CTLO + offset
			: GPIO0HV0CTLO + (offset - mg->ngpio_lv);

	return intel_scu_ipc_iowrite8(ctlo,
			CTLO_OUT_DEF | (value ? CTLO_DOUT_H : CTLO_DOUT_L));
}

static int msic_gpio_get(struct gpio_chip *chip, unsigned offset)
{
	struct msic_gpio *mg = &msic_gpio;
	u8 value;

	u16 ctli = offset < mg->ngpio_lv ? GPIO0LV0CTLI + offset
			: GPIO0HV0CTLI + (offset - mg->ngpio_lv);

	if (intel_scu_ipc_ioread8(ctli, &value))
		return -EIO;

	return value & CTLI_DIN_MASK;
}

static void msic_gpio_set(struct gpio_chip *chip, unsigned offset, int value)
{
	struct msic_gpio *mg = &msic_gpio;

	u16 ctlo = offset < mg->ngpio_lv ? GPIO0LV0CTLO + offset
			: GPIO0HV0CTLO + (offset - mg->ngpio_lv);

	intel_scu_ipc_update_register(ctlo,
			value ? CTLO_DOUT_H : CTLO_DOUT_L, CTLO_DOUT_MASK);
}

static int __devinit ipc_msic_gpio_probe(struct ipc_device *ipcdev)
{
	struct device *dev = &ipcdev->dev;
	struct intel_msic_gpio_pdata *pdata = dev->platform_data;
	struct msic_gpio *mg = &msic_gpio;
	int retval;

	dev_dbg(dev, "base %d\n", pdata->gpio_base);

	if (!pdata || !pdata->gpio_base) {
		dev_err(dev, "incorrect or missing platform data\n");
		return -ENOMEM;
	}

	dev_set_drvdata(dev, mg);

	mg->ngpio_lv = pdata->ngpio_lv;
	mg->chip.label = dev_name(&ipcdev->dev);
	mg->chip.direction_input = msic_gpio_direction_input;
	mg->chip.direction_output = msic_gpio_direction_output;
	mg->chip.get = msic_gpio_get;
	mg->chip.set = msic_gpio_set;
	mg->chip.base = pdata->gpio_base;
	mg->chip.ngpio = pdata->ngpio_lv + pdata->ngpio_hv;
	mg->chip.can_sleep = pdata->can_sleep;
	mg->chip.dev = dev;

	retval = gpiochip_add(&mg->chip);
	if (retval)
		dev_err(dev, "%s: Can not add msic gpio chip.\n", __func__);

	return retval;
}

static int __devexit ipc_msic_gpio_remove(struct ipc_device *ipcdev)
{
	struct device *dev = &ipcdev->dev;

	dev_set_drvdata(dev, NULL);
	return 0;
}

static struct ipc_driver ipc_msic_gpio_driver = {
	.driver = {
		.name		= DRIVER_NAME,
		.owner		= THIS_MODULE,
	},
	.probe		= ipc_msic_gpio_probe,
	.remove		= __devexit_p(ipc_msic_gpio_remove),
};

static int __init ipc_msic_gpio_init(void)
{
	return ipc_driver_register(&ipc_msic_gpio_driver);
}

static void __init ipc_msic_gpio_exit(void)
{
	return ipc_driver_unregister(&ipc_msic_gpio_driver);
}

subsys_initcall(ipc_msic_gpio_init);
module_exit(ipc_msic_gpio_exit);

MODULE_AUTHOR("Bin Yang <bin.yang@intel.com>");
MODULE_DESCRIPTION("Intel MSIC GPIO driver");
MODULE_LICENSE("GPL v2");
