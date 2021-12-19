/*
 *  clv_machine.c - ASoc Machine driver for Intel Cloverview MID platform
 *
 *  Copyright (C) 2011-12 Intel Corp
 *  Author: KP Jeeja<jeeja.kp@intel.com>
 *  Author: Vaibhav Agarwal <vaibhav.agarwal@intel.com>
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */


#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/async.h>
#include <linux/delay.h>
#include <linux/ipc_device.h>
#include <linux/wakelock.h>
#include <linux/gpio.h>
#include <asm/intel_scu_ipc.h>
#include <asm/intel_scu_ipcutil.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <linux/switch.h>
#include <linux/input.h>
#include "../codecs/wm5102.h"
#include "../codecs/arizona.h"
#include <linux/mfd/arizona/core.h>
#include <linux/mfd/arizona/registers.h>

#define K5_SYSCLK_RATE (2*24576000)

#define DAI_DSP_CODEC 1


struct snd_soc_jack	hs_jack;
struct input_dev *inp_dev;
static int aif_incall; /* storage to enusure we only sync on AIF2 when in a call  */
static bool card_sysclk = false;
static bool vibrator_sysclk = false;

void resume_set_sysclk(void)
{
    if (!aif_incall && !card_sysclk) {
        printk("enter %s, arizona: set sync to AIF1BCLK\n", __func__);
        arizona_set_fll(&wm5102_p->fll[0], ARIZONA_FLL_SRC_AIF1BCLK, 25 * 2 * 48000, K5_SYSCLK_RATE);
        vibrator_sysclk = true;
//    } else {
//        printk("%s, arizona: set sync to AIF2BCLK\n", __func__);
//        arizona_set_fll(&wm5102_p->fll[0], ARIZONA_FLL_SRC_AIF2BCLK, 16 * 2 * 48000, K5_SYSCLK_RATE);
    } else
        printk("enter %s, others have enabled sysclk!!\n", __func__);

    return;
}
EXPORT_SYMBOL_GPL(resume_set_sysclk);

void suspend_set_sysclk(void)
{
    if (card_sysclk)
        printk("enter %s, sysclk used by others!!\n", __func__);
    else {
        printk("enter %s, disable clk!!\n", __func__);
        arizona_set_fll(&wm5102_p->fll[0], 0, 0, 0);
    }
    vibrator_sysclk = false;

    return;
}
EXPORT_SYMBOL_GPL(suspend_set_sysclk);

static int clv_asp_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	unsigned int fmt;
	int ret;

	/* ARIZONA  Slave Mode`*/
	fmt =   SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF
					| SND_SOC_DAIFMT_CBS_CFS;

	/* Set codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, fmt);

	if (ret < 0) {
		pr_err("can't set codec DAI configuration %d\n", ret);
		return ret;
	}
	
	return 0;
}

static int clv_vsp_startup(struct snd_pcm_substream *substream)
{
	aif_incall++;
	return 0;
}

static void clv_vsp_shutdown(struct snd_pcm_substream *substream)
{
	if (aif_incall >= 1)
		aif_incall--;
	else 
		pr_err("Non symmetric settings of VSP DAI power up and down");
}



static int clv_vsp_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	unsigned int fmt;
	int ret , clk_source;

	pr_debug("Slave Mode selected\n");
	/* ARIZONA  Slave Mode`*/
	fmt =   SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF
		| SND_SOC_DAIFMT_CBS_CFS;
	clk_source = SND_SOC_CLOCK_IN;

	/* Set codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, fmt);
	if (ret < 0) {
		pr_err("can't set codec DAI configuration %d\n", ret);
		return ret;
	}

	return 0;
}

static int clv_bsp_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	unsigned int fmt;
	int ret;

	/* ARIZONA  Slave Mode`*/
	fmt =   SND_SOC_DAIFMT_DSP_A | SND_SOC_DAIFMT_IB_NF
					| SND_SOC_DAIFMT_CBM_CFM;

	/* Set codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, fmt);

	if (ret < 0) {
		pr_err("can't set codec DAI configuration %d\n", ret);
		return ret;
	}

	return 0;
}

struct clv_mc_private {
	struct ipc_device *socdev;
	void __iomem *int_base;
	struct snd_soc_codec *codec;
	/* Jack related */
	struct delayed_work jack_work;
	struct snd_soc_jack clv_jack;
	atomic_t bpirq_flag;
#ifdef CONFIG_HAS_WAKELOCK
	struct wake_lock *jack_wake_lock;
#endif
};


/* Board specific codec bias level control */
static int clv_set_bias_level(struct snd_soc_card *card,
		struct snd_soc_dapm_context *dapm,
		enum snd_soc_bias_level level)
{
	int ret;

	struct snd_soc_dai *codec_dai = card->rtd[DAI_DSP_CODEC].codec_dai;
	struct snd_soc_codec *codec = codec_dai->codec;

	if (dapm->dev != codec->dev)
        return 0;

	switch (level) {
	case SND_SOC_BIAS_ON:
	case SND_SOC_BIAS_PREPARE:
	case SND_SOC_BIAS_STANDBY:
        if (card->dapm.bias_level == SND_SOC_BIAS_OFF) {
            card_sysclk = true;
            if(vibrator_sysclk)
                snd_soc_codec_set_pll(codec, WM5102_FLL1,0,0,0);
            if (!aif_incall) {
                printk("%s, arizona: set sync to AIF1BCLK\n", __func__);
                ret = snd_soc_codec_set_pll(codec, WM5102_FLL1,
                        ARIZONA_FLL_SRC_AIF1BCLK,
                        25 * 2 * 48000,
                        K5_SYSCLK_RATE);
            } else {
                printk("%s, arizona: set sync to AIF2BCLK\n", __func__);
                ret = snd_soc_codec_set_pll(codec, WM5102_FLL1,
                        ARIZONA_FLL_SRC_AIF2BCLK,
                        16 * 2 * 48000,
                        K5_SYSCLK_RATE);
            }
            if (ret < 0) {
                pr_err("can't set FLL %d\n", ret);
                return ret;
            }

			card->dapm.bias_level = level;
		}
		break;
	case SND_SOC_BIAS_OFF:
		/* OSC clk will be turned OFF after processing
		 * codec->dapm.bias_level = SND_SOC_BIAS_OFF.
		 */
		break;
	default:
		pr_err("%s: Invalid bias level=%d\n", __func__, level);
		return -EINVAL;
		break;
	}
	pr_debug("card(%s)->bias_level %u\n", card->name,
			card->dapm.bias_level);

	return 0;
}

static int clv_set_bias_level_post(struct snd_soc_card *card,
		struct snd_soc_dapm_context *dapm,
		enum snd_soc_bias_level level)
{
	int ret;

	struct snd_soc_dai *codec_dai = card->rtd[DAI_DSP_CODEC].codec_dai;
	struct snd_soc_codec *codec = codec_dai->codec;

	if (dapm->dev != codec->dev)
        return 0;

 	switch (level) {
	case SND_SOC_BIAS_ON:
	case SND_SOC_BIAS_PREPARE:
	case SND_SOC_BIAS_STANDBY:
		/* Processing already done during set_bias_level()
		 * callback. No action required here.
		 */
		break;
	case SND_SOC_BIAS_OFF:
		if (codec->dapm.bias_level != SND_SOC_BIAS_OFF) {
            printk("enter %s, not SND_SOC_BIAS_OFF\n", __func__);
			break;
        }

        ret = snd_soc_codec_set_pll(codec, WM5102_FLL1,0,0,0); 
        if (ret < 0) {
            pr_err("can't turn off FLL %d\n", ret);
            return ret;
        }
        card_sysclk = false;
        printk("enter %s, disable clk!!\n", __func__);

		card->dapm.bias_level = level;
		break;
	default:
		pr_err("%s: Invalid bias level=%d\n", __func__, level);
		return -EINVAL;
		break;
	}
	pr_debug("%s:card(%s)->bias_level %u\n", __func__, card->name,
			card->dapm.bias_level);

	return 0;
}

static int clv_init(struct snd_soc_pcm_runtime *runtime)
{
	struct snd_soc_codec *codec = runtime->codec;
//	struct snd_soc_dapm_context *dapm = &codec->dapm;
	int ret;
//	struct snd_soc_card *card = runtime->card;

    /* Headset and button jack detection */
    snd_soc_jack_new(codec, "Headset", SND_JACK_HEADSET, &hs_jack);

    inp_dev	= input_allocate_device();
    input_set_capability(inp_dev, EV_SW, SW_HEADPHONE_INSERT);
    input_set_capability(inp_dev, EV_SW, SW_MICROPHONE_INSERT);
    input_set_capability(inp_dev, EV_KEY, KEY_MEDIA);
    inp_dev->name = "Headset Hook Key";

    ret	= input_register_device(inp_dev);
    if (ret != 0) {
        printk("enter: %s, ret = %d: Error in input_register_device\n", __func__, ret);
    }

    arizona_core_interrupt_regist();
    switch_arizona_interrupt_regist();
    wm5102_fll_interrupt_regist();

	return 0;
}

static unsigned int rates_48000[] = {
	8000,
	48000,
};

static struct snd_pcm_hw_constraint_list constraints_48000 = {
	.count	= ARRAY_SIZE(rates_48000),
	.list	= rates_48000,
};

static int clv_startup(struct snd_pcm_substream *substream)
{
	pr_debug("%s - applying rate constraint\n", __func__);
	snd_pcm_hw_constraint_list(substream->runtime, 0,
				   SNDRV_PCM_HW_PARAM_RATE,
				   &constraints_48000);
	return 0;
}

static struct snd_soc_dapm_widget medfield_wm5102_widgets[] = {
	SND_SOC_DAPM_SPK("Int Spk", NULL ), // HLL medfield_wm5102_event_int_spk),
	SND_SOC_DAPM_HP("Earphone", NULL),
	SND_SOC_DAPM_HP("Headphone Jack", NULL),
	SND_SOC_DAPM_LINE("Line Out", NULL),
	SND_SOC_DAPM_MIC("Main MIC", NULL),
	SND_SOC_DAPM_MIC("Secondary MIC", NULL),
	SND_SOC_DAPM_MIC("HSMIC", NULL),
	SND_SOC_DAPM_LINE("FM_OUT1", NULL),
	SND_SOC_DAPM_LINE("FM_OUT2", NULL),
  
};

static struct snd_soc_dapm_route medfield_wm5102_audio_paths[] = {
    {"Sub LINN", NULL, "EPOUTN"},
    {"Sub LINP", NULL, "EPOUTP"}, 

    {"Earphone", NULL, "Sub SPKN"},
    {"Earphone", NULL, "Sub SPKP"}, 

	{"Headphone Jack", NULL, "HPOUT1R"},
	{"Headphone Jack", NULL, "HPOUT1L"},

#if 0
	{"Int Spk", NULL, "SPKOUTLP"},
	{"Int Spk", NULL, "SPKOUTLN"},
#else
	{"Int Spk", NULL, "HPOUT2R"},
	{"Int Spk", NULL, "HPOUT2L"},
#endif

	{"HSMIC", NULL, "MICBIAS1"},
	{"IN1L", NULL, "HSMIC"},

	{"Main MIC", NULL, "MICBIAS2"},
	{"IN3L", NULL, "Main MIC"},

	{"Secondary MIC", NULL, "MICBIAS2"},
	{"IN3R", NULL, "Secondary MIC"},

	{"IN2L", NULL, "FM_OUT1"},
	{"IN2R", NULL, "FM_OUT2"},

};

static const struct snd_kcontrol_new medfield_wm5102_controls[] = {
	SOC_DAPM_PIN_SWITCH("Int Spk"),
	SOC_DAPM_PIN_SWITCH("Earphone"),
	SOC_DAPM_PIN_SWITCH("Headphone Jack"),
	SOC_DAPM_PIN_SWITCH("LineOut"),
	SOC_DAPM_PIN_SWITCH("HSMIC"),
	SOC_DAPM_PIN_SWITCH("Main MIC"),
	SOC_DAPM_PIN_SWITCH("Secondary MIC"),
	SOC_DAPM_PIN_SWITCH("LineIn"),
};

static struct snd_soc_ops clv_asp_ops = {
	.startup = clv_startup,
	.hw_params = clv_asp_hw_params,
};

static struct snd_soc_ops clv_vsp_ops = {
	.hw_params = clv_vsp_hw_params,
	.startup = clv_vsp_startup,
	.shutdown = clv_vsp_shutdown,
};

static struct snd_soc_ops clv_bsp_ops = {
	//.startup = clv_bsp_startup,
	.hw_params = clv_bsp_hw_params,
};

struct snd_soc_dai_link clv_msic_dailink[] = {
	{
		.name = "Cloverview ASP",
		.stream_name = "Audio",
		.cpu_dai_name = "Headset-cpu-dai",
		.codec_dai_name = "wm5102-aif1",
		.codec_name = "wm5102-codec",
		.platform_name = "sst-platform",
		.init = clv_init,
		.ignore_suspend = 1,
		.ops = &clv_asp_ops,
	},

	{
		.name = "Cloverview VSP",
		.stream_name = "Voice",
		.cpu_dai_name = "Voice-cpu-dai",
		.codec_dai_name = "wm5102-aif2",
		.codec_name = "wm5102-codec",
		.platform_name = "sst-platform",
		.init = NULL,
		.ignore_suspend = 1,
		.ops = &clv_vsp_ops,
	},

	{
		.name = "SoC BT",
		.stream_name = "BT_Voice",
		.cpu_dai_name = "BT Virtual-cpu-dai",
		.codec_dai_name = "wm5102-aif3",
		.codec_name = "wm5102-codec",
		.platform_name = "sst-platform",
		.init = NULL,
		.ignore_suspend = 1,
		.ops = &clv_bsp_ops,
	},

    {
        .name = "Cloverview Comp ASP",
        .stream_name = "Compress-Audio",
        .cpu_dai_name = "Compress-cpu-dai",
        .codec_dai_name = "wm5102-aif1",
        .codec_name = "wm5102-codec",
        .platform_name = "sst-platform",
        .init = NULL,
        .ignore_suspend = 1,
        .ops = &clv_asp_ops,
    },
};

#ifdef CONFIG_PM

static int snd_clv_suspend(struct device *dev)
{
	pr_debug("K5 In %s device name\n", __func__);
	snd_soc_suspend(dev);
	return 0;
}
static int snd_clv_resume(struct device *dev)
{
	pr_debug("K5 In %s\n", __func__);
	snd_soc_resume(dev);
	return 0;
}

static int snd_clv_poweroff(struct device *dev)
{
	pr_debug("K5 In %s\n", __func__);
	snd_soc_poweroff(dev);
	return 0;
}

#else
#define snd_clv_suspend NULL
#define snd_clv_resume NULL
#define snd_clv_poweroff NULL
#endif

static int medfield_wm5102_late_probe(struct snd_soc_card *card)
{
	struct snd_soc_codec *codec = card->rtd[0].codec;
//	struct snd_soc_dai *codec_dai = card->rtd[DAI_DSP_CODEC].codec_dai;
	int ret;

	/* We should start and stop the FLL in set_bias_level, can start now
	 * as we don't need the sync clock to start when we have 32kHz ref clock.
	 */
    aif_incall = 0;
    printk("%s, arizona: set sync to AIF1BCLK\n", __func__);
    card_sysclk = true;
    if(vibrator_sysclk)
        snd_soc_codec_set_pll(codec, WM5102_FLL1,0,0,0);
    ret = snd_soc_codec_set_pll(codec, WM5102_FLL1,
            ARIZONA_FLL_SRC_AIF1BCLK,
            25 * 2 * 48000,
            K5_SYSCLK_RATE);
    if (ret < 0) {
        pr_err("can't set FLL in late probe%d\n", ret);
        return ret;
    }

	/* Always derive SYSCLK from the FLL, we will ensure FLL is enabled in
	 * set_bias_level()
	 */
	ret = snd_soc_codec_set_sysclk(codec, ARIZONA_CLK_SYSCLK, ARIZONA_CLK_SRC_FLL1,
				     K5_SYSCLK_RATE, SND_SOC_CLOCK_IN);
	if (ret < 0) {
		pr_err("can't set codec clock %d\n", ret);
		return ret;
	}
	
	return 0;
}

static struct snd_soc_codec_conf clv_codec_conf[] = {
       {
       .dev_name = "wm2000.1-003a",
       .name_prefix = "Sub",
       },
};

static struct snd_soc_aux_dev clv_aux_devs[] = {
       {
       .name = "wm2000",
       .codec_name = "wm2000.1-003a",
       },
};

/* SoC card */
static struct snd_soc_card snd_soc_card_clv = {
	.name = "K5",
	.dai_link = clv_msic_dailink,
	.num_links = ARRAY_SIZE(clv_msic_dailink),
	.set_bias_level = clv_set_bias_level,
	.set_bias_level_post = clv_set_bias_level_post,
	.aux_dev = clv_aux_devs,
	.num_aux_devs = ARRAY_SIZE(clv_aux_devs),
	.codec_conf = clv_codec_conf,
	.num_configs = ARRAY_SIZE(clv_codec_conf),

	.controls = medfield_wm5102_controls,
	.num_controls = ARRAY_SIZE(medfield_wm5102_controls),
	.dapm_widgets = medfield_wm5102_widgets,
	.num_dapm_widgets = ARRAY_SIZE(medfield_wm5102_widgets),
	.dapm_routes = medfield_wm5102_audio_paths,
	.num_dapm_routes = ARRAY_SIZE(medfield_wm5102_audio_paths),

	.late_probe = medfield_wm5102_late_probe,
};

static int snd_clv_mc_probe(struct ipc_device *ipcdev)
{
	int ret_val = 0;
	struct clv_mc_private *ctx;

	pr_debug("In %s\n", __func__);
	ctx = kzalloc(sizeof(*ctx), GFP_ATOMIC);
	if (!ctx) {
		pr_err("allocation failed\n");
		return -ENOMEM;
	}

#ifdef CONFIG_HAS_WAKELOCK
	ctx->jack_wake_lock =
		kzalloc(sizeof(*(ctx->jack_wake_lock)), GFP_ATOMIC);
	if (!ctx->jack_wake_lock) {
		pr_err("allocation failed for wake_lock\n");
		kfree(ctx);
		return -ENOMEM;
	}
	wake_lock_init(ctx->jack_wake_lock, WAKE_LOCK_SUSPEND,
			"jack_detect");
#endif

	/* register the soc card */
	snd_soc_card_clv.dev = &ipcdev->dev;
	snd_soc_card_set_drvdata(&snd_soc_card_clv, ctx);
	ret_val = snd_soc_register_card(&snd_soc_card_clv);
	if (ret_val) {
		pr_err("snd_soc_register_card failed %d\n", ret_val);
		goto unalloc;
	}
	ipc_set_drvdata(ipcdev, &snd_soc_card_clv);

	pr_debug("successfully exited probe\n");
	return ret_val;

unalloc:
#ifdef CONFIG_HAS_WAKELOCK
	if (wake_lock_active(ctx->jack_wake_lock))
		wake_unlock(ctx->jack_wake_lock);
	wake_lock_destroy(ctx->jack_wake_lock);
	kfree(ctx->jack_wake_lock);
#endif
	kfree(ctx);
	return ret_val;
}

static int __devexit snd_clv_mc_remove(struct ipc_device *ipcdev)
{
	struct snd_soc_card *soc_card = ipc_get_drvdata(ipcdev);
	struct clv_mc_private *ctx = snd_soc_card_get_drvdata(soc_card);

	pr_debug("In %s\n", __func__);
	cancel_delayed_work_sync(&ctx->jack_work);
#ifdef CONFIG_HAS_WAKELOCK
	if (wake_lock_active(ctx->jack_wake_lock))
		wake_unlock(ctx->jack_wake_lock);
	wake_lock_destroy(ctx->jack_wake_lock);
	kfree(ctx->jack_wake_lock);
#endif
//	snd_soc_jack_free_gpios(&ctx->clv_jack, 2, hs_gpio);
	kfree(ctx);
	snd_soc_card_set_drvdata(soc_card, NULL);
	snd_soc_unregister_card(soc_card);
	ipc_set_drvdata(ipcdev, NULL);

	return 0;
}
const struct dev_pm_ops snd_clv_mc_pm_ops = {
	.suspend = snd_clv_suspend,
	.resume = snd_clv_resume,
	.poweroff = snd_clv_poweroff,
};

static struct ipc_driver snd_clv_mc_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "ctp_audio",
		.pm   = &snd_clv_mc_pm_ops,
	},
	.probe = snd_clv_mc_probe,
	.remove = __devexit_p(snd_clv_mc_remove),
};

static int __init snd_clv_driver_init(void)
{
	pr_info("In %s\n", __func__);
	return ipc_driver_register(&snd_clv_mc_driver);
}
late_initcall(snd_clv_driver_init);

static void __exit snd_clv_driver_exit(void)
{
	pr_debug("In %s\n", __func__);
	ipc_driver_unregister(&snd_clv_mc_driver);
}

module_exit(snd_clv_driver_exit);

MODULE_DESCRIPTION("ASoC K5 machine driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("ipc:k5-audio");
