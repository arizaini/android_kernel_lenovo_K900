#ifndef _LINUX_FASTPOWER_H_
#define  _LINUX_FASTPOWER_H_

#define FASTPOWER_OFF_MODE	1
#define FASTPOWER_UP_DEFAULT_MODE	0
#define FASTPOWER_UP_PWRBTN_MODE	1
#define FASTPOWER_UP_USB_MODE	2

extern int get_fast_poweroff_status(void);
extern void set_fast_powerup_status(int value);

#endif
