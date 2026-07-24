// SPDX-License-Identifier: GPL-2.0
/*
 * WWJAudio Simple ALSA Sound Card Driver
 * No Device Tree required - directly registers ALSA card
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#define WWJAUDIO_NAME "wwjaudio"
#define WWJAUDIO_CARD_NAME "WWJAudio"
#define WWJAUDIO_PCM_NAME "WWJAudio PCM"

static struct snd_card *wwj_card;

static int wwj_pcm_open(struct snd_pcm_substream *substream)
{
	pr_info("wwjaudio: %s - stream:%s\n", __func__,
		substream->stream == SNDRV_PCM_STREAM_PLAYBACK ? "PLAYBACK" : "CAPTURE");
	return 0;
}

static int wwj_pcm_close(struct snd_pcm_substream *substream)
{
	pr_info("wwjaudio: %s - stream:%s\n", __func__,
		substream->stream == SNDRV_PCM_STREAM_PLAYBACK ? "PLAYBACK" : "CAPTURE");
	return 0;
}

static int wwj_pcm_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *hwparams)
{
	pr_info("wwjaudio: %s - stream:%s\n", __func__,
		substream->stream == SNDRV_PCM_STREAM_PLAYBACK ? "PLAYBACK" : "CAPTURE");
	pr_info("wwjaudio:   channels:%d, rate:%d, format:%d\n",
		params_channels(hwparams),
		params_rate(hwparams),
		params_format(hwparams));

	snd_pcm_set_runtime_buffer(substream, &substream->runtime->dma_buffer);
	return 0;
}

static int wwj_pcm_hw_free(struct snd_pcm_substream *substream)
{
	pr_info("wwjaudio: %s - stream:%s\n", __func__,
		substream->stream == SNDRV_PCM_STREAM_PLAYBACK ? "PLAYBACK" : "CAPTURE");
	snd_pcm_set_runtime_buffer(substream, NULL);
	return 0;
}

static int wwj_pcm_prepare(struct snd_pcm_substream *substream)
{
	pr_info("wwjaudio: %s - stream:%s\n", __func__,
		substream->stream == SNDRV_PCM_STREAM_PLAYBACK ? "PLAYBACK" : "CAPTURE");
	return 0;
}

static int wwj_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	pr_info("wwjaudio: %s - stream:%s, cmd:%d\n", __func__,
		substream->stream == SNDRV_PCM_STREAM_PLAYBACK ? "PLAYBACK" : "CAPTURE",
		cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		pr_info("wwjaudio: START stream\n");
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		pr_info("wwjaudio: STOP stream\n");
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		pr_info("wwjaudio: PAUSE stream\n");
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		pr_info("wwjaudio: RESUME stream\n");
		break;
	default:
		pr_info("wwjaudio: unknown cmd:%d\n", cmd);
		break;
	}
	return 0;
}

static snd_pcm_uframes_t wwj_pcm_pointer(struct snd_pcm_substream *substream)
{
	return substream->runtime->control->appl_ptr;
}

static struct snd_pcm_ops wwj_pcm_ops = {
	.open = wwj_pcm_open,
	.close = wwj_pcm_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = wwj_pcm_hw_params,
	.hw_free = wwj_pcm_hw_free,
	.prepare = wwj_pcm_prepare,
	.trigger = wwj_pcm_trigger,
	.pointer = wwj_pcm_pointer,
};

static struct snd_pcm_hardware wwj_pcm_hw = {
	.info = SNDRV_PCM_INFO_MMAP |
		SNDRV_PCM_INFO_INTERLEAVED |
		SNDRV_PCM_INFO_BLOCK_TRANSFER |
		SNDRV_PCM_INFO_MMAP_VALID,

	.formats = SNDRV_PCM_FMTBIT_S16_LE,

	.rates = SNDRV_PCM_RATE_48000,
	.rate_min = 48000,
	.rate_max = 48000,

	.channels_min = 2,
	.channels_max = 2,

	.buffer_bytes_max = 1024 * 1024,
	.period_bytes_min = 64,
	.period_bytes_max = 64 * 1024,
	.periods_min = 2,
	.periods_max = 128,
};

static int wwj_audio_probe(struct platform_device *pdev)
{
	int ret;
	struct snd_pcm *pcm;

	pr_info("wwjaudio: %s - creating sound card\n", __func__);

	ret = snd_card_new(&pdev->dev, SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1,
			   THIS_MODULE, 0, &wwj_card);
	if (ret < 0) {
		pr_err("wwjaudio: failed to create sound card: %d\n", ret);
		return ret;
	}

	strcpy(wwj_card->driver, WWJAUDIO_NAME);
	strcpy(wwj_card->shortname, WWJAUDIO_CARD_NAME);
	strcpy(wwj_card->longname, WWJAUDIO_CARD_NAME " - Test Driver");

	ret = snd_pcm_new(wwj_card, WWJAUDIO_PCM_NAME, 0, 1, 1);
	if (ret < 0) {
		pr_err("wwjaudio: failed to create PCM: %d\n", ret);
		goto err_card_free;
	}

	pcm = wwj_card->pcm[0];
	pcm->private_data = NULL;
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK, &wwj_pcm_ops);
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_CAPTURE, &wwj_pcm_ops);
	snd_pcm_lib_preallocate_pages_for_all(pcm, SNDRV_DMA_TYPE_CONTINUOUS,
					      snd_dma_continuous_data(GFP_KERNEL),
					      64 * 1024, 1024 * 1024);

	ret = snd_pcm_hw_constraint_integer(pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream,
					    SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0) {
		pr_err("wwjaudio: failed to set constraint: %d\n", ret);
		goto err_card_free;
	}
	ret = snd_pcm_hw_constraint_integer(pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream,
					    SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0) {
		pr_err("wwjaudio: failed to set constraint: %d\n", ret);
		goto err_card_free;
	}

	pcm->info_flags = 0;

	ret = snd_card_register(wwj_card);
	if (ret < 0) {
		pr_err("wwjaudio: failed to register sound card: %d\n", ret);
		goto err_card_free;
	}

	platform_set_drvdata(pdev, wwj_card);
	pr_info("wwjaudio: sound card registered successfully\n");
	return 0;

err_card_free:
	snd_card_free(wwj_card);
	return ret;
}

static int wwj_audio_remove(struct platform_device *pdev)
{
	pr_info("wwjaudio: %s - removing sound card\n", __func__);
	snd_card_free(wwj_card);
	return 0;
}

static struct platform_driver wwj_audio_driver = {
	.driver = {
		.name = WWJAUDIO_NAME,
	},
	.probe = wwj_audio_probe,
	.remove = wwj_audio_remove,
};

static struct platform_device *wwj_audio_device;

static int __init wwj_audio_init(void)
{
	int ret;

	pr_info("wwjaudio: %s - initializing\n", __func__);

	ret = platform_driver_register(&wwj_audio_driver);
	if (ret) {
		pr_err("wwjaudio: failed to register driver: %d\n", ret);
		return ret;
	}

	wwj_audio_device = platform_device_alloc(WWJAUDIO_NAME, -1);
	if (!wwj_audio_device) {
		pr_err("wwjaudio: failed to allocate platform device\n");
		ret = -ENOMEM;
		goto err_driver_unregister;
	}

	ret = platform_device_add(wwj_audio_device);
	if (ret) {
		pr_err("wwjaudio: failed to add platform device: %d\n", ret);
		goto err_device_put;
	}

	pr_info("wwjaudio: initialized successfully\n");
	return 0;

err_device_put:
	platform_device_put(wwj_audio_device);
err_driver_unregister:
	platform_driver_unregister(&wwj_audio_driver);
	return ret;
}

static void __exit wwj_audio_exit(void)
{
	pr_info("wwjaudio: %s - exiting\n", __func__);
	platform_device_unregister(wwj_audio_device);
	platform_driver_unregister(&wwj_audio_driver);
}

module_init(wwj_audio_init);
module_exit(wwj_audio_exit);

MODULE_DESCRIPTION("WWJAudio Simple ALSA Sound Card Driver");
MODULE_AUTHOR("WWJ");
MODULE_LICENSE("GPL v2");