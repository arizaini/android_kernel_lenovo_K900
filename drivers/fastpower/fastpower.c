#include <linux/input.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <asm/io.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <linux/security.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/fastpower.h>

#define FASTPOWER_USR_MAJOR 333
#define FASTPOWER_OFF_CMD_SET	0
#define FASTPOWER_UP_CMD_GET	1

static int fastpower_dev_major=FASTPOWER_USR_MAJOR;
static int  MAX_BUF_LEN = 1024;
static char fastpower_dev_buf[1024];
static int fast_poweroff_occured = 0;
static int fast_powerup_occured = 0;

void fastpower_exit(void);

typedef struct 
{
    struct cdev dev;
    char inner_char[1024];

}fastpower_dev;

fastpower_dev *_pdev;

static int fastpower_open(struct inode *inode,struct file *filp)
{
    pr_err("fastpower device opened\n");
    return 0;
}

static int fastpower_release(struct inode *inode,struct file *filp)
{
    pr_err("fastpower device released\n");
    return 0;
}

static ssize_t fastpower_read(struct file * filp, char * buf, 
        size_t count, loff_t *ppos)
{
    int len;

    len=strlen(fastpower_dev_buf);
    pr_info("count = %d before read function\n",count);
    if (count < len)
        return -EINVAL;
    if (*ppos != 0)
        return 0;
    if (copy_to_user(buf, fastpower_dev_buf, len))
        return -EINVAL;
    *ppos = len;

    return len;
}

int get_fast_poweroff_status(void)
{
    return fast_poweroff_occured;
}
EXPORT_SYMBOL(get_fast_poweroff_status);

void set_fast_powerup_status(int value)
{
    fast_powerup_occured = value;
}
EXPORT_SYMBOL(set_fast_powerup_status);

static ssize_t fastpower_write(struct file *filp ,char * buf ,ssize_t count,loff_t* ppos)
{
    pr_info("count = %d before write function\n",count);
    if(count>MAX_BUF_LEN)
        count=MAX_BUF_LEN;
    copy_from_user(fastpower_dev_buf,buf,count);
    return count;
}

static int fastpower_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    void __user *argp = (void __user *)arg;

    pr_debug("ioctl in cmd=%d\n",cmd);
    switch (cmd) {
        case 0:
            copy_from_user(&fast_poweroff_occured,argp,sizeof(fast_poweroff_occured));
            pr_info("[_%d]fastpower_dev_buf=%d\n", __LINE__,fast_poweroff_occured);
            break;
        case 1:
            copy_to_user(argp, &fast_powerup_occured, sizeof(fast_powerup_occured));
            pr_info("[_%d]argp=%s\n", __LINE__,argp);
            break;
        default:
            break;
    }   
    return 0;
}

static const struct file_operations _fops=
{
    .owner = THIS_MODULE,
    .open = fastpower_open,
    .read = fastpower_read,
    .write = fastpower_write,
    .unlocked_ioctl = fastpower_ioctl,
    .release = fastpower_release,
};

static ssize_t fastpower_val_show(struct device *dev,struct device_attribute *attr,char *buf,size_t count)
{
    if(_pdev)
        pr_err("get fastpower_dev OK\n");
    sprintf(buf, "%s",_pdev->inner_char);
    return strlen(_pdev->inner_char);
}

static ssize_t fastpower_val_store(struct device *dev,struct device_attribute *attr,const char *buf,size_t count)
{
    if(count>MAX_BUF_LEN)
        count=MAX_BUF_LEN;
    sprintf(_pdev->inner_char,"%s",buf);
    return count;
}

static DEVICE_ATTR(val,S_IRUGO|S_IWUSR,fastpower_val_show,fastpower_val_store);
static void fastpower_setup_cdev(fastpower_dev *pdev,int index)
{
    int err,devno=MKDEV(fastpower_dev_major,index);
    cdev_init(&pdev->dev,&_fops);
    pdev->dev.owner=THIS_MODULE;
    pdev->dev.ops=&_fops;
    err=cdev_add(&pdev->dev,devno,1);
    if(err)
        pr_err("fastpower dev add  Error %d adding index %d\n",err,index);
}

struct class *my_class;

static int __init fastpower_init(void)
{
    int rs;
    struct device * temp = NULL;
    dev_t devno=MKDEV(fastpower_dev_major,0);   

    if(devno)
        rs=register_chrdev_region(devno,1,"fastpower"); 
    else
    {
        rs=alloc_chrdev_region(&devno,0,1,"fastpower"); 
        fastpower_dev_major=MAJOR(devno);  
    }

    if(rs<0){
        return rs;
    }
    _pdev=kmalloc(sizeof(fastpower_dev),GFP_KERNEL); 

    if(!_pdev)
    {
        rs=-ENOMEM;
        goto fail_malloc;
    }
    memset(_pdev,0,sizeof(fastpower_dev));  
    fastpower_setup_cdev(_pdev,0);

    my_class = class_create(THIS_MODULE, "fastpower_class");

    if(IS_ERR(my_class)) 
    {
        pr_err("fastpower module class create error\n");
        return -1; 
    }


    temp = device_create(my_class, NULL, MKDEV(fastpower_dev_major, 0),  NULL, "fastpower");
    rs = device_create_file(temp,&dev_attr_val);
    if(rs < 0){
        pr_err("fastpower--fail to create attribute val.\n");
        goto destroy_device;
    }

    return 0;
fail_malloc:
    pr_err("fastpower module fail malloc memory to struct fastpower_dev *\n");
    unregister_chrdev_region(devno,1);
    return rs;
destroy_device:
    pr_err("fastpower device failed\n");
    device_remove_file(temp,&dev_attr_val);
    return rs;
}

void fastpower_exit(void)
{
    dev_t devno = MKDEV (fastpower_dev_major, 0);

    cdev_del(&_pdev->dev);

    device_destroy(my_class, devno);         //delete device node under /dev
    class_destroy(my_class);                               //delete class created by us

    kfree(_pdev);

    unregister_chrdev_region(devno,1);
}

module_init(fastpower_init);
module_exit(fastpower_exit);

MODULE_LICENSE("GPL");
