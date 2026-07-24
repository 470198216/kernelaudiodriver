// SPDX-License-Identifier: GPL-2.0
/*
 * WWJAudio Simple ALSA Sound Card Driver
 * Based on sound/drivers/dummy.c
 */
#include <linux/init.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/wait.h>
#include <linux/hrtimer.h>
#include <linux/math64.h>
#include <linux/module.h>
#include <sound/core.h>
#include <sound/control.h>
#include <sound/tlv.h>
#include <sound/pcm.h>
#include <sound/rawmidi.h>
#include <sound/info.h>
#include <sound/initval.h>

MODULE_AUTHOR("WWJ");
MODULE_DESCRIPTION("WWJAudio Simple ALSA Sound Card Driver");
MODULE_LICENSE("GPL");

#define WWJAUDIO_NAME "wwjaudio"
#define WWJAUDIO_CARD_NAME "WWJAudio"

static struct platform_device *wwj_audio_device;

#define MAX_BUFFER_SIZE		(64*1024)
#define MIN_PERIOD_SIZE		64
#define MAX_PERIOD_SIZE		MAX_BUFFER_SIZE
#define USE_FORMATS 		(SNDRV_PCM_FMTBIT_U8 | SNDRV_PCM_FMTBIT_S16_LE)
#define USE_RATE		SNDRV_PCM_RATE_CONTINUOUS | SNDRV_PCM_RATE_8000_48000
#define USE_RATE_MIN		8000
#define USE_RATE_MAX		48000
#define USE_CHANNELS_MIN 	1
#define USE_CHANNELS_MAX 	2
#define USE_PERIODS_MIN 	2
#define USE_PERIODS_MAX 	1024

static bool fake_buffer = 1;

struct wwj_audio {
	struct snd_card *card;
	struct snd_pcm *pcm;
	struct snd_pcm_hardware pcm_hw;
};

struct wwj_timer_pcm {
	spinlock_t lock;
	struct timer_list timer;
	unsigned long base_time;
	unsigned int frac_pos;
	unsigned int frac_period_rest;
	unsigned int frac_buffer_size;
	unsigned int frac_period_size;
	unsigned int rate;
	int elapsed;
	struct snd_pcm_substream *substream;
};

static void wwj_timer_rearm(struct wwj_timer_pcm *wpcm)
{
	mod_timer(&wpcm->timer, jiffies +
		DIV_ROUND_UP(wpcm->frac_period_rest, wpcm->rate));
}

static void wwj_timer_update(struct wwj_timer_pcm *wpcm)
{
	unsigned long delta;

	delta = jiffies - wpcm->base_time;
	if (!delta)
		return;
	wpcm->base_time += delta;
	delta *= wpcm->rate;
	wpcm->frac_pos += delta;
	while (wpcm->frac_pos >= wpcm->frac_buffer_size)
		wpcm->frac_pos -= wpcm->frac_buffer_size;
	while (wpcm->frac_period_rest <= delta) {
		wpcm->elapsed++;
		wpcm->frac_period_rest += wpcm->frac_period_size;
	}
	wpcm->frac_period_rest -= delta;
}

static void wwj_timer_callback(struct timer_list *t)
{
	struct wwj_timer_pcm *wpcm = from_timer(wpcm, t, timer);
	unsigned long flags;
	int elapsed = 0;

	spin_lock_irqsave(&wpcm->lock, flags);
	wwj_timer_update(wpcm);
	wwj_timer_rearm(wpcm);
	elapsed = wpcm->elapsed;
	wpcm->elapsed = 0;
	spin_unlock_irqrestore(&wpcm->lock, flags);
	if (elapsed)
		snd_pcm_period_elapsed(wpcm->substream);
}

static int wwj_timer_start(struct snd_pcm_substream *substream)
{
	struct wwj_timer_pcm *wpcm = substream->runtime->private_data;
	spin_lock(&wpcm->lock);
	wpcm->base_time = jiffies;
	wwj_timer_rearm(wpcm);
	spin_unlock(&wpcm->lock);
	return 0;
}

static int wwj_timer_stop(struct snd_pcm_substream *substream)
{
	struct wwj_timer_pcm *wpcm = substream->runtime->private_data;
	spin_lock(&wpcm->lock);
	del_timer(&wpcm->timer);
	spin_unlock(&wpcm->lock);
	return 0;
}

static int wwj_timer_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct wwj_timer_pcm *wpcm = runtime->private_data;

	wpcm->frac_pos = 0;
	wpcm->rate = runtime->rate;
	wpcm->frac_buffer_size = runtime->buffer_size * HZ;
	wpcm->frac_period_size = runtime->period_size * HZ;
	wpcm->frac_period_rest = wpcm->frac_period_size;
	wpcm->elapsed = 0;

	return 0;
}

static snd_pcm_uframes_t wwj_timer_pointer(struct snd_pcm_substream *substream)
{
	struct wwj_timer_pcm *wpcm = substream->runtime->private_data;
	snd_pcm_uframes_t pos;

	spin_lock(&wpcm->lock);
	wwj_timer_update(wpcm);
	pos = wpcm->frac_pos / HZ;
	spin_unlock(&wpcm->lock);
	return pos;
}

static int wwj_timer_create(struct snd_pcm_substream *substream)
{
	struct wwj_timer_pcm *wpcm;

	wpcm = kzalloc(sizeof(*wpcm), GFP_KERNEL);
	if (!wpcm)
		return -ENOMEM;
	substream->runtime->private_data = wpcm;
	timer_setup(&wpcm->timer, wwj_timer_callback, 0);
	spin_lock_init(&wpcm->lock);
	wpcm->substream = substream;
	return 0;
}

static void wwj_timer_free(struct snd_pcm_substream *substream)
{
	kfree(substream->runtime->private_data);
}

static const struct snd_pcm_hardware wwj_pcm_hw = {
	.info =			(SNDRV_PCM_INFO_MMAP |
				 SNDRV_PCM_INFO_INTERLEAVED |
				 SNDRV_PCM_INFO_RESUME |
				 SNDRV_PCM_INFO_MMAP_VALID),
	.formats =		USE_FORMATS,
	.rates =		USE_RATE,
	.rate_min =		USE_RATE_MIN,
	.rate_max =		USE_RATE_MAX,
	.channels_min =		USE_CHANNELS_MIN,
	.channels_max =		USE_CHANNELS_MAX,
	.buffer_bytes_max =	MAX_BUFFER_SIZE,
	.period_bytes_min =	MIN_PERIOD_SIZE,
	.period_bytes_max =	MAX_PERIOD_SIZE,
	.periods_min =		USE_PERIODS_MIN,
	.periods_max =		USE_PERIODS_MAX,
	.fifo_size =		0,
};

static int wwj_pcm_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *hw_params)
{
	pr_info("wwjaudio: %s - channels:%d, rate:%d\n", __func__,
		params_channels(hw_params), params_rate(hw_params));

	if (fake_buffer) {
		substream->runtime->dma_bytes = params_buffer_bytes(hw_params);
		return 0;
	}
	return 0;
}

static int wwj_pcm_open(struct snd_pcm_substream *substream)
{
	struct wwj_audio *wwj = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	int err;

	pr_info("wwjaudio: %s - stream:%s\n", __func__,
		substream->stream == SNDRV_PCM_STREAM_PLAYBACK ? "PLAYBACK" : "CAPTURE");

	err = wwj_timer_create(substream);
	if (err < 0)
		return err;

	runtime->hw = wwj->pcm_hw;

	return 0;
}

static int wwj_pcm_close(struct snd_pcm_substream *substream)
{
	pr_info("wwjaudio: %s - stream:%s\n", __func__,
		substream->stream == SNDRV_PCM_STREAM_PLAYBACK ? "PLAYBACK" : "CAPTURE");
	wwj_timer_free(substream);
	return 0;
}

static int wwj_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	pr_info("wwjaudio: %s - stream:%s, cmd:%d\n", __func__,
		substream->stream == SNDRV_PCM_STREAM_PLAYBACK ? "PLAYBACK" : "CAPTURE",
		cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		return wwj_timer_start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		return wwj_timer_stop(substream);
	}
	return -EINVAL;
}

static int wwj_pcm_prepare(struct snd_pcm_substream *substream)
{
	pr_info("wwjaudio: %s - stream:%s\n", __func__,
		substream->stream == SNDRV_PCM_STREAM_PLAYBACK ? "PLAYBACK" : "CAPTURE");
	return wwj_timer_prepare(substream);
}

static snd_pcm_uframes_t wwj_pcm_pointer(struct snd_pcm_substream *substream)
{
	return wwj_timer_pointer(substream);
}

static int wwj_pcm_copy(struct snd_pcm_substream *substream,
			int channel, unsigned long pos,
			struct iov_iter *iter, unsigned long bytes)
{
	return 0;
}

static int wwj_pcm_silence(struct snd_pcm_substream *substream,
			   int channel, unsigned long pos,
			   unsigned long bytes)
{
	return 0;
}

static void *wwj_page[2];

static struct page *wwj_pcm_get_page(struct snd_pcm_substream *substream,
				     unsigned long offset)
{
	return virt_to_page(wwj_page[substream->stream]);
}

static const struct snd_pcm_ops wwj_pcm_ops = {
	.open =		wwj_pcm_open,
	.close =	wwj_pcm_close,
	.hw_params =	wwj_pcm_hw_params,
	.prepare =	wwj_pcm_prepare,
	.trigger =	wwj_pcm_trigger,
	.pointer =	wwj_pcm_pointer,
};

static const struct snd_pcm_ops wwj_pcm_ops_no_buf = {
	.open =		wwj_pcm_open,
	.close =	wwj_pcm_close,
	.hw_params =	wwj_pcm_hw_params,
	.prepare =	wwj_pcm_prepare,
	.trigger =	wwj_pcm_trigger,
	.pointer =	wwj_pcm_pointer,
	.copy =		wwj_pcm_copy,
	.fill_silence =	wwj_pcm_silence,
	.page =		wwj_pcm_get_page,
};

static int wwj_pcm_new(struct wwj_audio *wwj, int device, int substreams)
{
	struct snd_pcm *pcm;
	const struct snd_pcm_ops *ops;
	int err;

	err = snd_pcm_new(wwj->card, WWJAUDIO_CARD_NAME " PCM", device,
			  substreams, substreams, &pcm);
	if (err < 0) {
		pr_err("wwjaudio: failed to create PCM: %d\n", err);
		return err;
	}
	wwj->pcm = pcm;

	if (fake_buffer)
		ops = &wwj_pcm_ops_no_buf;
	else
		ops = &wwj_pcm_ops;

	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK, ops);
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_CAPTURE, ops);
	pcm->private_data = wwj;
	pcm->info_flags = 0;
	strcpy(pcm->name, WWJAUDIO_CARD_NAME " PCM");

	if (!fake_buffer) {
		snd_pcm_set_managed_buffer_all(pcm,
			SNDRV_DMA_TYPE_CONTINUOUS,
			NULL,
			0, 64*1024);
	}

	return 0;
}

static int wwj_mixer_new(struct wwj_audio *wwj)
{
	struct snd_card *card = wwj->card;

	strcpy(card->mixername, WWJAUDIO_CARD_NAME " Mixer");
	return 0;
}

static int wwj_audio_probe(struct platform_device *pdev)
{
	struct snd_card *card;
	struct wwj_audio *wwj;
	int err;

	pr_info("wwjaudio: %s - creating sound card\n", __func__);

	err = snd_devm_card_new(&pdev->dev, SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1,
				THIS_MODULE, sizeof(struct wwj_audio), &card);
	if (err < 0) {
		pr_err("wwjaudio: failed to create sound card: %d\n", err);
		return err;
	}

	wwj = card->private_data;
	wwj->card = card;
	wwj->pcm_hw = wwj_pcm_hw;

	err = wwj_pcm_new(wwj, 0, 1);
	if (err < 0)
		return err;

	err = wwj_mixer_new(wwj);
	if (err < 0)
		return err;

	strcpy(card->driver, WWJAUDIO_NAME);
	strcpy(card->shortname, WWJAUDIO_CARD_NAME);
	sprintf(card->longname, "%s", WWJAUDIO_CARD_NAME);

	err = snd_card_register(card);
	if (err < 0) {
		pr_err("wwjaudio: failed to register sound card: %d\n", err);
		return err;
	}

	platform_set_drvdata(pdev, card);
	pr_info("wwjaudio: sound card registered successfully\n");
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int wwj_audio_suspend(struct device *dev)
{
	struct snd_card *card = dev_get_drvdata(dev);
	snd_power_change_state(card, SNDRV_CTL_POWER_D3hot);
	return 0;
}

static int wwj_audio_resume(struct device *dev)
{
	struct snd_card *card = dev_get_drvdata(dev);
	snd_power_change_state(card, SNDRV_CTL_POWER_D0);
	return 0;
}

static SIMPLE_DEV_PM_OPS(wwj_audio_pm, wwj_audio_suspend, wwj_audio_resume);
#define WWJAUDIO_PM_OPS	&wwj_audio_pm
#else
#define WWJAUDIO_PM_OPS	NULL
#endif

static struct platform_driver wwj_audio_driver = {
	.probe		= wwj_audio_probe,
	.driver		= {
		.name	= WWJAUDIO_NAME,
		.pm	= WWJAUDIO_PM_OPS,
	},
};

static int alloc_fake_buffer(void)
{
	int i;

	if (!fake_buffer)
		return 0;
	for (i = 0; i < 2; i++) {
		wwj_page[i] = (void *)get_zeroed_page(GFP_KERNEL);
		if (!wwj_page[i]) {
			for (i = i-1; i >= 0; i--)
				free_page((unsigned long)wwj_page[i]);
			return -ENOMEM;
		}
	}
	return 0;
}

static void free_fake_buffer(void)
{
	if (fake_buffer) {
		int i;
		for (i = 0; i < 2; i++)
			if (wwj_page[i]) {
				free_page((unsigned long)wwj_page[i]);
				wwj_page[i] = NULL;
			}
	}
}

static int __init wwj_audio_init(void)
{
	int err;

	pr_info("wwjaudio: %s - initializing\n", __func__);

	err = platform_driver_register(&wwj_audio_driver);
	if (err < 0) {
		pr_err("wwjaudio: failed to register driver: %d\n", err);
		return err;
	}

	err = alloc_fake_buffer();
	if (err < 0) {
		pr_err("wwjaudio: failed to allocate fake buffer\n");
		platform_driver_unregister(&wwj_audio_driver);
		return err;
	}

	wwj_audio_device = platform_device_register_simple(WWJAUDIO_NAME,
							   -1, NULL, 0);
	if (IS_ERR(wwj_audio_device)) {
		err = PTR_ERR(wwj_audio_device);
		pr_err("wwjaudio: failed to register device: %d\n", err);
		free_fake_buffer();
		platform_driver_unregister(&wwj_audio_driver);
		return err;
	}

	pr_info("wwjaudio: initialized successfully\n");
	return 0;
}

static void __exit wwj_audio_exit(void)
{
	pr_info("wwjaudio: %s - exiting\n", __func__);
	platform_device_unregister(wwj_audio_device);
	platform_driver_unregister(&wwj_audio_driver);
	free_fake_buffer();
}

module_init(wwj_audio_init);
module_exit(wwj_audio_exit);