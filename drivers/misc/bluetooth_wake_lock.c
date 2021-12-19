/*
 * bluetooth_wakelock.c - add function to bluetooth  wakelock 
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
 * Author: <gaofeng@lenovo.com>
 *
 * Note : This driver should be integrated with AP/CP switch driver, these similary
 *        function driver will be placed in the same /proc directory, for example:
 *        /proc/racer-switch/
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/sysdev.h>
#include <linux/proc_fs.h>
#include <linux/gpio.h>
#include <linux/wakelock.h>
#define ENTRY_BT_WAKELOCK	"bt_wakelock"
struct wake_lock brcm_lock;
static int bt_wake;
/*
 * Considering the Core gpio, need add 96
 */
#define UART_AUDIOJACK_SWITCH_PIN	(64+96)

#define SWITCH_DBG
#ifdef SWITCH_DBG
#define switch_dbg(format, args...)	printk(KERN_INFO "%s:"format, \
		__FUNCTION__, ##args)
#else
#define switch_dbg(fmt, args...)
#endif

#define UART_PATH_EVT	(1)
#define AUDIO_PAbluetooth_wakelock_writeTH_EVT	(0)

static int bluetooth_wakelock_read(char *page, char **start, off_t offset,
        int count, int *eof, void *data)
{
  //  *eof = 1;
  //  return  sprintf(page, "btwake:%u\n", bt_wake);
      return 0;
}


static ssize_t bluetooth_wakelock_write(struct file *file, const char __user *buf, ssize_t nbytes, loff_t *ppos)
{
	
  
    char string[nbytes];
    sscanf(buf, "%s", string);

    printk("entry bluetooth_wakelock_write \n");  
    if (!strcmp((const char *)string, (const char *)"0")){
        bt_wake = 0;
        printk("exit brcm_wakelock \n");
        wake_unlock(&brcm_lock); 
//        wake_lock_timeout(&brcm_lock,10*HZ);
    }
     else if (!strcmp((const char *)string, (const char *)"1")) {
            bt_wake = 1;
            printk("enter brcm_wakelock \n");
          wake_lock(&brcm_lock);
     } else {
         printk("entry bluetooth_wakelock_write \n");
     }
     return nbytes;
}

static const struct file_operations bt_wakelock_operations = {
	.owner	= THIS_MODULE,
	.write	= bluetooth_wakelock_write,
    .read = bluetooth_wakelock_read,
};


static int __init bluetooth_wakelock_init(void)
{
	struct proc_dir_entry	*pde;

	/* Setting UART & AUDIOJACK switch gpio, default switch to uart debug port */
    wake_lock_init(&brcm_lock, WAKE_LOCK_SUSPEND, "brcm_bt_lock");
	/*Create /proc/racer_switch/uart_audiojack_switch */
	pde = proc_create(ENTRY_BT_WAKELOCK, S_IFREG | S_IWUGO | S_IWUSR, NULL, &bt_wakelock_operations);
	if (!pde)
		goto err1;

	return 0;

err1:
	remove_proc_entry(ENTRY_BT_WAKELOCK, NULL);
	return -ENOMEM;
}


static void __exit bluetooth_wakelock_exit(void)
{
    wake_lock_destroy(&brcm_lock);
	remove_proc_entry(ENTRY_BT_WAKELOCK, NULL);
	return ;
}

MODULE_AUTHOR("gaofeng3@lenovo.com");
MODULE_DESCRIPTION("BLUETOOTH wakelock driver");
MODULE_LICENSE("GPL");

fs_initcall(bluetooth_wakelock_init);
module_exit(bluetooth_wakelock_exit);
