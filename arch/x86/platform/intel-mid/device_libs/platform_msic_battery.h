/*
 * platform_msic_battery.h: MSIC battery platform data header file
 *
 * (C) Copyright 2008 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */
#ifndef _PLATFORM_MSIC_BATTERY_H_
#define _PLATFORM_MSIC_BATTERY_H_

extern void __init *msic_battery_platform_data(void *info)
			__attribute__((weak));

extern int intel_msic_save_config_data(const char *name,
					void *data, int len);
extern int intel_msic_restore_config_data(const char *name,
					void *data, int len);
#endif
