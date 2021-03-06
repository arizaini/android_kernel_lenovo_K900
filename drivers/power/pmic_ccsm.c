/*
 * pmic_ccsm.c - Intel MID PMIC Charger Driver
 *
 * Copyright (C) 2011 Intel Corporation
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Author: Jenny TC <jenny.tc@intel.com>
 * Author: Yegnesh Iyer <yegnesh.s.iyer@intel.com>
 */

/* Includes */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/kfifo.h>
#include <linux/param.h>
#include <linux/device.h>
#include <linux/ipc_device.h>
#include <linux/usb/otg.h>
#include <linux/power_supply.h>
#include <linux/wakelock.h>
#include <linux/power/charger_helper.h>
#include <linux/power_supply.h>
#include <asm/intel_basincove_gpadc.h>
#include <asm/intel_scu_ipc.h>
#include <linux/io.h>
#include <linux/power/intel_mid_powersupply.h>
#include <linux/sched.h>
#include <linux/pm_runtime.h>
#include <linux/sfi.h>
#include <linux/async.h>
#include <linux/reboot.h>
#include <linux/notifier.h>
#include <linux/power/battery_id.h>
#include "pmic_ccsm.h"

/* Macros */
#define DRIVER_NAME "pmic_ccsm"
#define PMIC_SRAM_INTR_ADDR 0xFFFFF616
#define ADC_TO_TEMP 1
#define TEMP_TO_ADC 0
#define is_valid_temp(tmp)\
	(!(tmp > adc_tbl[0].temp ||\
		tmp < adc_tbl[ARRAY_SIZE(adc_tbl) - 1].temp))
#define is_valid_adc_code(val)\
	(!(val < adc_tbl[0].adc_val ||\
		val > adc_tbl[ARRAY_SIZE(adc_tbl) - 1].adc_val))
#define CONVERT_ADC_TO_TEMP(adc_val, temp)\
	adc_temp_conv(adc_val, temp, ADC_TO_TEMP)
#define CONVERT_TEMP_TO_ADC(temp, adc_val)\
	adc_temp_conv(temp, adc_val, TEMP_TO_ADC)
#define NEED_ZONE_SPLIT(bprof)\
	 ((bprof->temp_mon_ranges < MIN_BATT_PROF))

/* Type definitions */
static void pmic_bat_zone_changed(void);
static void pmic_battery_overheat_handler(bool);

/* Extern definitions */

/* Global declarations */
static DEFINE_MUTEX(pmic_lock);
static struct pmic_chrgr_drv_context chc;
static struct interrupt_info chgrirq0_info[] = {
	{
		CHGIRQ0_BZIRQ_MASK,
		0,
		"Battery temperature zone changed",
		NULL,
		NULL,
		pmic_bat_zone_changed,
		NULL,
	},
	{
		CHGIRQ0_BAT_CRIT_MASK,
		SCHGIRQ0_SBAT_CRIT_MASK,
		NULL,
		"Battery Over heat exception",
		"Battery Over heat exception Recovered",
		NULL,
		pmic_battery_overheat_handler
	},
	{
		CHGIRQ0_BAT0_ALRT_MASK,
		SCHGIRQ0_SBAT0_ALRT_MASK,
		NULL,
		"Battery0 temperature inside boundary",
		"Battery0 temperature outside boundary",
		NULL,
		pmic_battery_overheat_handler
	},
	{
		CHGIRQ0_BAT1_ALRT_MASK,
		SCHGIRQ0_SBAT1_ALRT_MASK,
		NULL,
		"Battery1 temperature inside boundary",
		"Battery1 temperature outside boundary",
		NULL,
		NULL
	},
};

static struct temp_lookup adc_tbl[] = {
	{0x24, 125, 0}, {0x28, 120, 0},
	{0x2D, 115, 0}, {0x32, 110, 0},
	{0x38, 105, 0}, {0x40, 100, 0},
	{0x48, 95, 0}, {0x51, 90, 0},
	{0x5C, 85, 0}, {0x68, 80, 0},
	{0x77, 75, 0}, {0x87, 70, 0},
	{0x99, 65, 0}, {0xAE, 60, 0},
	{0xC7, 55, 0}, {0xE2, 50, 0},
	{0x101, 45, 0}, {0x123, 40, 0},
	{0x149, 35, 0}, {0x172, 30, 0},
	{0x19F, 25, 0}, {0x1CE, 20, 0},
	{0x200, 15, 0}, {0x233, 10, 0},
	{0x266, 5, 0}, {0x299, 0, 0},
	{0x2CA, -5, 0}, {0x2F9, -10, 0},
	{0x324, -15, 0}, {0x34B, -20, 0},
	{0x36D, -25, 0}, {0x38A, -30, 0},
	{0x3A4, -35, 0}, {0x3B8, -40, 0},
};

u16 pmic_inlmt[][2] = {
	{ 100, CHGRCTRL1_FUSB_INLMT_100_MASK},
	{ 150, CHGRCTRL1_FUSB_INLMT_150_MASK},
	{ 500, CHGRCTRL1_FUSB_INLMT_500_MASK},
	{ 900, CHGRCTRL1_FUSB_INLMT_900_MASK},
	{ 1500, CHGRCTRL1_FUSB_INLMT_1500_MASK},
};

static inline struct power_supply *get_psy_battery(void)
{
	struct class_dev_iter iter;
	struct device *dev;
	struct power_supply *pst;

	class_dev_iter_init(&iter, power_supply_class, NULL, NULL);
	while ((dev = class_dev_iter_next(&iter))) {
		pst = (struct power_supply *)dev_get_drvdata(dev);
		if (pst->type == POWER_SUPPLY_TYPE_BATTERY) {
			class_dev_iter_exit(&iter);
			return pst;
		}
	}
	class_dev_iter_exit(&iter);

	return NULL;
}


/* Function definitions */
static void lookup_regval(u16 tbl[][2], size_t size, u16 in_val, u8 *out_val)
{
	int i;
	for (i = 1; i < size; ++i)
		if (in_val < tbl[i][0])
			break;

	*out_val = (u8)tbl[i-1][1];
}

static int interpolate_y(int dx1x0, int dy1y0, int dxx0, int y0)
{
	return y0 + DIV_ROUND_CLOSEST((dxx0 * dy1y0), dx1x0);
}

static int interpolate_x(int dy1y0, int dx1x0, int dyy0, int x0)
{
	return x0 + DIV_ROUND_CLOSEST((dyy0 * dx1x0), dy1y0);
}

static int adc_temp_conv(int in_val, int *out_val, int conv)
{
	int tbl_row_cnt = ARRAY_SIZE(adc_tbl), i;

	if (conv == ADC_TO_TEMP) {
		if (!is_valid_adc_code(in_val))
			return -ERANGE;

		if (in_val == adc_tbl[tbl_row_cnt-1].adc_val)
			i = tbl_row_cnt - 1;
		else {
			for (i = 0; i < tbl_row_cnt; ++i)
				if (in_val < adc_tbl[i].adc_val)
					break;
		}

		*out_val =
		    interpolate_y((adc_tbl[i].adc_val - adc_tbl[i - 1].adc_val),
				  (adc_tbl[i].temp - adc_tbl[i - 1].temp),
				  (in_val - adc_tbl[i - 1].adc_val),
				  adc_tbl[i - 1].temp);
	} else {
		if (!is_valid_temp(in_val))
			return -ERANGE;

		if (in_val == adc_tbl[tbl_row_cnt-1].temp)
			i = tbl_row_cnt - 1;
		else {
			for (i = 0; i < tbl_row_cnt; ++i)
				if (in_val > adc_tbl[i].temp)
					break;
		}

		*((short int *)out_val) =
		    interpolate_x((adc_tbl[i].temp - adc_tbl[i - 1].temp),
				  (adc_tbl[i].adc_val - adc_tbl[i - 1].adc_val),
				  (in_val - adc_tbl[i - 1].temp),
				  adc_tbl[i - 1].adc_val);
	}
	return 0;
}

static int pmic_read_reg(u16 addr, u8 *val)
{
	int ret;

	ret = intel_scu_ipc_ioread8(addr, val);
	if (ret) {
		dev_err(chc.dev,
			"Error in intel_scu_ipc_ioread8 0x%.4x\n", addr);
		return -EIO;
	}
	return 0;
}


static int __pmic_write_tt(u8 addr, u8 data)
{
	int ret;

	ret = intel_scu_ipc_iowrite8(CHRTTADDR_ADDR, addr);
	if (unlikely(ret))
		return ret;
	msleep(100);

	ret = intel_scu_ipc_iowrite8(CHRTTDATA_ADDR, data);
	msleep(100);

	return ret;
}

static inline int pmic_write_tt(u8 addr, u8 data)
{
	int ret;

	mutex_lock(&pmic_lock);
	ret = __pmic_write_tt(addr, data);
	mutex_unlock(&pmic_lock);

	return ret;
}

static int __pmic_read_tt(u8 addr, u8 *data)
{
	int ret;

	ret = intel_scu_ipc_iowrite8(CHRTTADDR_ADDR, addr);
	if (ret)
		return ret;

	usleep_range(2000, 3000);

	return intel_scu_ipc_ioread8(CHRTTDATA_ADDR, data);
}

static inline int pmic_read_tt(u8 addr, u8 *data)
{
	int ret;

	mutex_lock(&pmic_lock);
	ret = __pmic_read_tt(addr, data);
	mutex_unlock(&pmic_lock);

	return ret;
}

static int pmic_update_tt(u8 addr, u8 mask, u8 data)
{
	u8 tdata;
	int ret;

	mutex_lock(&pmic_lock);
	ret = __pmic_read_tt(addr, &data);
	if (unlikely(ret))
		goto exit;

	tdata = (tdata & mask) | data;
	ret = __pmic_write_tt(addr, data);
exit:
	mutex_unlock(&pmic_lock);
	return ret;
}

#ifdef CONFIG_DEBUG_FS
static int pmic_chrgr_reg_show(struct seq_file *seq, void *unused)
{
	int ret;
	long addr;
	u8 val;

	addr = *((u8 *)seq->private);

	if ((addr == CHRGRIRQ1_ADDR) || (addr == CHGRIRQ0_ADDR))
		val = ioread16(chc.pmic_intr_iomap);
	else {
		ret = pmic_read_reg(addr, &val);
		if (ret != 0) {
			dev_err(chc.dev,
				"Error reading tt register 0x%2x\n",
				addr);
			return -EIO;
		}
	}

	seq_printf(seq, "0x%x\n", val);
	return 0;
}

static int pmic_chrgr_tt_reg_show(struct seq_file *seq, void *unused)
{
	int ret;
	long addr;
	u8 val;

	addr = *((u8 *)seq->private);

	if ((addr == CHRGRIRQ1_ADDR) || (addr == CHGRIRQ0_ADDR))
		val = ioread16(chc.pmic_intr_iomap);
	else {
		ret = pmic_read_tt(addr, &val);
		if (ret != 0) {
			dev_err(chc.dev,
				"Error reading tt register 0x%2x\n",
				addr);
			return -EIO;
		}
	}

	seq_printf(seq, "0x%x\n", val);
	return 0;
}

static int pmic_chrgr_tt_reg_open(struct inode *inode, struct file *file)
{
	return single_open(file, pmic_chrgr_tt_reg_show, inode->i_private);
}

static int pmic_chrgr_reg_open(struct inode *inode, struct file *file)
{
	return single_open(file, pmic_chrgr_reg_show, inode->i_private);
}

static struct dentry *charger_debug_dir;
static u8 pmic_regs[] = {
	IRQLVL1_ADDR,
	IRQLVL1_MASK_ADDR,
	CHGRIRQ0_ADDR,
	SCHGRIRQ0_ADDR,
	MCHGRIRQ0_ADDR,
	LOWBATTDET0_ADDR,
	LOWBATTDET1_ADDR,
	BATTDETCTRL_ADDR,
	VBUSDETCTRL_ADDR,
	VDCINDETCTRL_ADDR,
	CHRGRIRQ1_ADDR,
	SCHGRIRQ1_ADDR,
	MCHGRIRQ1_ADDR,
	CHGRCTRL0_ADDR,
	CHGRCTRL1_ADDR,
	CHGRSTATUS_ADDR,
	USBIDCTRL_ADDR,
	USBIDSTAT_ADDR,
	WAKESRC_ADDR,
	THRMBATZONE_ADDR,
	THRMZN0L_ADDR,
	THRMZN0H_ADDR,
	THRMZN1L_ADDR,
	THRMZN1H_ADDR,
	THRMZN2L_ADDR,
	THRMZN2H_ADDR,
	THRMZN3L_ADDR,
	THRMZN3H_ADDR,
	THRMZN4L_ADDR,
	THRMZN4H_ADDR,
};

static u8 pmic_tt_regs[] = {
	TT_I2CDADDR_ADDR,
	TT_CHGRINIT0OS_ADDR,
	TT_CHGRINIT1OS_ADDR,
	TT_CHGRINIT2OS_ADDR,
	TT_CHGRINIT3OS_ADDR,
	TT_CHGRINIT4OS_ADDR,
	TT_CHGRINIT5OS_ADDR,
	TT_CHGRINIT6OS_ADDR,
	TT_CHGRINIT7OS_ADDR,
	TT_USBINPUTICCOS_ADDR,
	TT_USBINPUTICCMASK_ADDR,
	TT_CHRCVOS_ADDR,
	TT_CHRCVMASK_ADDR,
	TT_CHRCCOS_ADDR,
	TT_CHRCCMASK_ADDR,
	TT_LOWCHROS_ADDR,
	TT_LOWCHRMASK_ADDR,
	TT_WDOGRSTOS_ADDR,
	TT_WDOGRSTMASK_ADDR,
	TT_CHGRENOS_ADDR,
	TT_CHGRENMASK_ADDR,
	TT_CUSTOMFIELDEN_ADDR,
	TT_CHGRINIT0VAL_ADDR,
	TT_CHGRINIT1VAL_ADDR,
	TT_CHGRINIT2VAL_ADDR,
	TT_CHGRINIT3VAL_ADDR,
	TT_CHGRINIT4VAL_ADDR,
	TT_CHGRINIT5VAL_ADDR,
	TT_CHGRINIT6VAL_ADDR,
	TT_CHGRINIT7VAL_ADDR,
	TT_USBINPUTICC100VAL_ADDR,
	TT_USBINPUTICC150VAL_ADDR,
	TT_USBINPUTICC500VAL_ADDR,
	TT_USBINPUTICC900VAL_ADDR,
	TT_USBINPUTICC1500VAL_ADDR,
	TT_CHRCVEMRGLOWVAL_ADDR,
	TT_CHRCVCOLDVAL_ADDR,
	TT_CHRCVCOOLVAL_ADDR,
	TT_CHRCVWARMVAL_ADDR,
	TT_CHRCVHOTVAL_ADDR,
	TT_CHRCVEMRGHIVAL_ADDR,
	TT_CHRCCEMRGLOWVAL_ADDR,
	TT_CHRCCCOLDVAL_ADDR,
	TT_CHRCCCOOLVAL_ADDR,
	TT_CHRCCWARMVAL_ADDR,
	TT_CHRCCHOTVAL_ADDR,
	TT_CHRCCEMRGHIVAL_ADDR,
	TT_LOWCHRENVAL_ADDR,
	TT_LOWCHRDISVAL_ADDR,
};

void dump_pmic_regs(void)
{
	u32 pmic_reg_cnt = ARRAY_SIZE(pmic_regs);
	u32 reg_index;
	u8 data;
	int retval;


	dev_info(chc.dev, "PMIC Register dump\n");
	dev_info(chc.dev, "====================\n");

	for (reg_index = 0; reg_index < pmic_reg_cnt; reg_index++) {

		retval = intel_scu_ipc_ioread8(pmic_regs[reg_index], &data);
		if (retval)
			dev_err(chc.dev, "Error in reading %x\n",
				pmic_regs[reg_index]);
		else
			dev_info(chc.dev, "0x%x=0x%x\n",
				pmic_regs[reg_index], data);
	}
	dev_info(chc.dev, "====================\n");
}
EXPORT_SYMBOL(dump_pmic_regs);

void dump_pmic_tt_regs(void)
{
	u32 pmic_tt_reg_cnt = ARRAY_SIZE(pmic_tt_regs);
	u32 reg_index;
	u8 data;
	int retval;

	dev_info(chc.dev, "PMIC CHRGR TT dump\n");
	dev_info(chc.dev, "====================\n");

	for (reg_index = 0; reg_index < pmic_tt_reg_cnt; reg_index++) {

		retval = pmic_read_tt(pmic_tt_regs[reg_index], &data);
		if (retval)
			dev_err(chc.dev, "Error in reading %x\n",
				pmic_tt_regs[reg_index]);
		else
			dev_info(chc.dev, "0x%x=0x%x\n",
				pmic_tt_regs[reg_index], data);
	}

	dev_info(chc.dev, "====================\n");
}
static const struct file_operations pmic_chrgr_reg_fops = {
	.open = pmic_chrgr_reg_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release
};

static const struct file_operations pmic_chrgr_tt_reg_fops = {
	.open = pmic_chrgr_tt_reg_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release
};

static void pmic_debugfs_init(void)
{
	struct dentry *fentry;
	struct dentry *pmic_regs_dir;
	struct dentry *pmic_tt_regs_dir;

	u32 reg_index;
	u32 pmic_reg_cnt = ARRAY_SIZE(pmic_regs);
	u32 pmic_tt_reg_cnt = ARRAY_SIZE(pmic_tt_regs);
	char name[6] = {0};

	/* Creating a directory under debug fs for charger */
	charger_debug_dir = debugfs_create_dir(DRIVER_NAME , NULL) ;
	if (charger_debug_dir == NULL)
		goto debugfs_root_exit;

	/* Create a directory for pmic charger registers */
	pmic_regs_dir = debugfs_create_dir("pmic_ccsm_regs",
			charger_debug_dir);

	if (pmic_regs_dir == NULL)
		goto debugfs_err_exit;

	for (reg_index = 0; reg_index < pmic_reg_cnt; reg_index++) {

		sprintf(name, "%.2x",
				pmic_regs[reg_index]);

		fentry = debugfs_create_file(name,
				S_IRUGO,
				pmic_regs_dir,
				&pmic_regs[reg_index],
				&pmic_chrgr_reg_fops);

		if (fentry == NULL)
			goto debugfs_err_exit;
	}

	/* Create a directory for pmic tt charger registers */
	pmic_tt_regs_dir = debugfs_create_dir("pmic_ccsm_tt_regs",
			charger_debug_dir);

	if (pmic_tt_regs_dir == NULL)
		goto debugfs_err_exit;

	for (reg_index = 0; reg_index < pmic_tt_reg_cnt; reg_index++) {

		sprintf(name, "%.2x", pmic_tt_regs[reg_index]);

		fentry = debugfs_create_file(name,
				S_IRUGO,
				pmic_tt_regs_dir,
				&pmic_tt_regs[reg_index],
				&pmic_chrgr_tt_reg_fops);

		if (fentry == NULL)
			goto debugfs_err_exit;
	}

	dev_dbg(chc.dev, "Debugfs created successfully!!");
	return;

debugfs_err_exit:
	debugfs_remove_recursive(charger_debug_dir);
debugfs_root_exit:
	dev_err(chc.dev, "Error creating debugfs entry!!");
	return;
}

static void pmic_debugfs_exit(void)
{
	if (charger_debug_dir != NULL)
		debugfs_remove_recursive(charger_debug_dir);
}
#endif

static void pmic_bat_zone_changed(void)
{
	int retval;
	int cur_zone;
	u8 data = 0;
	struct power_supply *psy_bat;

	retval = intel_scu_ipc_ioread8(THRMBATZONE_ADDR, &data);
	if (retval) {
		dev_err(chc.dev, "Error in reading battery zone\n");
		return;
	}

	cur_zone = data & THRMBATZONE_MASK;
	dev_info(chc.dev, "Battery Zone changed. Current zone is %d\n",
			(data & THRMBATZONE_MASK));

	/* if current zone is the top and bottom zones then report OVERHEAT
	 */
	if ((cur_zone == PMIC_BZONE_LOW) || (cur_zone == PMIC_BZONE_HIGH))
		chc.health = POWER_SUPPLY_HEALTH_OVERHEAT;
	else
		chc.health = POWER_SUPPLY_HEALTH_GOOD;

	psy_bat = get_psy_battery();
	if (psy_bat)
		power_supply_changed(psy_bat);

	return;
}

static void pmic_battery_overheat_handler(bool stat)
{
	if (stat)
		chc.health = POWER_SUPPLY_HEALTH_OVERHEAT;
	else
		chc.health = POWER_SUPPLY_HEALTH_GOOD;
	return;
}

int pmic_get_health(void)
{
	return chc.health;
}
EXPORT_SYMBOL(pmic_get_health);

int pmic_enable_charging(bool enable)
{
	int ret;
	u8 val;

	if (enable) {
		ret = intel_scu_ipc_update_register(CHGRCTRL1_ADDR,
			CHGRCTRL1_FTEMP_EVENT_MASK, CHGRCTRL1_FTEMP_EVENT_MASK);
		if (ret)
			return ret;
	}

	val = (enable) ? 0 : EXTCHRDIS_ENABLE;

	ret = intel_scu_ipc_update_register(CHGRCTRL0_ADDR,
			val, CHGRCTRL0_EXTCHRDIS_MASK);
	return ret;
}
EXPORT_SYMBOL(pmic_enable_charging);

static inline int update_zone_cc(int zone, u8 reg_val)
{
	u8 rval;
	u8 addr_cc = TT_CHRCCHOTVAL_ADDR - zone;

	dev_dbg(chc.dev, "%s:%X=%X\n", __func__, addr_cc, reg_val);
	pmic_write_tt(addr_cc, reg_val);

	pmic_read_tt(addr_cc, &rval);
	if (rval != reg_val) {
		dev_err(chc.dev,
			"%s:Error in writing to TT reg :%x Wrote=%X:Read=%X\n",
			__func__, addr_cc, reg_val, rval);
		return -EIO;
	}

	return 0;
}

static inline int update_zone_cv(int zone, u8 reg_val)
{
	u8 addr_cv = TT_CHRCVHOTVAL_ADDR - zone;
	dev_dbg(chc.dev, "%s:%X=%X\n", __func__, addr_cv, reg_val);
	return pmic_write_tt(addr_cv, reg_val);
}

static inline int update_zone_temp(int zone, u16 adc_val)
{
	int ret;
	u16 addr_tzone = THRMZN4H_ADDR - (2 * zone);

	ret = intel_scu_ipc_iowrite8(addr_tzone, (u8)(adc_val >> 8));
	if (unlikely(ret))
		return ret;
	dev_dbg(chc.dev, "%s:%X:%X=%X\n", __func__, addr_tzone,
				(addr_tzone+1), adc_val);

	return intel_scu_ipc_iowrite8(addr_tzone+1, (u8)(adc_val & 0xFF));
}

int pmic_set_cc(int new_cc)
{
	struct ps_pse_mod_prof *bcprof = chc.actual_bcprof;
	struct ps_pse_mod_prof *r_bcprof = chc.runtime_bcprof;
	int temp_mon_ranges;
	int new_cc1;
	int ret;
	int i;
	u8 reg_val;

	/* No need to write PMIC if CC = 0 */
	if (!new_cc)
		return 0;

	temp_mon_ranges = min_t(u16, bcprof->temp_mon_ranges,
			BATT_PROF_MAX_TEMP_NR_RNG);

	for (i = 0; i < temp_mon_ranges; ++i) {
		new_cc1 = min_t(int, new_cc,
				bcprof->temp_mon_range[i].full_chrg_cur);

		if (new_cc1 != r_bcprof->temp_mon_range[i].full_chrg_cur) {
			chc.pdata->cc_to_reg(new_cc1, &reg_val);
			ret = update_zone_cc(i, reg_val);
			if (unlikely(ret))
				return ret;
			r_bcprof->temp_mon_range[i].full_chrg_cur = new_cc1;
		}
	}

	/* send the new CC and CV */
	intel_scu_ipc_update_register(CHGRCTRL1_ADDR,
		CHGRCTRL1_FTEMP_EVENT_MASK, CHGRCTRL1_FTEMP_EVENT_MASK);

	return 0;
}
EXPORT_SYMBOL(pmic_set_cc);

int pmic_set_cv(int new_cv)
{
	struct ps_pse_mod_prof *bcprof = chc.actual_bcprof;
	struct ps_pse_mod_prof *r_bcprof = chc.runtime_bcprof;
	int temp_mon_ranges;
	int new_cv1;
	int ret;
	int i;
	u8 reg_val;

	/* No need to write PMIC if CV = 0 */
	if (!new_cv)
		return 0;

	temp_mon_ranges = min_t(u16, bcprof->temp_mon_ranges,
			BATT_PROF_MAX_TEMP_NR_RNG);

	for (i = 0; i < temp_mon_ranges; ++i) {
		new_cv1 = min_t(int, new_cv,
				bcprof->temp_mon_range[i].full_chrg_vol);

		if (new_cv1 != r_bcprof->temp_mon_range[i].full_chrg_vol) {
			chc.pdata->cv_to_reg(new_cv1, &reg_val);

			ret = update_zone_cv(i, reg_val);
			if (unlikely(ret))
				return ret;

			r_bcprof->temp_mon_range[i].full_chrg_vol = new_cv1;
		}
	}

	/* send the new CC and CV */
	intel_scu_ipc_update_register(CHGRCTRL1_ADDR,
		CHGRCTRL1_FTEMP_EVENT_MASK, CHGRCTRL1_FTEMP_EVENT_MASK);

	return 0;
}
EXPORT_SYMBOL(pmic_set_cv);

int pmic_set_ilimmA(int ilim_mA)
{
	u8 mask;

	lookup_regval(pmic_inlmt, ARRAY_SIZE(pmic_inlmt),
			ilim_mA, &mask);
	dev_dbg(chc.dev, "Setting inlmt %d in register %x=%x\n", ilim_mA,
		CHGRCTRL1_ADDR, mask);
	return intel_scu_ipc_update_register(CHGRCTRL1_ADDR, 0xFF, mask);
}
EXPORT_SYMBOL(pmic_set_ilimmA);

/**
 * pmic_read_adc_val - read ADC value of specified sensors
 * @channel: channel of the sensor to be sampled
 * @sensor_val: pointer to the charger property to hold sampled value
 * @chc :  battery info pointer
 *
 * Returns 0 if success
 */
#ifdef CONFIG_BASINCOVE_GPADC
static int pmic_read_adc_val(int channel, int *sensor_val,
			      struct pmic_chrgr_drv_context *chc)
{

	int ret, adc_val;
	struct gpadc_result *adc_res;
	adc_res = kzalloc(sizeof(struct gpadc_result), GFP_KERNEL);
	if (!adc_res)
		return -ENOMEM;
	ret = intel_basincove_gpadc_sample(channel, adc_res);
	if (ret) {
		dev_err(chc->dev, "gpadc_sample failed:%d\n", ret);
		goto exit;
	}

	adc_val = GPADC_RSL(channel, adc_res);
	switch (channel) {
	case GPADC_BATTEMP0:
		ret = CONVERT_ADC_TO_TEMP(adc_val, sensor_val);
		break;
	default:
		dev_err(chc->dev, "invalid sensor%d", channel);
		ret = -EINVAL;
	}
exit:
	kfree(adc_res);
	return ret;
}
#endif

int pmic_get_battery_pack_temp(int *temp)
{
#ifdef CONFIG_BASINCOVE_GPADC
	if (chc.invalid_batt)
		return -ENODEV;
	return pmic_read_adc_val(GPADC_BATTEMP0, temp, &chc);
#else
	*temp = 30;
#endif
	return 0;
}
EXPORT_SYMBOL(pmic_get_battery_pack_temp);

static void handle_level0_interrupt(u8 int_reg, u8 stat_reg,
				struct interrupt_info int_info[],
				int int_info_size)
{
	int i;
	bool int_stat;
	char *log_msg;

	if (int_reg && stat_reg)
		dump_pmic_regs();

	for (i = 0; i < int_info_size; ++i) {

		/*continue if interrupt register bit is not set */
		if (!(int_reg & int_info[i].int_reg_mask))
			continue;

		/*log message if interrupt bit is set */
		if (int_info[i].log_msg_int_reg_true)
			dev_err(chc.dev, "%s",
					int_info[i].log_msg_int_reg_true);

		/* interrupt bit is set.call int handler. */
		if (int_info[i].int_handle)
			int_info[i].int_handle();

		/* continue if stat_reg_mask is zero which
		 *  means ignore status register
		 */
		if (!(int_info[i].stat_reg_mask))
			continue;

		dev_dbg(chc.dev,
				"stat_reg=%X int_info[i].stat_reg_mask=%X",
				stat_reg, int_info[i].stat_reg_mask);

		/* check if the interrupt status is true */
		int_stat = (stat_reg & int_info[i].stat_reg_mask);

		/* log message */
		log_msg = int_stat ? int_info[i].log_msg_stat_true :
			int_info[i].log_msg_stat_false;

		if (log_msg)
			dev_err(chc.dev, "%s", log_msg);

		/* call status handler function */
		if (int_info[i].stat_handle)
			int_info[i].stat_handle(int_stat);

	}

	return ;
}

static void handle_level1_interrupt(u8 int_reg, u8 stat_reg)
{
	int mask;

	if (!int_reg)
		return;

	mask = !!(int_reg & stat_reg);

	if (int_reg & CHRGRIRQ1_SUSBIDDET_MASK) {
			if (mask)
				dev_info(chc.dev, "USB ID Detected. Notifying OTG driver\n");
			else
				dev_info(chc.dev, "USB ID Removed. Notifying OTG driver\n");
			atomic_notifier_call_chain(&chc.otg->notifier,
				USB_EVENT_ID, &mask);
	}

	if (int_reg & CHRGRIRQ1_SVBUSDET_MASK) {
		if (mask)
			dev_info(chc.dev, "USB VBUS Detected. Notifying OTG driver\n");
		else
			dev_info(chc.dev, "USB VBUS Removed. Notifying OTG driver\n");
		atomic_notifier_call_chain(&chc.otg->notifier,
			USB_EVENT_VBUS, &mask);
	}

	return;
}
static void pmic_event_worker(struct work_struct *work)
{
	struct pmic_event *evt, *tmp;
	unsigned long flags;

	dev_dbg(chc.dev, "%s\n", __func__);

	mutex_lock(&chc.evt_queue_lock);
	list_for_each_entry_safe(evt, tmp, &chc.evt_queue, node) {
		list_del(&evt->node);

		dev_dbg(chc.dev, "CHGRIRQ0=%X SCHGRIRQ0=%X CHGRIRQ1=%x SCHGRIRQ1=%X\n",
				evt->chgrirq0_int, evt->chgrirq0_stat,
				evt->chgrirq1_int, evt->chgrirq1_stat);
		if (evt->chgrirq0_int)
			handle_level0_interrupt(evt->chgrirq0_int,
				evt->chgrirq0_stat, chgrirq0_info,
				ARRAY_SIZE(chgrirq0_info));

		if (evt->chgrirq1_stat)
			handle_level1_interrupt(evt->chgrirq1_int,
							evt->chgrirq1_stat);
		kfree(evt);
	}

	mutex_unlock(&chc.evt_queue_lock);
}

static irqreturn_t pmic_isr(int irq, void *data)
{
	u16 pmic_intr;
	u8 chgrirq0_int;
	u8 chgrirq1_int;

	pmic_intr = ioread16(chc.pmic_intr_iomap);
	chgrirq0_int = (u8)pmic_intr;
	chgrirq1_int = (u8)(pmic_intr >> 8);

	if (!chgrirq1_int && !(chgrirq0_int & PMIC_CHRGR_INT0_MASK))
		return IRQ_NONE;

	dev_dbg(chc.dev, "%s", __func__);

	return IRQ_WAKE_THREAD;
}
static irqreturn_t pmic_thread_handler(int id, void *data)
{
	u16 pmic_intr;
	struct pmic_event *evt;
	int ret;

	evt = kzalloc(sizeof(*evt), GFP_ATOMIC);

	pmic_intr = ioread16(chc.pmic_intr_iomap);
	evt->chgrirq0_int = (u8)pmic_intr;
	evt->chgrirq1_int = (u8)(pmic_intr >> 8);

	/*
	In case this is an external charger interrupt, we are
	clearing the level 1 irq register and let external charger
	driver handle the interrupt.
	 */

	if (!(evt->chgrirq1_int) &&
		!(evt->chgrirq0_int & PMIC_CHRGR_CCSM_INT0_MASK)) {
		intel_scu_ipc_update_register(IRQLVL1_MASK_ADDR, 0x00,
				IRQLVL1_CHRGR_MASK);
		kfree(evt);
		return IRQ_NONE;
	}

	if (evt->chgrirq0_int & PMIC_CHRGR_CCSM_INT0_MASK) {
		ret = intel_scu_ipc_ioread8(SCHGRIRQ0_ADDR,
				&evt->chgrirq0_stat);
		if (ret) {
			dev_err(chc.dev,
				"%s: Error(%d) in intel_scu_ipc_ioread8. Faile to read SCHGRIRQ0_ADDR\n",
					__func__, ret);
			kfree(evt);
			goto end;
		}
	}
	if (evt->chgrirq1_int) {
		ret = intel_scu_ipc_ioread8(SCHGRIRQ1_ADDR,
				&evt->chgrirq1_stat);
		if (ret) {
			dev_err(chc.dev,
				"%s: Error(%d) in intel_scu_ipc_ioread8. Faile to read SCHGRIRQ1_ADDR\n",
					__func__, ret);
			kfree(evt);
			goto end;
		}
	}

	INIT_LIST_HEAD(&evt->node);

	mutex_lock(&chc.evt_queue_lock);
	list_add_tail(&evt->node, &chc.evt_queue);
	mutex_unlock(&chc.evt_queue_lock);

	queue_work(system_nrt_wq, &chc.evt_work);

end:
	/*clear first level IRQ */
	dev_dbg(chc.dev, "Clearing IRQLVL1_MASK_ADDR\n");
	intel_scu_ipc_update_register(IRQLVL1_MASK_ADDR, 0x00,
			IRQLVL1_CHRGR_MASK);

	return IRQ_HANDLED;
}

static int pmic_init(void)
{
	int ret = 0, i, temp_mon_ranges;
	u16 adc_val;
	u8 reg_val;
	struct ps_pse_mod_prof *bcprof = chc.actual_bcprof;

	ret = intel_scu_ipc_update_register(CHGRCTRL0_ADDR, SWCONTROL_ENABLE,
			CHGRCTRL0_SWCONTROL_MASK);
	if (ret) {
		dev_err(chc.dev, "Error enabling sw control!!\n");
		return ret;
	}

	temp_mon_ranges = min_t(u16, bcprof->temp_mon_ranges,
			BATT_PROF_MAX_TEMP_NR_RNG);
	for (i = 0; i < temp_mon_ranges; ++i) {
		ret =
		CONVERT_TEMP_TO_ADC(bcprof->temp_mon_range[i].temp_up_lim,
				(int *)&adc_val);
		if (unlikely(ret)) {
			dev_err(chc.dev,
				"Error converting temperature for zone %d!!\n",
				i);
			return ret;
		}
		ret = update_zone_temp(i, adc_val);
		if (unlikely(ret)) {
			dev_err(chc.dev,
				"Error updating zone temp for zone %d\n",
				i);
			return ret;
		}
		chc.pdata->cc_to_reg(bcprof->temp_mon_range[i].
				full_chrg_cur, &reg_val);

		ret = update_zone_cc(i, reg_val);
		if (unlikely(ret)) {
			dev_err(chc.dev,
				"Error updating zone cc for zone %d\n",
				i);
			return ret;
		}

		chc.pdata->cv_to_reg(bcprof->temp_mon_range[i].
				full_chrg_vol, &reg_val);

		ret = update_zone_cv(i, reg_val);
		if (unlikely(ret)) {
			dev_err(chc.dev,
				"Error updating zone cv for zone %d\n",
				i);
			return ret;
		}

		/* Write lowest temp limit */
		if (i == (bcprof->temp_mon_ranges - 1)) {
			ret = CONVERT_TEMP_TO_ADC(bcprof->temp_low_lim,
							(int *)&adc_val);
			if (unlikely(ret)) {
				dev_err(chc.dev,
					"Error converting low lim temp!!\n");
				return ret;
			}

			ret = update_zone_temp(i+1, adc_val);

			if (unlikely(ret)) {
				dev_err(chc.dev,
					"Error updating last temp for zone %d\n",
					i+1);
				return ret;
			}
		}
	}
	return ret;
}

static inline void print_ps_pse_mod_prof(struct ps_pse_mod_prof *bcprof)
{
	int i, temp_mon_ranges;

	dev_info(chc.dev, "ChrgProf: batt_id:%s\n", bcprof->batt_id);
	dev_info(chc.dev, "ChrgProf: battery_type:%u\n", bcprof->battery_type);
	dev_info(chc.dev, "ChrgProf: capacity:%u\n", bcprof->capacity);
	dev_info(chc.dev, "ChrgProf: voltage_max:%u\n", bcprof->voltage_max);
	dev_info(chc.dev, "ChrgProf: chrg_term_mA:%u\n", bcprof->chrg_term_mA);
	dev_info(chc.dev, "ChrgProf: low_batt_mV:%u\n", bcprof->low_batt_mV);
	dev_info(chc.dev, "ChrgProf: disch_tmp_ul:%u\n", bcprof->disch_tmp_ul);
	dev_info(chc.dev, "ChrgProf: disch_tmp_ll:%u\n", bcprof->disch_tmp_ll);
	dev_info(chc.dev, "ChrgProf: temp_mon_ranges:%u\n",
			bcprof->temp_mon_ranges);
	temp_mon_ranges = min_t(u16, bcprof->temp_mon_ranges,
			BATT_PROF_MAX_TEMP_NR_RNG);

	for (i = 0; i < temp_mon_ranges; ++i) {
		dev_info(chc.dev, "ChrgProf: temp_up_lim[%d]:%d\n",
				i, bcprof->temp_mon_range[i].temp_up_lim);
		dev_info(chc.dev, "ChrgProf: full_chrg_vol[%d]:%d\n",
				i, bcprof->temp_mon_range[i].full_chrg_vol);
		dev_info(chc.dev, "ChrgProf: full_chrg_cur[%d]:%d\n",
				i, bcprof->temp_mon_range[i].full_chrg_cur);
		dev_info(chc.dev, "ChrgProf: maint_chrgr_vol_ll[%d]:%d\n",
				i, bcprof->temp_mon_range[i].maint_chrg_vol_ll);
		dev_info(chc.dev, "ChrgProf: maint_chrgr_vol_ul[%d]:%d\n",
				i, bcprof->temp_mon_range[i].maint_chrg_vol_ul);
		dev_info(chc.dev, "ChrgProf: maint_chrg_cur[%d]:%d\n",
				i, bcprof->temp_mon_range[i].maint_chrg_cur);
	}
	dev_info(chc.dev, "ChrgProf: temp_low_lim:%d\n", bcprof->temp_low_lim);
}

static int find_tempzone_index(short int *interval,
				int *num_zones,
				short int *temp_up_lim)
{
	struct ps_pse_mod_prof *bprof = chc.sfi_bcprof->batt_prof;
	int up_lim_index = -1, low_lim_index = -1;
	int diff = 0;
	int i;

	*num_zones = MIN_BATT_PROF - bprof->temp_mon_ranges + 1;
	if ((*num_zones) <= 0)
		return 0;

	for (i = 0 ; i < bprof->temp_mon_ranges ; i++) {
		if (bprof->temp_mon_range[i].temp_up_lim == BATT_TEMP_WARM)
			up_lim_index = i;
	}

	low_lim_index = up_lim_index + 1;

	if (low_lim_index == bprof->temp_mon_ranges)
		diff = bprof->temp_low_lim -
			bprof->temp_mon_range[up_lim_index].temp_up_lim;
	else
		diff = bprof->temp_mon_range[low_lim_index].temp_up_lim -
			bprof->temp_mon_range[up_lim_index].temp_up_lim;

	*interval = diff / (*num_zones);
	*temp_up_lim = bprof->temp_mon_range[up_lim_index].temp_up_lim;

	return up_lim_index;
}


static void set_pmic_batt_prof(struct ps_pse_mod_prof *new_prof,
				struct ps_pse_mod_prof *bprof)
{
	int num_zones;
	int split_index;
	int i, j = 0;
	short int temp_up_lim;
	short int interval;

	if ((new_prof == NULL) || (bprof == NULL))
		return;

	if (!NEED_ZONE_SPLIT(bprof)) {
		dev_info(chc.dev, "No need to split the zones!!\n");
		memcpy(new_prof, bprof, sizeof(struct ps_pse_mod_prof));
		return;
	}

	strcpy(&(new_prof->batt_id[0]), &(bprof->batt_id[0]));
	new_prof->battery_type = bprof->battery_type;
	new_prof->capacity = bprof->capacity;
	new_prof->voltage_max =  bprof->voltage_max;
	new_prof->chrg_term_mA = bprof->chrg_term_mA;
	new_prof->low_batt_mV =  bprof->low_batt_mV;
	new_prof->disch_tmp_ul = bprof->disch_tmp_ul;
	new_prof->disch_tmp_ll = bprof->disch_tmp_ll;

	split_index = find_tempzone_index(&interval, &num_zones, &temp_up_lim);

	for (i = 0 ; i < bprof->temp_mon_ranges; i++) {
		if ((i == split_index) && (num_zones > 0)) {
			for (j = 0; j < num_zones; j++,
					temp_up_lim += interval) {
				memcpy(&new_prof->temp_mon_range[i+j],
					&bprof->temp_mon_range[i],
					sizeof(bprof->temp_mon_range[i]));
				new_prof->temp_mon_range[i+j].temp_up_lim =
					temp_up_lim;
			}
			j--;
		} else {
			memcpy(&new_prof->temp_mon_range[i+j],
				&bprof->temp_mon_range[i],
				sizeof(bprof->temp_mon_range[i]));
		}
	}

	new_prof->temp_mon_ranges = i+j;
	new_prof->temp_low_lim = bprof->temp_low_lim;

	return;
}


static int pmic_check_initial_events(void)
{
	struct pmic_event *evt;
	int ret;

	evt = kzalloc(sizeof(struct pmic_event), GFP_KERNEL);

	ret = intel_scu_ipc_ioread8(SCHGRIRQ0_ADDR, &evt->chgrirq0_stat);
	evt->chgrirq0_int = evt->chgrirq0_stat;
	ret = intel_scu_ipc_ioread8(SCHGRIRQ1_ADDR, &evt->chgrirq1_stat);
	evt->chgrirq1_int = evt->chgrirq1_stat;

	if (evt->chgrirq1_stat || evt->chgrirq0_int) {
		INIT_LIST_HEAD(&evt->node);
		mutex_lock(&chc.evt_queue_lock);
		list_add_tail(&evt->node, &chc.evt_queue);
		mutex_unlock(&chc.evt_queue_lock);
		schedule_work(&chc.evt_work);
	}
	pmic_bat_zone_changed();

	return ret;
}

/**
 * pmic_charger_probe - PMIC charger probe function
 * @ipcdev: pmic ipc device structure
 * Context: can sleep
 *
 * pmic charger driver initializes its internal data
 * structure and other  infrastructure components for it
 * to work as expected.
 */
static int pmic_chrgr_probe(struct ipc_device *ipcdev)
{
	int retval = 0;

	if (!ipcdev)
		return -ENODEV;

	chc.health = POWER_SUPPLY_HEALTH_UNKNOWN;
	chc.dev = &ipcdev->dev;
	chc.irq = ipc_get_irq(ipcdev, 0);
	chc.pdata = ipcdev->dev.platform_data;
	ipc_set_drvdata(ipcdev, &chc);

	if (chc.pdata == NULL) {
		dev_err(chc.dev, "Platform data not initialized\n");
		return -EFAULT;
	}

	chc.sfi_bcprof = kzalloc(sizeof(struct ps_batt_chg_prof),
				GFP_KERNEL);
	if (chc.sfi_bcprof == NULL) {
		dev_err(chc.dev,
			"Error allocating memeory SFI battery profile\n");
		return -ENOMEM;
	}

	retval = get_batt_prop(chc.sfi_bcprof);
	if (retval) {
		dev_err(chc.dev,
			"Error reading battery profile from battid frmwrk\n");
		kfree(chc.sfi_bcprof);
		chc.invalid_batt = true;
		chc.sfi_bcprof = NULL;
	}

	if (!chc.invalid_batt) {
		chc.actual_bcprof = kzalloc(sizeof(struct ps_pse_mod_prof),
					GFP_KERNEL);
		if (chc.actual_bcprof == NULL) {
			dev_err(chc.dev,
				"Error allocating mem for local battery profile\n");
			kfree(chc.sfi_bcprof);
			return -ENOMEM;
		}

		chc.runtime_bcprof = kzalloc(sizeof(struct ps_pse_mod_prof),
					GFP_KERNEL);
		if (chc.runtime_bcprof == NULL) {
			dev_err(chc.dev,
			"Error allocating mem for runtime batt profile\n");
			kfree(chc.sfi_bcprof);
			kfree(chc.actual_bcprof);
			return -ENOMEM;
		}

		set_pmic_batt_prof(chc.actual_bcprof,
				chc.sfi_bcprof->batt_prof);
		print_ps_pse_mod_prof(chc.actual_bcprof);
		retval = pmic_init();
		if (retval) {
			dev_err(chc.dev, "Error in Initializing PMIC\n");
			kfree(chc.sfi_bcprof);
			kfree(chc.actual_bcprof);
			kfree(chc.runtime_bcprof);
			return retval;
		}

		dump_pmic_tt_regs();
		memcpy(chc.runtime_bcprof, chc.actual_bcprof,
			sizeof(struct ps_pse_mod_prof));
	}
	chc.pmic_intr_iomap = ioremap_nocache(PMIC_SRAM_INTR_ADDR, 8);
	if (!chc.pmic_intr_iomap) {
		dev_err(&ipcdev->dev, "ioremap Failed\n");
		retval = -ENOMEM;
		goto ioremap_failed;
	}

	chc.otg = otg_get_transceiver();
	if (!chc.otg) {
		dev_err(&ipcdev->dev, "Failed to get otg transceiver!!\n");
		retval = -ENOMEM;
		goto otg_req_failed;
	}

	INIT_WORK(&chc.evt_work, pmic_event_worker);
	INIT_LIST_HEAD(&chc.evt_queue);
	mutex_init(&chc.evt_queue_lock);

	/* register interrupt */
	retval = request_threaded_irq(chc.irq, pmic_isr,
			pmic_thread_handler,
			IRQF_SHARED ,
			DRIVER_NAME, &chc);
	if (retval) {
		dev_err(&ipcdev->dev,
			"Error in request_threaded_irq(irq(%d)!!\n",
			chc.irq);
		goto req_irq_failed;
	}

	pmic_check_initial_events();

	/* unmask charger interrupts in second level IRQ register*/
	retval = intel_scu_ipc_update_register(MCHGRIRQ0_ADDR, 0x00,
			PMIC_CHRGR_INT0_MASK);
	/* unmask charger interrupts in second level IRQ register*/
	retval = intel_scu_ipc_iowrite8(MCHGRIRQ1_ADDR, 0x00);
	if (unlikely(retval))
		goto unmask_irq_failed;


	/* unmask IRQLVL1 register */
	retval = intel_scu_ipc_update_register(IRQLVL1_MASK_ADDR, 0x00,
			IRQLVL1_CHRGR_MASK);
	if (unlikely(retval))
		goto unmask_irq_failed;

	retval = intel_scu_ipc_update_register(USBIDCTRL_ADDR,
			 ACADETEN_MASK | USBIDEN_MASK,
			ACADETEN_MASK | USBIDEN_MASK);
	if (unlikely(retval))
		goto unmask_irq_failed;

	chc.health = POWER_SUPPLY_HEALTH_GOOD;
#ifdef CONFIG_DEBUG_FS
	pmic_debugfs_init();
#endif
	return 0;

unmask_irq_failed:
	free_irq(chc.irq, &chc);
req_irq_failed:
otg_req_failed:
	iounmap(chc.pmic_intr_iomap);
ioremap_failed:
	kfree(chc.sfi_bcprof);
	kfree(chc.actual_bcprof);
	kfree(chc.runtime_bcprof);
	return retval;
}

static void pmic_chrgr_do_exit_ops(struct pmic_chrgr_drv_context *chc)
{
	/*TODO:
	 * If charger is connected send IPC message to SCU to continue charging
	 */
#ifdef CONFIG_DEBUG_FS
	pmic_debugfs_exit();
#endif
}

/**
 * pmic_charger_remove - PMIC Charger driver remove
 * @ipcdev: PMIC charger ipc device structure
 * Context: can sleep
 *
 * PMIC charger finalizes its internal data structure and other
 * infrastructure components that it initialized in
 * pmic_chrgr_probe.
 */
static int pmic_chrgr_remove(struct ipc_device *ipcdev)
{
	struct pmic_chrgr_drv_context *chc = ipc_get_drvdata(ipcdev);

	if (chc) {
		pmic_chrgr_do_exit_ops(chc);
		free_irq(chc->irq, chc);
		iounmap(chc->pmic_intr_iomap);
		kfree(chc->sfi_bcprof);
		kfree(chc->actual_bcprof);
		kfree(chc->runtime_bcprof);
	}

	return 0;
}

#ifdef CONFIG_PM
static int pmic_chrgr_suspend(struct device *dev)
{
	dev_dbg(dev, "%s called\n", __func__);
	return 0;
}

static int pmic_chrgr_resume(struct device *dev)
{
	dev_dbg(dev, "%s called\n", __func__);
	return 0;
}
#endif

#ifdef CONFIG_PM_RUNTIME
static int pmic_chrgr_runtime_suspend(struct device *dev)
{
	dev_dbg(dev, "%s called\n", __func__);
	return 0;
}

static int pmic_chrgr_runtime_resume(struct device *dev)
{
	dev_dbg(dev, "%s called\n", __func__);
	return 0;
}

static int pmic_chrgr_runtime_idle(struct device *dev)
{
	dev_dbg(dev, "%s called\n", __func__);
	return 0;
}
#endif

/*********************************************************************
 *		Driver initialisation and finalization
 *********************************************************************/

static const struct dev_pm_ops pmic_chrgr_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(pmic_chrgr_suspend,
				pmic_chrgr_resume)
	SET_RUNTIME_PM_OPS(pmic_chrgr_runtime_suspend,
				pmic_chrgr_runtime_resume,
				pmic_chrgr_runtime_idle)
};

static struct ipc_driver pmic_chrgr_driver = {
	.driver = {
		   .name = DRIVER_NAME,
		   .owner = THIS_MODULE,
		   .pm = &pmic_chrgr_pm_ops,
		   },
	.probe = pmic_chrgr_probe,
	.remove = __devexit_p(pmic_chrgr_remove),
};

static int __init pmic_chrgr_init(void)
{
	return ipc_driver_register(&pmic_chrgr_driver);
}

static void __exit pmic_chrgr_exit(void)
{
	ipc_driver_unregister(&pmic_chrgr_driver);
}

/* Defer init call so that dependant drivers will be loaded. Using  async
 * for parallel driver initialization */
late_initcall(pmic_chrgr_init);
module_exit(pmic_chrgr_exit);

MODULE_AUTHOR("Jenny TC <jenny.tc@intel.com>");
MODULE_DESCRIPTION("PMIC Charger  Driver");
MODULE_LICENSE("GPL");
