/*
 * Copyright © 2010 Intel Corporation
 *
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
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 * Jim Liu <jim.liu@intel.com>
 * Jackie Li<yaodong.li@intel.com>
 * Chao Jiang <chao.jiang@intel.com>
 */

#include "displays/otm1280a_vid.h"
#include "mdfld_output.h"
#include "mdfld_dsi_dpi.h"
#include "mdfld_dsi_pkg_sender.h"
#include "otm1280a.h"

#define GPIO_MIPI_PANEL_RESET 57

struct drm_display_mode*
otm1280a_vid_get_config_mode(struct drm_device *dev)
{
	struct drm_display_mode *mode;
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *) dev->dev_private;
	struct mrst_timing_info *ti = &dev_priv->gct_data.DTD;
	bool use_gct = false; /*Disable GCT for now*/

	PSB_DEBUG_ENTRY("\n");

	mode = kzalloc(sizeof(*mode), GFP_KERNEL);
	if (!mode)
		return NULL;
	PSB_DEBUG_ENTRY("lenovo\n");

	if (use_gct) {
		PSB_DEBUG_ENTRY("gct find MIPI panel.\n");

		mode->hdisplay = (ti->hactive_hi << 8) | ti->hactive_lo;
		mode->vdisplay = (ti->vactive_hi << 8) | ti->vactive_lo;
		mode->hsync_start = mode->hdisplay + \
				((ti->hsync_offset_hi << 8) | \
				ti->hsync_offset_lo);
		mode->hsync_end = mode->hsync_start + \
				((ti->hsync_pulse_width_hi << 8) | \
				ti->hsync_pulse_width_lo);
		mode->htotal = mode->hdisplay + ((ti->hblank_hi << 8) | \
								ti->hblank_lo);
		mode->vsync_start = \
			mode->vdisplay + ((ti->vsync_offset_hi << 8) | \
						ti->vsync_offset_lo);
		mode->vsync_end = \
			mode->vsync_start + ((ti->vsync_pulse_width_hi << 8) | \
						ti->vsync_pulse_width_lo);
		mode->vtotal = mode->vdisplay + \
				((ti->vblank_hi << 8) | ti->vblank_lo);
		mode->clock = ti->pixel_clock * 10;

		PSB_DEBUG_ENTRY("hdisplay is %d\n", mode->hdisplay);
		PSB_DEBUG_ENTRY("vdisplay is %d\n", mode->vdisplay);
		PSB_DEBUG_ENTRY("HSS is %d\n", mode->hsync_start);
		PSB_DEBUG_ENTRY("HSE is %d\n", mode->hsync_end);
		PSB_DEBUG_ENTRY("htotal is %d\n", mode->htotal);
		PSB_DEBUG_ENTRY("VSS is %d\n", mode->vsync_start);
		PSB_DEBUG_ENTRY("VSE is %d\n", mode->vsync_end);
		PSB_DEBUG_ENTRY("vtotal is %d\n", mode->vtotal);
		PSB_DEBUG_ENTRY("clock is %d\n", mode->clock);
	} else {
 		printk("wangxz3:modified clock\n");
		mode->hdisplay = 720;
		mode->vdisplay = 1280;
		mode->hsync_start = 720 + 10;
		mode->hsync_end = 720 + 18 + 18;
		mode->htotal = 720 + 10 + 18 + 18;
		mode->vsync_start = 1280 + 4;
		mode->vsync_end = 1280 + 8 + 8;
		mode->vtotal = 1280 + 8 + 4 + 8;
                mode->vrefresh = 60;
		mode->clock = mode->vrefresh * mode->vtotal * mode->htotal/1000;
	}

	drm_mode_set_name(mode);
	drm_mode_set_crtcinfo(mode, 0);

	mode->type |= DRM_MODE_TYPE_PREFERRED;

	return mode;
}

static int otm1280a_vid_get_panel_info(struct drm_device *dev,
				int pipe,
				struct panel_info *pi)
{
	if (!dev || !pi)
		return -EINVAL;

	pi->width_mm = TMD_PANEL_WIDTH;
	pi->height_mm = TMD_PANEL_HEIGHT;

	PSB_DEBUG_ENTRY("lenovo\n");
	return 0;
}

/* ************************************************************************* *\
 * FUNCTION: mdfld_init_otm1280a_MIPI
 *
 * DESCRIPTION:  This function is called only by mrst_dsi_mode_set and
 *               restore_display_registers.  since this function does not
 *               acquire the mutex, it is important that the calling function
 *               does!
\* ************************************************************************* */
static int mdfld_dsi_otm1280a_drv_ic_init(struct mdfld_dsi_config *dsi_config, int pipe)
{
	u32 ret, i, j = 0;
    int state = 0;
	struct drm_device *dev = dsi_config->dev;
	struct mdfld_dsi_pkg_sender *sender
			= mdfld_dsi_get_pkg_sender(dsi_config);
	DRM_INFO("OTM1280A display initial sequence.\n");

	if (!sender) {
		DRM_ERROR("Cannot get sender\n");
		return -ENODEV;
	}
	PSB_DEBUG_ENTRY("lenovo\n");

	/*wait for 5 ms*/
    mdelay(1);

	for (i = 0; i < ARRAY_SIZE(len); i++) {
		REG_WRITE(sender->mipi_data_len_reg, datalen[i]);
		state = mdfld_dsi_send_gen_long_lp(sender, &otm_cmd[j], datalen[i]+1, 0);
        if(state < 0)
            goto quit_init;
		j += len[i];
        udelay(5);
#if 0 
        REG_WRITE(0xb000, 9); 
		//udelay(1);
		udelay(5);
        REG_WRITE(0xb000, 1);
#endif
	}

	mdelay(1);
	ret = 0xD036;
	REG_WRITE(sender->mipi_data_len_reg, 1);
	state = mdfld_dsi_send_gen_long_lp(sender, &ret, 2, 0);
    
#if 0
    REG_WRITE(0xb000, 9);
    udelay(5);
    REG_WRITE(0xb000, 1);
#endif

	mdelay(1);

quit_init:    
    return state;
}

static void
mdfld_dsi_otm1280a_dsi_controller_init(struct mdfld_dsi_config *dsi_config,
				int pipe, int update)
{
	struct mdfld_dsi_hw_context *hw_ctx =
		&dsi_config->dsi_hw_context;
	struct drm_device *dev = dsi_config->dev;

	int lane_count = 4;
	u32 val = 0;
	u32 mipi_control_reg = dsi_config->regs.mipi_control_reg;
	u32 intr_en_reg = dsi_config->regs.intr_en_reg;
	u32 hs_tx_timeout_reg = dsi_config->regs.hs_tx_timeout_reg;
	u32 lp_rx_timeout_reg = dsi_config->regs.lp_rx_timeout_reg;
	u32 turn_around_timeout_reg =
		dsi_config->regs.turn_around_timeout_reg;
	u32 device_reset_timer_reg =
		dsi_config->regs.device_reset_timer_reg;
	u32 high_low_switch_count_reg =
		dsi_config->regs.high_low_switch_count_reg;
	u32 init_count_reg = dsi_config->regs.init_count_reg;
	u32 eot_disable_reg = dsi_config->regs.eot_disable_reg;
	u32 lp_byteclk_reg = dsi_config->regs.lp_byteclk_reg;
	u32 clk_lane_switch_time_cnt_reg =
		dsi_config->regs.clk_lane_switch_time_cnt_reg;
	u32 video_mode_format_reg =
		dsi_config->regs.video_mode_format_reg;
	u32 dphy_param_reg = dsi_config->regs.dphy_param_reg;
	u32 dsi_func_prg_reg = dsi_config->regs.dsi_func_prg_reg;
	PSB_DEBUG_ENTRY("lenovo\n");

	PSB_DEBUG_ENTRY("%s: initializing dsi controller on pipe %d\n",
			__func__, pipe);

	dsi_config->lane_count = 4;
	hw_ctx->mipi_control = 0x18;
	hw_ctx->intr_en = 0xffffffff;
	hw_ctx->hs_tx_timeout = 0x9f880;
	hw_ctx->lp_rx_timeout = 0xffff;
	hw_ctx->turn_around_timeout = 0x14;
	hw_ctx->device_reset_timer = 0xffff;
	hw_ctx->high_low_switch_count = 0x46;
	hw_ctx->init_count = 0x7d0;
	hw_ctx->eot_disable = 0x2;
	hw_ctx->lp_byteclk = 0x4;
	hw_ctx->clk_lane_switch_time_cnt = 0xa0014;
	hw_ctx->dphy_param = 0x120A2B0C; //0x150c3408; //0x120D2B0C; //0x281c3014;

	/*setup video mode format*/
	val = 0;
	val = dsi_config->video_mode | DSI_DPI_COMPLETE_LAST_LINE;
	hw_ctx->video_mode_format = val;

	/*set up func_prg*/
	val = 0;
	val |= lane_count;
	val |= dsi_config->channel_num << DSI_DPI_VIRT_CHANNEL_OFFSET;

	switch (dsi_config->bpp) {
	case 16:
		val |= DSI_DPI_COLOR_FORMAT_RGB565;
		break;
	case 18:
		val |= DSI_DPI_COLOR_FORMAT_RGB666;
		break;
	case 24:
		val |= DSI_DPI_COLOR_FORMAT_RGB888;
		break;
	default:
		DRM_ERROR("unsupported color format, bpp = %d\n",
			dsi_config->bpp);
	}
	hw_ctx->dsi_func_prg = val;
	if (update) {
		REG_WRITE(dphy_param_reg, hw_ctx->dphy_param);
		REG_WRITE(mipi_control_reg, hw_ctx->mipi_control);
		REG_WRITE(intr_en_reg, hw_ctx->intr_en);
		REG_WRITE(hs_tx_timeout_reg, hw_ctx->hs_tx_timeout);
		REG_WRITE(lp_rx_timeout_reg, hw_ctx->lp_rx_timeout);
		REG_WRITE(turn_around_timeout_reg, hw_ctx->turn_around_timeout);
		REG_WRITE(device_reset_timer_reg, hw_ctx->device_reset_timer);
		REG_WRITE(high_low_switch_count_reg,
			hw_ctx->high_low_switch_count);
		REG_WRITE(init_count_reg, hw_ctx->init_count);
		REG_WRITE(eot_disable_reg, hw_ctx->eot_disable);
		REG_WRITE(lp_byteclk_reg, hw_ctx->lp_byteclk);
		REG_WRITE(clk_lane_switch_time_cnt_reg,
			hw_ctx->clk_lane_switch_time_cnt);
		REG_WRITE(video_mode_format_reg, hw_ctx->video_mode_format);
		REG_WRITE(dsi_func_prg_reg, hw_ctx->dsi_func_prg);
	}
}

static int
mdfld_dsi_otm1280a_get_power_state(struct mdfld_dsi_config *dsi_config,
				int pipe)
{
	PSB_DEBUG_ENTRY("Getting power state...");

	/*All panels have been turned off during panel detection*/
	PSB_DEBUG_ENTRY("lenovo\n");
	return MDFLD_DSI_PANEL_POWER_OFF;
}

static int mdfld_dsi_otm1280a_power_on(struct mdfld_dsi_config *dsi_config)
{
	struct mdfld_dsi_pkg_sender *sender =
		mdfld_dsi_get_pkg_sender(dsi_config);
	struct drm_device *dev = dsi_config->dev;
        extern struct drm_device *gpDrmDevice;
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *) gpDrmDevice->dev_private;
	int err;
	u32 cmd;
	PSB_DEBUG_ENTRY("lenovo\n");

	PSB_DEBUG_ENTRY("Turn on video mode otm1280a panel...\n");

	if (!sender) {
		DRM_ERROR("Failed to get DSI packet sender\n");
		return -EINVAL;
	}
	/*send TURN_ON packet*/
	//err = mdfld_dsi_send_dpi_spk_pkg_hs(sender,
	err = mdfld_dsi_send_dpi_spk_pkg_hs(sender,
				MDFLD_DSI_DPI_SPK_TURN_ON);
	if (err) {
		DRM_ERROR("Failed to send turn on packet\n");
		return err;
	}

	
	cmd = 0x11;
	REG_WRITE(sender->mipi_data_len_reg, 0);
	err = mdfld_dsi_send_gen_long_lp(sender, &cmd, 1, 0);
    	if(err)
    	{
		DRM_ERROR("Failed to send cmd 0x11\n");
        	return err;
   	}	
	msleep(120);

	cmd = 0x29;
	REG_WRITE(sender->mipi_data_len_reg, 0);
	err = mdfld_dsi_send_gen_long_lp(sender, &cmd, 1, 0);
    	if(err)
    	{	
		DRM_ERROR("Failed to send cmd 0x29\n");
        	return err;
    	}
	msleep(10);

    	{
        int d = 0;
        int ret = mdfld_dsi_read_gen_lp(sender, 0x0A, 0, 1, &d, 1); 
        
            printk("Read ic power status: 0x%x\n", d); 
       // pr_err("read ic power status, 0x%x\n", d);
    	}
	return 0;
}

static int mdfld_dsi_otm1280a_power_off(struct mdfld_dsi_config *dsi_config)
{
	struct mdfld_dsi_pkg_sender *sender =
		mdfld_dsi_get_pkg_sender(dsi_config);
	struct drm_device *dev = dsi_config->dev;
        extern struct drm_device *gpDrmDevice;
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *) gpDrmDevice->dev_private;
	int err;
	u32 cmd;
	PSB_DEBUG_ENTRY("lenovo\n");

	PSB_DEBUG_ENTRY("Turn off video mode otm1280a panel...\n");

	if (!sender) {
		DRM_ERROR("Failed to get DSI packet sender\n");
		return -EINVAL;
	}

	/*send SHUT_DOWN packet */
	err = mdfld_dsi_send_dpi_spk_pkg_hs(sender,
				MDFLD_DSI_DPI_SPK_SHUT_DOWN);
	if (err) {
		DRM_ERROR("Failed to send turn off packet\n");
		return err;
	}
	
	cmd = 0x28;
	REG_WRITE(sender->mipi_data_len_reg, 0);
	mdfld_dsi_send_gen_long_lp(sender, &cmd, 1, 0);
	
	cmd = 0x10;
	REG_WRITE(sender->mipi_data_len_reg, 0);
	//mdfld_dsi_send_gen_long_lp(sender, &cmd, 1, 0);
	//mdfld_dsi_send_mcs_long_lp(sender, &cmd, 1, 0);
	mdfld_dsi_send_mcs_long_hs(sender, &cmd, 1, 0); //ok, 5-6mA
	mdelay(120);

    	gpio_direction_input(GPIO_MIPI_PANEL_RESET);
	
	return 0;
}

static int mdfld_dsi_otm1280a_detect(struct mdfld_dsi_config *dsi_config, int pipe)
{
	int	status;
	dsi_config->lane_count = 4;
	dsi_config->lane_config = MDFLD_DSI_DATA_LANE_4_0;
	status =  MDFLD_DSI_PANEL_CONNECTED;
	dsi_config->dsi_hw_context.pll_bypass_mode = 0;
	PSB_DEBUG_ENTRY("lenovo\n");
	return status;
}

static int mdfld_dsi_otm1280a_panel_reset(struct mdfld_dsi_config *dsi_config, int boot)
{
	static u32 gpio_requested;
	u32 tmp, pipestat, dspcntr, dspstride, device_ready;
	int retry, err;
	uint32_t val, pipe0_enabled, pipe2_enabled;
	int ret = 0;
	struct mdfld_dsi_hw_registers *regs;
	struct drm_device *dev;
	struct mdfld_dsi_pkg_sender *sender
			= mdfld_dsi_get_pkg_sender(dsi_config);

	regs = &dsi_config->regs;
	dev = dsi_config->dev;
	PSB_DEBUG_ENTRY("lenovo\n");


	if (!gpio_requested) {
		ret = gpio_request(57, "gfx");
		if (ret) {
			DRM_ERROR("Failed to request gpio lenovo %d\n", 57);
			return ret;
		}
		ret = gpio_request(43, "gfx1");
		gpio_requested = 1;
	}

	gpio_direction_output(57, 1);
	gpio_direction_output(43, 1);
	/*reset high level width 1ms*/
    mdelay(1);
	gpio_direction_output(57, 0);
    mdelay(10); 

	gpio_direction_output(57, 1);
    mdelay(120); 

	return 0;
}

static int mdfld_dsi_otm1280a_set_brightness(struct mdfld_dsi_config *dsi_config,
					int level)
{
	struct mdfld_dsi_pkg_sender *sender =
		mdfld_dsi_get_pkg_sender(dsi_config);
	u8 backlight_value;

	PSB_DEBUG_ENTRY("Set brightness level %d...\n", level);

	if (!sender) {
		DRM_ERROR("Failed to get DSI packet sender\n");
		return -EINVAL;
	}

	backlight_value = ((level * 0xff) / 100) & 0xff;

	/* send backlight control packet*/
	/* backlight_value = 0x2c; */
/*	data = 0x00002c53;
	//mdfld_dsi_send_dcs(sender, 0x53, &backlight_value, 0, 0, 0);
	REG_WRITE(sender->mipi_data_len_reg, 1);
	mdfld_dsi_send_gen_long_lp(sender, &data, 1, 0);
	printk("turn on BCTRL/DD/BL\n");
	data = 0x0000d851;
	//mdfld_dsi_send_dcs(sender, 0x51, &backlight_value, 0, 0, 0);
	mdfld_dsi_send_gen_long_lp(sender, &data, 1, 0);
*/	/*
	mdfld_dsi_send_mcs_short_lp(sender,
				0x51,
				0xd8,
				1,
				MDFLD_DSI_SEND_PACKAGE);
	*/
	return 0;
}

/*otm1280a DPI encoder helper funcs*/
static const
struct drm_encoder_helper_funcs mdfld_otm1280a_dpi_encoder_helper_funcs = {
	.save = mdfld_dsi_dpi_save,
	.restore = mdfld_dsi_dpi_restore,
	.dpms = mdfld_dsi_dpi_dpms,
	.mode_fixup = mdfld_dsi_dpi_mode_fixup,
	.prepare = mdfld_dsi_dpi_prepare,
	.mode_set = mdfld_dsi_dpi_mode_set,
	.commit = mdfld_dsi_dpi_commit,
};

/*OTM1280A DPI encoder funcs*/
static const struct drm_encoder_funcs mdfld_otm1280a_dpi_encoder_funcs = {
	.destroy = drm_encoder_cleanup,
};

void otm1280a_vid_init(struct drm_device *dev, struct panel_funcs *p_funcs)
{
	if (!dev || !p_funcs) {
		DRM_ERROR("Invalid parameters\n");
		return;
	}

	PSB_DEBUG_ENTRY("\n");
	PSB_DEBUG_ENTRY("lenovo\n");
	p_funcs->encoder_funcs = &mdfld_otm1280a_dpi_encoder_funcs;
	p_funcs->encoder_helper_funcs = &mdfld_otm1280a_dpi_encoder_helper_funcs;
	p_funcs->get_config_mode = &otm1280a_vid_get_config_mode;
	p_funcs->update_fb = NULL;
	p_funcs->get_panel_info = otm1280a_vid_get_panel_info;
	p_funcs->reset = mdfld_dsi_otm1280a_panel_reset;
	p_funcs->drv_ic_init = mdfld_dsi_otm1280a_drv_ic_init;
	p_funcs->dsi_controller_init = mdfld_dsi_otm1280a_dsi_controller_init;
	p_funcs->detect = mdfld_dsi_otm1280a_detect;
	p_funcs->get_panel_power_state = mdfld_dsi_otm1280a_get_power_state;
	p_funcs->power_on = mdfld_dsi_otm1280a_power_on;
	p_funcs->power_off = mdfld_dsi_otm1280a_power_off;
	p_funcs->set_brightness = mdfld_dsi_otm1280a_set_brightness;
}

static int do_dump_dc_register(const char *val, struct kernel_param *kp)
{
    int value;
    int ret = param_set_int(val, kp);

    if(ret < 0)
    {
        printk(KERN_ERR"Errored set cabc");
        return -EINVAL;
    }
    value = *((int*)kp->arg);

    if(value)
    {
        extern struct drm_device *gpDrmDevice;
        int reg = value;
        struct drm_psb_private* dev_priv = gpDrmDevice->dev_private;
	    struct drm_device *dev = ((struct mdfld_dsi_config *)dev_priv->dsi_configs[0])->dev;

        for(; reg < value + 0x100; reg += 4)
            pr_err("0x%x  = 0x%x\n", reg, REG_READ(reg));    
    }

    return 0;
}

static int dump_dc_register;
module_param_call(dump_dc_register, do_dump_dc_register, param_get_int, &dump_dc_register, S_IRUSR | S_IWUSR);

#if 0
static int do_read_ic_register(const char *val, struct kernel_param *kp)
{
    int value;
    int ret = param_set_int(val, kp);

    if(ret < 0)
    {
        printk(KERN_ERR"Errored set cabc");
        return -EINVAL;
    }
    value = *((int*)kp->arg);

    if(value)
    {
        extern struct drm_device *gpDrmDevice;
        int data = 0;
        int reg = value;
        struct drm_psb_private* dev_priv = gpDrmDevice->dev_private;
	    struct mdfld_dsi_pkg_sender *sender =
		    mdfld_dsi_get_pkg_sender(dev_priv->dsi_configs[0]);
        struct mdfld_dsi_config *config = (struct mdfld_dsi_config *)dev_priv->dsi_configs[0];
	    struct drm_device *dev = ((struct mdfld_dsi_config *)dev_priv->dsi_configs[0])->dev;
        int ret = mdfld_dsi_read_gen_lp(sender, reg & 0xff, 0, 1, &data, 1); 
        if(ret)
            DRM_ERROR("Failed to read ic register status: 0x%x\n", data); 
        pr_err("read ic register , 0x%x\n", data);
    }

    return 0;
}
static int read_ic_register;
module_param_call(read_ic_register, do_read_ic_register, param_get_int, &read_ic_register, S_IRUSR | S_IWUSR);
#endif
