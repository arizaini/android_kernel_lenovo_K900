/**************************************************************************
 * Copyright (c) 2012, Intel Corporation.
 * All Rights Reserved.

 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors:
 *    Hitesh K. Patel <hitesh.k.patel@intel.com>
 *    Dale B. Stimson <dale.b.stimson@intel.com>
 *    Jari Luoma-aho  <jari.luoma-aho@intel.com>
 *    Jari Nippula    <jari.nippula@intel.com>
 *
 */

#if (defined CONFIG_GPU_BURST) || (defined CONFIG_GPU_BURST_MODULE)

#if !(defined(SUPPORT_SGX_HWPERF))
#warning CONFIG_GPU_BURST requires SUPPORT_SGX_HWPERF
#endif

#include <linux/module.h>

#include "sgxdefs.h"
#include "sgxmmu.h"
#include "services_headers.h"
#include "buffer_manager.h"
#include "sgxapi_km.h"
#include "sgxinfo.h"
#include "sgx_mkif_km.h"
#include "sgxconfig.h"
#include "sysconfig.h"
#include "pvr_bridge_km.h"

#include "sgx_bridge_km.h"

#include "pdump_km.h"
#include "ra.h"
#include "mmu.h"
#include "handle.h"
#include "perproc.h"

#include "sgxutils.h"
#include "pvrversion.h"
#include "sgx_options.h"

#include "lists.h"
#include "srvkm.h"
#include "ttrace.h"

#include "gburst_hw.h"
#include "gburst_hw_if.h"


/**
 * GBURST_UPDATE_GPU_TIMING - Define non-zero to update timing information
 * in the pvr driver.  It would be desirable to have this turned on, but
 * potential instability issues need to be resolved first.
 */

#if (!defined GBURST_UPDATE_GPU_TIMING)
#define GBURST_UPDATE_GPU_TIMING 1
#endif


/**
 * There are PVRSRV_SGX_HWPERF_NUM_COUNTERS such counters per gpu core,
 * where, if defined (SGX544) (and therefore, having
 * SGX_FEATURE_EXTENDED_PERF_COUNTERS defined), the numbers of
 * counters are defined as:
 *     #define PVRSRV_SGX_HWPERF_NUM_COUNTERS       8
 *     #define PVRSRV_SGX_HWPERF_NUM_MISC_COUNTERS 11
 */

#define NUM_COUNTERS (PVRSRV_SGX_HWPERF_NUM_COUNTERS)
#define GBURST_MONITORED_COUNTER_FIRST    6
#define GBURST_MONITORED_COUNTER_LAST     7

/**
 * Utilization calculation per counter uses the following formula:
 * utilization = 100 * current_utilization / maximum_utilization, where
 * maximum_utilization = pi_coeff * timestamp_delta
 *   - pi_coeff = cycles per timestamp increment * cycles per counter update
 *      + cycles per timestamp increment = 16 (timer updates every 16th
 *        GPU cycle)
 *      + cycles per counter update = GPU cycles spend per one counter update
 *   - timestamp_delta = timestamp_end - timestamp_start
 *      + time counter values between two consecutive counter entry
 * current_utilization
 *      + performance counter value increment between two consecutive counter
 *         entries
 */

struct perf_counter_info_s {
	u32 pi_group;       /* The counter group (in device). */
	u32 pi_bit;         /* The counter bit (in device). */
	u32 pi_coeff;	    /* Counter coefficient (see above) */
	u32 pi_cntr_bits;   /* Counter bits used for sum (in device) */
	u32 pi_summux;      /* Sum/Mux info, 1=sum 0=mux (in device) */
};


static const struct perf_counter_info_s pidat_initial[NUM_COUNTERS] = {
	{ 63, 0, 16, 0, 0 },    /* 0: group, id, coeff, counter_bits, SumMux */
	{ 63, 0, 16, 0, 0 },    /* 1: group, id, coeff, counter_bits, SumMux */
	{ 63, 0, 16, 0, 0 },    /* 2: group, id, coeff, counter_bits, SumMux */
	{ 63, 0, 16, 0, 0 },    /* 3: group, id, coeff, counter_bits, SumMux */
	{ 63, 0, 16, 0, 0 },    /* 4: group, id, coeff, counter_bits, SumMux */
	{ 63, 0, 16, 0, 0 },    /* 5: group, id, coeff, counter_bits, SumMux */
	{  1, 0, 64, 3, 1 },    /* 6: group, id, coeff, counter_bits, SumMux */
	{  1, 4, 64, 3, 1 },    /* 7: group, id, coeff, counter_bits, SumMux */
};


static struct perf_counter_info_s pidat[NUM_COUNTERS];


static int gburst_hw_initialized;


/* gburst_sgx_data - Information from the graphics driver: */
static struct gburst_hw_if_info_s gburst_sgx_data;


/**
 * gburst_hw_initialization_complete -- Attempt initialization,
 * if not already done.
 * Function return value:  1 if initialized, 0 if not.
 */
static inline int gburst_hw_initialization_complete(void)
{
	if (!gburst_hw_initialized)
		return gburst_hw_init();
	return 1;
}


int gburst_hw_inq_num_counters(int *ctr_first, int *ctr_last)
{
	*ctr_first = GBURST_MONITORED_COUNTER_FIRST;
	*ctr_last = GBURST_MONITORED_COUNTER_LAST;
	return NUM_COUNTERS;
}
EXPORT_SYMBOL(gburst_hw_inq_num_counters);


int gburst_hw_inq_num_cores(void)
{
	return SGX_FEATURE_MP_CORE_COUNT_3D;
}
EXPORT_SYMBOL(gburst_hw_inq_num_cores);


/**
 * gburst_hw_select_counters - Select specified counters in hardware.
 */
static void gburst_hw_select_counters(struct perf_counter_info_s *cdef)
{
	unsigned int i;
	PVRSRV_DEVICE_NODE *psDeviceNode;
	PVRSRV_SGXDEV_INFO *psDevInfo;

	psDeviceNode = gburst_sgx_data.gsh_gburst_psDeviceNode;
	psDevInfo = psDeviceNode->pvDevice;

	for (i = 0 ; i < NUM_COUNTERS ; i++) {
		/* The counter group (in device). */
		psDevInfo->psSGXHostCtl->aui32PerfGroup[i] = cdef[i].pi_group;
		/* The counter bit (in device). */
		psDevInfo->psSGXHostCtl->aui32PerfBit[i] = cdef[i].pi_bit;
		psDevInfo->psSGXHostCtl->ui32PerfCounterBitSelect &=
							~(0xF << (4*i));
		psDevInfo->psSGXHostCtl->ui32PerfCounterBitSelect |=
			( (cdef[i].pi_cntr_bits << 4*i) & (0xF << (4*i)) );

		/* Select SumMux value */
		if(cdef[i].pi_summux)
			psDevInfo->psSGXHostCtl->ui32PerfSumMux |=
					( (cdef[i].pi_summux & 1) << (8+i) );
		else
			psDevInfo->psSGXHostCtl->ui32PerfSumMux &=
							~( 1 << (8+i) );
	}

	return;
}


/**
 * gburst_hw_set_perf_status_periodic -- Tell the hardware to record performance
 * and which performance data to record.
 * Function return value: < 0 for error, otherwise 0.
 */
int gburst_hw_set_perf_status_periodic(int on_or_off)
{
	PVRSRV_DEVICE_NODE *psDeviceNode;
	IMG_HANDLE hDevMemContext;
	PVRSRV_ERROR eError;
	SGXMKIF_COMMAND sCommandData = {0};
	IMG_UINT32 cmd_code;

	if (!gburst_hw_initialization_complete())
		return -EINVAL;

	psDeviceNode = gburst_sgx_data.gsh_gburst_psDeviceNode;
	hDevMemContext = (IMG_HANDLE)
		psDeviceNode->sDevMemoryInfo.pBMKernelContext;

	/**
	 * FIXME - Described in document: FIXME
	 * FIXME - for these to work, register PERF_DEBUG_CTRL must
	 *    be used to enable performance and debug counters.
	 *    Not enabled for normal operation in order to save power.
	 *
	 * Command SGXMKIF_CMD_SETHWPERFSTATUS == 9, and is a member
	 * of enumeration type enum _SGXMKIF_CMD_TYPE_ .
	 *
	 * The command here is a bit mask, consisting of the OR of:
     *   PVRSRV_SGX_HWPERF_STATUS_OFF              (0x0)
     *   PVRSRV_SGX_HWPERF_STATUS_RESET_COUNTERS   (1UL << 0)
     *   PVRSRV_SGX_HWPERF_STATUS_GRAPHICS_ON      (1UL << 1)
     *   PVRSRV_SGX_HWPERF_STATUS_PERIODIC_ON      (1UL << 2)
     *   PVRSRV_SGX_HWPERF_STATUS_MK_EXECUTION_ON  (1UL << 3)
     */
	if (on_or_off)
		cmd_code = PVRSRV_SGX_HWPERF_STATUS_PERIODIC_ON;
	else
		cmd_code = PVRSRV_SGX_HWPERF_STATUS_OFF;

	sCommandData.ui32Data[0] = cmd_code;

	eError = SGXScheduleCCBCommandKM(psDeviceNode,
		SGXMKIF_CMD_SETHWPERFSTATUS, &sCommandData, KERNEL_ID,
		0, hDevMemContext, IMG_FALSE);
	if (eError != PVRSRV_OK)
		return -EIO;

	return 0;
}
EXPORT_SYMBOL(gburst_hw_set_perf_status_periodic);


int gburst_hw_inq_counter_id(unsigned int ctr_ix, int *ctr_grp, int *ctr_bit,
							int *cntrbits, int *summux)
{
	PVRSRV_SGXDEV_INFO *psDevInfo;
	if (!gburst_hw_initialization_complete())
		return -EINVAL;
	if (ctr_ix >= NUM_COUNTERS)
		return -EINVAL;

	/* Update local counter group/bit status from DevInfo */
	psDevInfo = gburst_sgx_data.gsh_gburst_psDeviceNode->pvDevice;
	pidat[ctr_ix].pi_group = psDevInfo->psSGXHostCtl
				->aui32PerfGroup[ctr_ix];
	pidat[ctr_ix].pi_bit = psDevInfo->psSGXHostCtl->aui32PerfBit[ctr_ix];

	*ctr_grp = pidat[ctr_ix].pi_group;
	*ctr_bit = pidat[ctr_ix].pi_bit;

	*cntrbits = pidat[ctr_ix].pi_cntr_bits;
	*summux = pidat[ctr_ix].pi_summux;
	return 0;
}
EXPORT_SYMBOL(gburst_hw_inq_counter_id);


int gburst_hw_set_counter_id(unsigned int ctr_ix, int ctr_grp, int ctr_bit,
							int cntrbits, int summux)
{
	PVRSRV_SGXDEV_INFO *psDevInfo;

	if (!gburst_hw_initialization_complete())
		return -EINVAL;

	/* Protect PVRScopeService client's counter usage */
	if (ctr_ix < GBURST_MONITORED_COUNTER_FIRST)
		return -EINVAL;
	if (ctr_ix > GBURST_MONITORED_COUNTER_LAST)
		return -EINVAL;
	if (ctr_grp >= 128)
		return -EINVAL;
	if (ctr_bit >= 32)
		return -EINVAL;

	psDevInfo = gburst_sgx_data.gsh_gburst_psDeviceNode->pvDevice;

	pidat[ctr_ix].pi_group = ctr_grp;
	pidat[ctr_ix].pi_bit = ctr_bit;
	pidat[ctr_ix].pi_cntr_bits = cntrbits;
	pidat[ctr_ix].pi_summux = summux;

	psDevInfo->psSGXHostCtl->aui32PerfGroup[ctr_ix] = ctr_grp;
	psDevInfo->psSGXHostCtl->aui32PerfBit[ctr_ix] = ctr_bit;

	/* Select CounterBits value */
	psDevInfo->psSGXHostCtl->ui32PerfCounterBitSelect &=
									~(0xF << 4*ctr_ix);
	psDevInfo->psSGXHostCtl->ui32PerfCounterBitSelect |=
			((cntrbits << 4*ctr_ix) & (0xF << 4*ctr_ix));

	/* Select SumMux value */
	if(summux)
		psDevInfo->psSGXHostCtl->ui32PerfSumMux |=
					((summux & 1) << (8+ctr_ix));
	else
		psDevInfo->psSGXHostCtl->ui32PerfSumMux &=
								~(1 << (8+ctr_ix));

	return 0;
}
EXPORT_SYMBOL(gburst_hw_set_counter_id);


int gburst_hw_inq_counter_coeff(unsigned int ctr_ix)
{
	if (!gburst_hw_initialization_complete())
		return -EINVAL;
	if (ctr_ix >= NUM_COUNTERS)
		return -EINVAL;

	return pidat[ctr_ix].pi_coeff;
}
EXPORT_SYMBOL(gburst_hw_inq_counter_coeff);


int gburst_hw_gpu_freq_mhz_info(int freq_MHz)
{
	if (!gburst_hw_initialization_complete())
		return -EINVAL;

#if GBURST_UPDATE_GPU_TIMING
	{
		const IMG_BOOL bIdleDevice = IMG_FALSE;
		const PVRSRV_DEV_POWER_STATE eCurrentPowerState =
			PVRSRV_DEV_POWER_STATE_ON;
		PVRSRV_ERROR eError;

		/**
		 * Update frequency information as needed by function
		 * SGXUpdateTimingInfo.
		 */
		{
			SGX_TIMING_INFORMATION *psTimingInfo;
#if defined(SGX_DYNAMIC_TIMING_INFO)
			{
				SGX_TIMING_INFORMATION sSGXTimingInfo = {0};
				psTimingInfo = &sSGXTimingInfo;
				SysGetSGXTimingInformation(psTimingInfo);
			}
#else
			{
				SGX_DEVICE_MAP *psSGXDeviceMap;
				SysGetDeviceMemoryMap(PVRSRV_DEVICE_TYPE_SGX,
					(IMG_VOID **) &psSGXDeviceMap);
				psTimingInfo = &psSGXDeviceMap->sTimingInfo;
			}
#endif

			psTimingInfo->ui32CoreClockSpeed = freq_MHz * 1000000;
		}

		eError = SGXPreClockSpeedChange(
			gburst_sgx_data.gsh_gburst_psDeviceNode, bIdleDevice,
			eCurrentPowerState);

		eError = SGXPostClockSpeedChange(
			gburst_sgx_data.gsh_gburst_psDeviceNode, bIdleDevice,
			eCurrentPowerState);
		if (eError != PVRSRV_OK)
			return -EIO;
	}
#endif /* if GBURST_UPDATE_GPU_TIMING */

	return 0;
}
EXPORT_SYMBOL(gburst_hw_gpu_freq_mhz_info);


int gburst_hw_perf_data_get_indices(int *ix_roff, int *ix_woff)
{
	SGXMKIF_HWPERF_CB *psHWPerfCB;

	if (!gburst_hw_initialization_complete())
		return -EINVAL;

	psHWPerfCB = gburst_sgx_data.gsh_gburst_psHWPerfCB;
	*ix_roff = psHWPerfCB->ui32Roff;
	*ix_woff = psHWPerfCB->ui32Woff;

	return 0;
}
EXPORT_SYMBOL(gburst_hw_perf_data_get_indices);


/* Time stamp is gpu clock divided by 16. */
int gburst_hw_perf_data_get_data(uint32_t *time_stamp, int *counters_storable,
	uint32_t **p_pdat)
{
	SGXMKIF_HWPERF_CB *psHWPerfCB;
	SGXMKIF_HWPERF_CB_ENTRY *psMKPerfEntry;

	if (!gburst_hw_initialization_complete())
		return -EINVAL;

	psHWPerfCB = gburst_sgx_data.gsh_gburst_psHWPerfCB;
	psMKPerfEntry = psHWPerfCB->psHWPerfCBData + psHWPerfCB->ui32Roff;

	/* Time stamp is gpu clock divided by 16. */
	*time_stamp = psMKPerfEntry->ui32Time;
	if (psMKPerfEntry->ui32Type == PVRSRV_SGX_HWPERF_PERIODIC)
		*counters_storable = 1;
	else
		*counters_storable = 0;

	*p_pdat = (uint32_t *) psMKPerfEntry->ui32Counters;

	return 0;
}
EXPORT_SYMBOL(gburst_hw_perf_data_get_data);


int gburst_hw_perf_data_read_index_incr(uint32_t *ix_roff)
{
	SGXMKIF_HWPERF_CB *psHWPerfCB;

	if (!gburst_hw_initialization_complete())
		return -EINVAL;

	psHWPerfCB = gburst_sgx_data.gsh_gburst_psHWPerfCB;

	/* Step to next entry (if any) in circular buffer. */
	psHWPerfCB->ui32Roff =
		(psHWPerfCB->ui32Roff + 1) & (SGXMKIF_HWPERF_CB_SIZE - 1);

	*ix_roff = psHWPerfCB->ui32Roff;

	return 0;
}
EXPORT_SYMBOL(gburst_hw_perf_data_read_index_incr);


/**
 * gburst_hw_init -- Attempt initialization.
 * Function return value:  1 if initialized, 0 if not.
 */
int gburst_hw_init(void)
{
	int i;
	int sts;

	for (i = 0; i < NUM_COUNTERS; i++)
		pidat[i] = pidat_initial[i];

	sts = gburst_hw_if_get_info(&gburst_sgx_data);
	if (sts < 0)
		return 0;

	if (!gburst_sgx_data.gsh_initialized)
		return 0;

	gburst_hw_initialized = 1;

	gburst_hw_select_counters(pidat);

	return 1;
}
EXPORT_SYMBOL(gburst_hw_init);

#endif /* if (defined CONFIG_GPU_BURST) || (defined CONFIG_GPU_BURST_MODULE) */
