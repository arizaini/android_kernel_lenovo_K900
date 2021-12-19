#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/sysdev.h>
#include <linux/proc_fs.h>
#include <linux/gpio.h>

/*
 * Switch 1/2 for AP/CP usb route
 *
 * USB_AP_CP_SWITCH : GP_CORE_019
 * DBG_USB_CS_N     : GP_AON_051
 *
 * USB_AP_CP_SWITCH         DBG_USB_CS_N        			FUNCTION
 *      0                       0               AP<-->USB CONN1   MODEM<-->AP
 *      1						0				              N/A
 *      0                       1                             N/A
 *      1                       1               MODEM<->SWITCH2   SWITCH2<->USB CONN1
 */
//#define K5_EVT_USB_ROUTE

#define SWITCH_ENTRY	"ap_cp_switch"
#define DBG_USB_CS_N	(51)

#ifdef K5_EVT_USB_ROUTE
#define USB_AP_CP_SWITCH (19 + 96)  //GPIO number = GP_CORE_019 + 96
#endif
static ssize_t proc_switch_write(struct file *file, const char __user *buf, size_t nbytes, loff_t *ppos)
{
	 char string[nbytes];
	sscanf(buf, "%s", string);
	if (!strcmp((const char *)string, (const char *)"cp")) {
#ifndef K5_EVT_USB_ROUTE
		gpio_set_value(DBG_USB_CS_N, 1);
		printk(KERN_INFO "set DBG_USB_CS_N to high!\n");
#else
		gpio_set_value(DBG_USB_CS_N, 1);
		gpio_set_value(USB_AP_CP_SWITCH, 1);
		printk(KERN_EMERG "### K5 EVT board, set to CP path ###\n");
#endif
	} else if (!strcmp((const char *)string, (const char *)"ap")) {
#ifndef K5_EVT_USB_ROUTE
		printk(KERN_INFO "set DBG_USB_CS_N to low!\n");
		gpio_set_value(DBG_USB_CS_N, 0);
#else
		gpio_set_value(USB_AP_CP_SWITCH, 0);
		gpio_set_value(DBG_USB_CS_N, 0);
		printk(KERN_EMERG "### K5 EVT board, set to AP path ###\n");
#endif
	} else {
		printk(KERN_INFO "Command Error :\n");
		printk(KERN_INFO "switch to ap : echo \"ap\" >> /proc/mfld_ap_cp_switch\n");
		printk(KERN_INFO "switch to cp : echo \"cp\" >> /proc/mfld_ap_cp_switch\n");
	}
	return nbytes;
}

static const struct file_operations proc_switch_operations;

static int __init ap_cp_switch_init(void)
{
	int ret;
	ret = gpio_request(DBG_USB_CS_N, 0);
	if (ret) {
		printk(KERN_INFO "Request DBG_USB_CS_N failed!\n");
		return 0;
	}

#ifdef K5_EVT_USB_ROUTE
	printk(KERN_INFO "### K5_EVT_USB_ROUTE is defined !!!###\n");
	ret = 0;
	ret = gpio_request(USB_AP_CP_SWITCH, 0);
	if (ret) {
		printk(KERN_ERR "USB_AP_CP_SWITCH request failed!\n");
		return 0;
	}
	gpio_direction_output(USB_AP_CP_SWITCH, 0);	//AP<--->USB connect
#endif
	gpio_direction_output(DBG_USB_CS_N, 0);
	proc_create_data(SWITCH_ENTRY, S_IFREG | S_IWUGO | S_IWUSR, NULL, &proc_switch_operations, NULL);
    gpio_free(DBG_USB_CS_N);
	return 0;
}

static void __exit ap_cp_switch_exit(void)
{
	gpio_free(DBG_USB_CS_N);
#ifdef K5_EVT_USB_ROUTE
	gpio_free(USB_AP_CP_SWITCH);
#endif
	remove_proc_entry(SWITCH_ENTRY, NULL);
	return ;
}

static const struct file_operations proc_switch_operations = {
	.owner	= THIS_MODULE,
	.write	= proc_switch_write,
};

MODULE_AUTHOR("lvxin1@lenovo.com");
MODULE_DESCRIPTION("AP/CP switch driver");
MODULE_LICENSE("GPL");

fs_initcall(ap_cp_switch_init);
module_exit(ap_cp_switch_exit);
