/*
 *  Universal power supply monitor class
 *
 *  Copyright © 2007  Anton Vorontsov <cbou@mail.ru>
 *  Copyright © 2004  Szabolcs Gyurko
 *  Copyright © 2003  Ian Molton <spyro@f2s.com>
 *
 *  Modified: 2004, Oct     Szabolcs Gyurko
 *
 *  You may use this code as per GPL version 2
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/power_supply.h>
#include <linux/thermal.h>
#include "power_supply.h"
#include "power_supply_charger.h"

/* exported for the APM Power driver, APM emulation */
struct class *power_supply_class;
EXPORT_SYMBOL_GPL(power_supply_class);

static struct device_type power_supply_dev_type;

static struct mutex ps_chrg_evt_lock;

static struct power_supply_charger_cap power_supply_chrg_cap = {
		.chrg_evt	= POWER_SUPPLY_CHARGER_EVENT_DISCONNECT,
		.chrg_type	= POWER_SUPPLY_TYPE_USB,
		.mA		= 0	/* 0 mA */
};

static int __power_supply_changed_work(struct device *dev, void *data)
{
	struct power_supply *psy = (struct power_supply *)data;
	struct power_supply *pst = dev_get_drvdata(dev);
	int i;

	for (i = 0; i < psy->num_supplicants; i++)
		if (!strcmp(psy->supplied_to[i], pst->name)) {
			if (pst->external_power_changed)
				pst->external_power_changed(pst);
		}
	return 0;
}

static void power_supply_changed_work(struct work_struct *work)
{
	unsigned long flags;
	struct power_supply *psy = container_of(work, struct power_supply,
						changed_work);

	dev_dbg(psy->dev, "%s\n", __func__);

	spin_lock_irqsave(&psy->changed_lock, flags);
	if (psy->changed) {
		psy->changed = false;
		spin_unlock_irqrestore(&psy->changed_lock, flags);

		class_for_each_device(power_supply_class, NULL, psy,
				      __power_supply_changed_work);
		power_supply_trigger_charging_handler(psy);

		power_supply_update_leds(psy);

		kobject_uevent(&psy->dev->kobj, KOBJ_CHANGE);
		spin_lock_irqsave(&psy->changed_lock, flags);
	}
	if (!psy->changed)
		wake_unlock(&psy->work_wake_lock);
	spin_unlock_irqrestore(&psy->changed_lock, flags);
}

void power_supply_changed(struct power_supply *psy)
{
	unsigned long flags;

	dev_dbg(psy->dev, "%s\n", __func__);

	spin_lock_irqsave(&psy->changed_lock, flags);
	psy->changed = true;
	wake_lock(&psy->work_wake_lock);
	spin_unlock_irqrestore(&psy->changed_lock, flags);
	schedule_work(&psy->changed_work);
}
EXPORT_SYMBOL_GPL(power_supply_changed);

static int __power_supply_charger_event(struct device *dev, void *data)
{
	struct power_supply_charger_cap *cap =
				(struct power_supply_charger_cap *)data;
	struct power_supply *psy = dev_get_drvdata(dev);

	if (psy->charging_port_changed)
		psy->charging_port_changed(psy, cap);

	return 0;
}

void power_supply_charger_event(struct power_supply_charger_cap cap)
{
	class_for_each_device(power_supply_class, NULL, &cap,
				      __power_supply_charger_event);

	mutex_lock(&ps_chrg_evt_lock);
	memcpy(&power_supply_chrg_cap, &cap, sizeof(power_supply_chrg_cap));
	mutex_unlock(&ps_chrg_evt_lock);
}
EXPORT_SYMBOL_GPL(power_supply_charger_event);

void power_supply_query_charger_caps(struct power_supply_charger_cap *cap)
{
	mutex_lock(&ps_chrg_evt_lock);
	memcpy(cap, &power_supply_chrg_cap, sizeof(power_supply_chrg_cap));
	mutex_unlock(&ps_chrg_evt_lock);
}
EXPORT_SYMBOL_GPL(power_supply_query_charger_caps);

static int __power_supply_am_i_supplied(struct device *dev, void *data)
{
	union power_supply_propval ret = {0,};
	struct power_supply *psy = (struct power_supply *)data;
	struct power_supply *epsy = dev_get_drvdata(dev);
	int i;

	for (i = 0; i < epsy->num_supplicants; i++) {
		if (!strcmp(epsy->supplied_to[i], psy->name)) {
			if (epsy->get_property(epsy,
				  POWER_SUPPLY_PROP_ONLINE, &ret))
				continue;
			if (ret.intval)
				return ret.intval;
		}
	}
	return 0;
}

int power_supply_am_i_supplied(struct power_supply *psy)
{
	int error;

	error = class_for_each_device(power_supply_class, NULL, psy,
				      __power_supply_am_i_supplied);

	dev_dbg(psy->dev, "%s %d\n", __func__, error);

	return error;
}
EXPORT_SYMBOL_GPL(power_supply_am_i_supplied);

static int __power_supply_is_system_supplied(struct device *dev, void *data)
{
	union power_supply_propval ret = {0,};
	struct power_supply *psy = dev_get_drvdata(dev);

	if (psy->type != POWER_SUPPLY_TYPE_BATTERY) {
		if (psy->get_property(psy, POWER_SUPPLY_PROP_ONLINE, &ret))
			return 0;
		if (ret.intval)
			return ret.intval;
	}
	return 0;
}

int power_supply_is_system_supplied(void)
{
	int error;

	error = class_for_each_device(power_supply_class, NULL, NULL,
				      __power_supply_is_system_supplied);

	return error;
}
EXPORT_SYMBOL_GPL(power_supply_is_system_supplied);

static int __power_supply_is_battery_connected(struct device *dev, void *data)
{
	union power_supply_propval ret = {0,};
	struct power_supply *psy = dev_get_drvdata(dev);

	if (psy->type == POWER_SUPPLY_TYPE_BATTERY) {
		if (psy->get_property(psy, POWER_SUPPLY_PROP_PRESENT, &ret))
			return 0;
		if (ret.intval)
			return ret.intval;
	}
	return 0;
}

int power_supply_is_battery_connected(void)
{
	int error;

	error = class_for_each_device(power_supply_class, NULL, NULL,
					__power_supply_is_battery_connected);
	return error;
}
EXPORT_SYMBOL_GPL(power_supply_is_battery_connected);

int power_supply_set_battery_charged(struct power_supply *psy)
{
	if (psy->type == POWER_SUPPLY_TYPE_BATTERY && psy->set_charged) {
		psy->set_charged(psy);
		return 0;
	}

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(power_supply_set_battery_charged);

static int power_supply_match_device_by_name(struct device *dev, void *data)
{
	const char *name = data;
	struct power_supply *psy = dev_get_drvdata(dev);

	return strcmp(psy->name, name) == 0;
}

struct power_supply *power_supply_get_by_name(char *name)
{
	struct device *dev = class_find_device(power_supply_class, NULL, name,
					power_supply_match_device_by_name);

	return dev ? dev_get_drvdata(dev) : NULL;
}
EXPORT_SYMBOL_GPL(power_supply_get_by_name);

static void power_supply_dev_release(struct device *dev)
{
	pr_debug("device: '%s': %s\n", dev_name(dev), __func__);
	kfree(dev);
}

#ifdef CONFIG_THERMAL
static int power_supply_read_temp(struct thermal_zone_device *tzd,
		unsigned long *temp)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	if (WARN_ON(tzd == NULL))
		return -EINVAL;
	psy = tzd->devdata;
	ret = psy->get_property(psy, POWER_SUPPLY_PROP_TEMP, &val);

	/* Convert tenths of degree Celsius to milli degree Celsius. */
	if (!ret)
		*temp = val.intval * 100;

	return ret;
}

static struct thermal_zone_device_ops psy_tzd_ops = {
	.get_temp = power_supply_read_temp,
};

static int psy_register_thermal(struct power_supply *psy)
{
	int i;

	/* Register battery zone device psy reports temperature */
	for (i = 0; i < psy->num_properties; i++) {
		if (psy->properties[i] == POWER_SUPPLY_PROP_TEMP) {
			psy->tzd = thermal_zone_device_register(
				(char *)psy->name, 0, 0, psy, &psy_tzd_ops,
				0, 0, 0, 0);
			if (IS_ERR(psy->tzd))
				return PTR_ERR(psy->tzd);
			break;
		}
	}
	return 0;
}

static void psy_unregister_thermal(struct power_supply *psy)
{
	if (IS_ERR_OR_NULL(psy->tzd))
		return;
	thermal_zone_device_unregister(psy->tzd);
}

/* thermal cooling device callbacks */
static int ps_get_max_charge_cntl_limit(struct thermal_cooling_device *tcd,
						unsigned long *state)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	if (WARN_ON(tcd == NULL))
		return -EINVAL;
	psy = tcd->devdata;
	ret = psy->get_property(psy,
		POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX, &val);
	if (!ret)
		*state = val.intval;

	return ret;
}

static int ps_get_cur_chrage_cntl_limit(struct thermal_cooling_device *tcd,
						unsigned long *state)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	if (WARN_ON(tcd == NULL))
		return -EINVAL;
	psy = tcd->devdata;
	ret = psy->get_property(psy,
		POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT, &val);
	if (!ret)
		*state = val.intval;

	return ret;
}

static int ps_set_cur_charge_cntl_limit(struct thermal_cooling_device *tcd,
							unsigned long state)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	if (WARN_ON(tcd == NULL))
		return -EINVAL;
	psy = tcd->devdata;
	val.intval = state;
	ret = psy->set_property(psy,
		POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT, &val);

	psy_charger_throttle_charger(psy, state);

	return ret;
}

static struct thermal_cooling_device_ops psy_tcd_ops = {
	.get_max_state = ps_get_max_charge_cntl_limit,
	.get_cur_state = ps_get_cur_chrage_cntl_limit,
	.set_cur_state = ps_set_cur_charge_cntl_limit,
};

static int psy_register_cooler(struct power_supply *psy)
{
	int i;

	/* Register for cooling device if psy can control charging */
	for (i = 0; i < psy->num_properties; i++) {
		if (psy->properties[i] ==
				POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT) {
			psy->tcd = thermal_cooling_device_register(
							(char *)psy->name,
							psy, &psy_tcd_ops);
			if (IS_ERR(psy->tcd))
				return PTR_ERR(psy->tcd);
			break;
		}
	}
	return 0;
}

static void psy_unregister_cooler(struct power_supply *psy)
{
	if (IS_ERR_OR_NULL(psy->tcd))
		return;
	thermal_cooling_device_unregister(psy->tcd);
}
#else
static int psy_register_thermal(struct power_supply *psy)
{
	return 0;
}

static void psy_unregister_thermal(struct power_supply *psy)
{
}

static int psy_register_cooler(struct power_supply *psy)
{
	return 0;
}

static void psy_unregister_cooler(struct power_supply *psy)
{

}
#endif

int power_supply_register(struct device *parent, struct power_supply *psy)
{
	struct device *dev;
	int rc;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	device_initialize(dev);

	dev->class = power_supply_class;
	dev->type = &power_supply_dev_type;
	dev->parent = parent;
	dev->release = power_supply_dev_release;
	dev_set_drvdata(dev, psy);
	psy->dev = dev;

	INIT_WORK(&psy->changed_work, power_supply_changed_work);

	rc = kobject_set_name(&dev->kobj, "%s", psy->name);
	if (rc)
		goto kobject_set_name_failed;

	rc = device_add(dev);
	if (rc)
		goto device_add_failed;

	spin_lock_init(&psy->changed_lock);
	wake_lock_init(&psy->work_wake_lock, WAKE_LOCK_SUSPEND, "power-supply");

	rc = psy_register_thermal(psy);
	if (rc)
		goto register_thermal_failed;

	rc = psy_register_cooler(psy);
	if (rc)
		goto register_cooler_failed;

	rc = power_supply_create_triggers(psy);
	if (rc)
		goto create_triggers_failed;

	if (IS_CHARGER(psy))
		rc = power_supply_register_charger(psy);
	if (rc)
		goto charger_register_failed;

	power_supply_changed(psy);

	goto success;

charger_register_failed:
create_triggers_failed:
	psy_unregister_cooler(psy);
register_cooler_failed:
	psy_unregister_thermal(psy);
register_thermal_failed:
	wake_lock_destroy(&psy->work_wake_lock);
	device_del(dev);
kobject_set_name_failed:
device_add_failed:
	put_device(dev);
success:
	return rc;
}
EXPORT_SYMBOL_GPL(power_supply_register);

void power_supply_unregister(struct power_supply *psy)
{
	cancel_work_sync(&psy->changed_work);
	power_supply_remove_triggers(psy);
	if (IS_CHARGER(psy))
		power_supply_unregister_charger(psy);
	psy_unregister_cooler(psy);
	psy_unregister_thermal(psy);
	wake_lock_destroy(&psy->work_wake_lock);
	device_unregister(psy->dev);
}
EXPORT_SYMBOL_GPL(power_supply_unregister);

static int __init power_supply_class_init(void)
{
	power_supply_class = class_create(THIS_MODULE, "power_supply");

	if (IS_ERR(power_supply_class))
		return PTR_ERR(power_supply_class);

	power_supply_class->dev_uevent = power_supply_uevent;
	power_supply_init_attrs(&power_supply_dev_type);
	mutex_init(&ps_chrg_evt_lock);

	return 0;
}

static void __exit power_supply_class_exit(void)
{
	class_destroy(power_supply_class);
}

subsys_initcall(power_supply_class_init);
module_exit(power_supply_class_exit);

MODULE_DESCRIPTION("Universal power supply monitor class");
MODULE_AUTHOR("Ian Molton <spyro@f2s.com>, "
	      "Szabolcs Gyurko, "
	      "Anton Vorontsov <cbou@mail.ru>");
MODULE_LICENSE("GPL");
