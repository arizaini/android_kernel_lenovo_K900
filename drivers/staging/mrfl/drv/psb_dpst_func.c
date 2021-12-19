/******************************************************************************
 *
 * Copyright (c) 2011, Intel Corporation.
 * Portions (c), Imagination Technology, Ltd.
 * All rights reserved.
 *
 * Redistribution and Use.  Redistribution and use in binary form, without
 * modification, of the software code provided with this license ("Software"),
 * are permitted provided that the following conditions are met:
 *
 *  1. Redistributions must reproduce the above copyright notice and this
 *     license in the documentation and/or other materials provided with the
 *     Software.
 *  2. Neither the name of Intel Corporation nor the name of Imagination
 *     Technology, Ltd may be used to endorse or promote products derived from
 *     the Software without specific prior written permission.
 *  3. The Software can only be used in connection with the Intel hardware
 *     designed to use the Software as outlined in the documentation. No other
 *     use is authorized.
 *  4. No reverse engineering, decompilation, or disassembly of the Software
 *     is permitted.
 *  5. The Software may not be distributed under terms different than this
 *     license.
 *
 * Limited Patent License.  Intel Corporation grants a world-wide, royalty-free
 * , non-exclusive license under patents it now or hereafter owns or controls
 * to make, have made, use, import, offer to sell and sell ("Utilize") the
 * Software, but solely to the extent that any such patent is necessary to
 * Utilize the Software alone.  The patent license shall not apply to any
 * combinations which include the Software.  No hardware per se is licensed
 * hereunder.
 *
 * Ownership of Software and Copyrights. Title to all copies of the Software
 * remains with the copyright holders. The Software is copyrighted and
 * protected by the laws of the United States and other countries, and
 * international treaty provisions.
 *
 * DISCLAIMER.  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
******************************************************************************/

#include <linux/version.h>
    
#include "psb_drv.h"
#include "psb_intel_reg.h"
#include "psb_dpst.h"
#include "dispmgrnl.h"
#include "psb_dpst_func.h"


{
	
	
	
	    /* before we send get the status for run_algorithm */ 
	    dpst_histogram_get_status(g_dev, &mydata);
	
	
	
	
	
	



/* IOCTL - moved to standard calls for Kernel Integration */ 

{
	
	
	
	
	
	
	       (OSPM_DISPLAY_ISLAND, OSPM_UHB_ONLY_IF_ON)) {
		
	
	
		
		
		
		
		
		
		
		
		
		
		
		
		    /* Wait for two vblanks */ 
	} else {
		
		
		
		
		
		
		
		
		
		
	
	
	



{
	
	
	
	
	
	
	
	
	
	
	
	
	
	       (OSPM_DISPLAY_ISLAND, OSPM_UHB_ONLY_IF_ON)) {
		
	
	
	
	
	
	
		
		
			
		
			
			
			
			
		
	
	
	



// SH START DIET
// SH TODO This doesn't work yet.
int psb_diet_enable(struct drm_device *dev, void *data) 
{
	
	
	
	
	
	
	
	
	
	
	
	
	
	       (OSPM_DISPLAY_ISLAND, OSPM_UHB_ONLY_IF_ON)) {
		
	
	
		
		
		    dpst3_bin_threshold_count;
		
		
		
			
		
		
		
		
		
	
	
	else {
		
		
		
	
	
	



// SH END

{
	
	
	
	
	
	
		
		    /*find handle to drm kboject */ 
		    pdev = dev->pdev;
		
		
		
			
			    /*init dpst kmum comms */ 
			    dev_priv->psb_dpst_state = psb_dpst_init(kobj);
		
		
		
					   
	
		
		    /*hotplug and dpst destroy examples */ 
		    psb_irq_disable_dpst(dev);
		
					   
		
		
	
	



{
	
	
	
	
	
	
	       (OSPM_DISPLAY_ISLAND, OSPM_UHB_ONLY_IF_ON)) {
		
	
	
	
	
	    /* horizontal is the left 16 bits */ 
	    x = reg >> 16;
	
	    /* vertical is the right 16 bits */ 
	    y = reg & 0x0000ffff;
	
	    /* the values are the image size minus one */ 
	    x += 1;
	
	
	



{
	
	
	
	
	       (OSPM_DISPLAY_ISLAND, OSPM_UHB_ONLY_IF_ON)) {
		
	
	
	
	
	
	    /* printk(KERN_ALERT "guardband = %u\ninterrupt delay = %u\n",
	       reg_data.guardband, reg_data.guardband_interrupt_delay); */ 
	    PSB_WVDC32(reg_data.data, HISTOGRAM_INT_CONTROL);
	
	



/* Initialize the dpst data */ 
int dpst_init(struct drm_device *dev, int level, int output_id) 
{
	
	



{
	
	
	
	



			   int dst_group_id) 
{
	
	
	
	
	    /* Call into UMComm layer to receive histogram interrupts */ 
	    //eventval = Xpsb_kmcomm_get_kmevent((void *)tid);
	    /* fprintf(stderr, "Got message %d for DPST\n", eventval); */ 
	    messageType = notify_disp_obj->kobj.name[0];	/* need to debug to figure out which field this is */
	
		
	
	case 'h':		//DPST_EVENT_HIST_INTERRUPT:
		/* DPST histogram */ 
		    send_hist();
		
	
		break;
	
		break;
	
		
		    /* disable DPST */ 
		    do_not_quit = 0;
		
		



{
	
	
	    /* enable histogram interrupts */ 
	    ret = psb_hist_enable(dev, &enable);
	



				 
{
	
	
	
	
		
			"Error: histogram get status ioctl returned error: %d\n",
			ret);
		
	
	



{
	
	
	
	
	
#ifdef CONFIG_BACKLIGHT_CLASS_DEVICE
	    bd.props.brightness = psb_get_brightness(&bd);
	
	
#endif				/* 
	    return 0;



{
	
	
//      struct drm_mode_object *obj;
	struct drm_crtc *crtc;
	
	
	
	
	
//      int32_t obj_id;
	    
//      obj_id = lut_arg->output_id;
//      obj = drm_mode_object_find(dev, obj_id, DRM_MODE_OBJECT_CONNECTOR);
//      if (!obj) {
//              printk(KERN_ERR "Invalid Connector object, id = %d\n", obj_id);
//              DRM_DEBUG("Invalid Connector object.\n");
//              return -EINVAL;
//      }
	    
	crtc = connector->encoder->crtc;
	
	
		
	
	
	



{
	
	
		
			
				
				    *((unsigned long *)cmd_hdr->data);
			
			
			
			
			
			
			
			
			
		
		
	
		
			
				
				    *((unsigned long *)cmd_hdr->data);
				
				
			
		
	
		
			
				
				    *((unsigned long *)cmd_hdr->data);
				
				
			
		
	
		
			
				
				    *((unsigned long *)cmd_hdr->data);
				
				
			
		
	
		
			
				
				    *((unsigned long *)cmd_hdr->data);
				
				
			
		
	
		
			
				
				
			
		
	
		
			
				
				
			
		
	
		
			
		
		
	
		
			
			    ("kdispmgr: received unknown dpst command = %d.\n",
			     cmd_hdr->cmd);
		
	


