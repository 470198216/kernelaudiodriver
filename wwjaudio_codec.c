// SPDX-License-Identifier: GPL-2.0
/*
 * WWJAudio ALSA SoC dummy codec driver
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>

#define WWJAUDIO_NAME "wwjaudio-codec"

static struct snd_soc_component *wwjcodec_component;

static int wwjcodec_startup(struct snd_pcm_substream *substream,
			    struct snd_soc_dai *dai)
{
	pr_info("wwjaudio: %s - stream:%s, dai:%s\n", __func__,
		substream->stream == SNDRV_PCM_STREAM_PLAYBACK ? "PLAYBACK" : "CAPTURE",
		dai->name);
	return 0;
}

static void wwjcodec_shutdown(struct snd_pcm_substream *substream,
			      struct snd_soc_dai *dai)
{
	pr_info("wwjaudio: %s - stream:%s, dai:%s\n", __func__,
		substream->stream == SNDRV_PCM_STREAM_PLAYBACK ? "PLAYBACK" : "CAPTURE",
		dai->name);
}

static int wwjcodec_hw_params(struct snd_pcm_substream *substream,
			      struct snd_pcm_hw_params *hwparams,
			      struct snd_soc_dai *dai)
{
	pr_info("wwjaudio: %s - stream:%s, dai:%s\n", __func__,
		substream->stream == SNDRV_PCM_STREAM_PLAYBACK ? "PLAYBACK" : "CAPTURE",
		dai->name);
	pr_info("wwjaudio:   channels:%d, rate:%d, format:%d\n",
		params_channels(hwparams),
		params_rate(hwparams),
		params_format(hwparams));
	return 0;
}

static int wwjcodec_prepare(struct snd_pcm_substream *substream,
			    struct snd_soc_dai *dai)
{
	pr_info("wwjaudio: %s - stream:%s, dai:%s\n", __func__,
		substream->stream == SNDRV_PCM_STREAM_PLAYBACK ? "PLAYBACK" : "CAPTURE",
		dai->name);
	return 0;
}

static int wwjcodec_mute_stream(struct snd_soc_dai *dai, int mute, int stream)
{
	pr_info("wwjaudio: %s - mute:%d, stream:%s, dai:%s\n", __func__,
		mute,
		stream == SNDRV_PCM_STREAM_PLAYBACK ? "PLAYBACK" : "CAPTURE",
		dai->name);
	return 0;
}

static const struct snd_soc_dai_ops wwjcodec_dai_ops = {
	.startup = wwjcodec_startup,
	.shutdown = wwjcodec_shutdown,
	.hw_params = wwjcodec_hw_params,
	.prepare = wwjcodec_prepare,
	.mute_stream = wwjcodec_mute_stream,
};

static struct snd_soc_dai_driver wwjcodec_dai[] = {
	{
		.name = "wwj-i2s0",
		.id = 0,
		.playback = {
			.stream_name = "WWJ I2S 0 Playback",
			.rates = SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.rate_max = 48000,
			.rate_min = 48000,
			.channels_min = 1,
			.channels_max = 2,
			.sig_bits = 16,
		},
		.capture = {
			.stream_name = "WWJ I2S 0 Capture",
			.rates = SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.rate_max = 48000,
			.rate_min = 48000,
			.channels_min = 1,
			.channels_max = 2,
			.sig_bits = 16,
		},
		.ops = &wwjcodec_dai_ops,
	},
	{
		.name = "wwj-i2s1",
		.id = 1,
		.playback = {
			.stream_name = "WWJ I2S 1 Playback",
			.rates = SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.rate_max = 48000,
			.rate_min = 48000,
			.channels_min = 1,
			.channels_max = 2,
			.sig_bits = 16,
		},
		.capture = {
			.stream_name = "WWJ I2S 1 Capture",
			.rates = SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.rate_max = 48000,
			.rate_min = 48000,
			.channels_min = 1,
			.channels_max = 2,
			.sig_bits = 16,
		},
		.ops = &wwjcodec_dai_ops,
	},
};

static int wwjcodec_probe(struct snd_soc_component *comp)
{
	pr_info("wwjaudio: %s - component registered\n", __func__);
	wwjcodec_component = comp;
	return 0;
}

static int wwjcodec_write(struct snd_soc_component *comp, unsigned int reg,
			  unsigned int value)
{
	pr_info("wwjaudio: %s - reg:0x%x, value:0x%x\n", __func__, reg, value);
	return 0;
}

static unsigned int wwjcodec_read(struct snd_soc_component *comp,
				  unsigned int reg)
{
	pr_info("wwjaudio: %s - reg:0x%x\n", __func__, reg);
	return 0;
}

static const struct snd_soc_component_driver soc_codec_dev_wwjaudio = {
	.probe	= wwjcodec_probe,
	.read = wwjcodec_read,
	.write = wwjcodec_write,
	.name = WWJAUDIO_NAME,
};

#ifdef CONFIG_PM
static int wwjcodec_suspend(struct device *dev)
{
	pr_info("wwjaudio: %s\n", __func__);
	return 0;
}

static int wwjcodec_resume(struct device *dev)
{
	pr_info("wwjaudio: %s\n", __func__);
	return 0;
}

static const struct dev_pm_ops wwjcodec_pm_ops = {
	.suspend	= wwjcodec_suspend,
	.resume		= wwjcodec_resume,
};
#endif

static int wwjcodec_platform_probe(struct platform_device *pdev)
{
	pr_info("wwjaudio: %s - platform probe\n", __func__);
	return devm_snd_soc_register_component(&pdev->dev,
			&soc_codec_dev_wwjaudio,
			wwjcodec_dai, ARRAY_SIZE(wwjcodec_dai));
}

static const struct of_device_id wwjcodec_of_match[] = {
	{ .compatible = "wwj,wwjaudio-codec", },
	{},
};
MODULE_DEVICE_TABLE(of, wwjcodec_of_match);

static struct platform_driver wwjcodec_driver = {
	.driver = {
		.name = WWJAUDIO_NAME,
#ifdef CONFIG_PM
		.pm = &wwjcodec_pm_ops,
#endif
		.of_match_table = wwjcodec_of_match,
	},
	.probe = wwjcodec_platform_probe,
};

static int __init wwjcodec_init(void)
{
	pr_info("wwjaudio: %s - initializing\n", __func__);
	return platform_driver_register(&wwjcodec_driver);
}

static void __exit wwjcodec_exit(void)
{
	pr_info("wwjaudio: %s - exiting\n", __func__);
	platform_driver_unregister(&wwjcodec_driver);
}

module_init(wwjcodec_init);
module_exit(wwjcodec_exit);

MODULE_DESCRIPTION("WWJAudio ALSA SoC dummy codec driver");
MODULE_AUTHOR("WWJ");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" WWJAUDIO_NAME);