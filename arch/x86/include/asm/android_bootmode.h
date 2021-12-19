/*
 * Copyright (c) 2012 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * The full GNU General Public License is included in this distribution in
 * the file called "COPYING".
 */

#ifndef __ANDROID_BOOTMODE__
#define __ANDROID_BOOTMODE__

/* supported android platform boot modes */
enum android_bootmode {
	BOOTMODE_UNKNOWN,
	BOOTMODE_CHARGER,
	BOOTMODE_MAIN,
	BOOTMODE_FOTA,
};

int read_boot_mode(void);
#endif /* __ANDROID_BOOTMODE__ */
