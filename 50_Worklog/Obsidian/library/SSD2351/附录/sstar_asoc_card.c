/*
 * sstar_asoc_card.c - Sigmastar
 *
 * Copyright (c) [2019~2022] SigmaStar Technology.
 *
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License version 2 for more details.
 *
 */

#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <sound/core.h>
#include <sound/jack.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/simple_card_utils.h>

#include "mhal_audio.h"

#include "drv_adcmp.h"

#define DRV_NAME         "sstar-asoc-card"
#define CODEC_NAME_BYTES 30

static const struct of_device_id sstar_asoc_card_dt_ids[] = {
    {.compatible = "sstar, asoc-card", .data = (void *)0},
    {.compatible = "sstar, asoc2-card", .data = (void *)1},
    {.compatible = "sstar, asoc3-card", .data = (void *)2},
    {},
};

typedef enum
{
    UNPLUG,
    HEADPHONE,
    HEADSET,
} jack_device;

struct sstar_jack_priv
{
    struct asoc_simple_jack jack;
#if defined(CONFIG_SSTAR_ADCMP)
    struct hrtimer timer;
    int            poll_time;
    int            adc_chn;
    int            adc_values[6];
    jack_device    prev_jack_dev;
#endif
};

#if defined(CONFIG_SSTAR_ADCMP)
static void sstar_jack_report(struct sstar_jack_priv *jack_priv, jack_device dev)
{
    if (jack_priv->prev_jack_dev == dev)
    {
        // do noting
        return;
    }
    if (jack_priv->prev_jack_dev != UNPLUG)
    {
        snd_soc_jack_report(&(jack_priv->jack.jack), 0,
                            jack_priv->prev_jack_dev == HEADSET ? SND_JACK_HEADSET : SND_JACK_HEADPHONE);
    }
    if (dev == HEADSET)
    {
        jack_priv->jack.jack.jack->type = SND_JACK_HEADSET;
        snd_soc_jack_report(&(jack_priv->jack.jack), SND_JACK_HEADSET, SND_JACK_HEADSET);
    }
    else if (dev == HEADPHONE)
    {
        jack_priv->jack.jack.jack->type = SND_JACK_HEADPHONE;
        snd_soc_jack_report(&(jack_priv->jack.jack), SND_JACK_HEADPHONE, SND_JACK_HEADPHONE);
    }
    jack_priv->prev_jack_dev = dev;
}

#define ADC_VALUE_IN_ARRAY(DEV, val)                                        \
                                                                            \
    ((priv->adc_values[DEV * 2] != 0 || priv->adc_values[DEV * 2 + 1] != 0) \
     && (val > priv->adc_values[DEV * 2] && val < priv->adc_values[DEV * 2 + 1]))

static enum hrtimer_restart _pwmadc_timer_callback(struct hrtimer *timer)
{
    struct sstar_jack_priv *priv = container_of(timer, struct sstar_jack_priv, timer);
    struct adcmp_info       info = {0};

    info.channel = priv->adc_chn;

    // Get data
    sstar_adcmp_sample_channel(0, info.channel, &info.ch_data); // 0:group 0 -> pm adc0~21; 1:group 1 -> nonpm adc0~1

    // Process value
    if (ADC_VALUE_IN_ARRAY(UNPLUG, info.ch_data))
    {
        // 1.remove
        // printk("[unplug]pwmadc get channel4, data[%hu]\n", info.ch_data);
        sstar_jack_report(priv, UNPLUG);
    }
    else if (ADC_VALUE_IN_ARRAY(HEADSET, info.ch_data))
    {
        // 2.Headset
        // printk("[headset]pwmadc get channel5, data[%hu]\n", info.ch_data);
        sstar_jack_report(priv, HEADSET);
    }
    else if (ADC_VALUE_IN_ARRAY(HEADPHONE, info.ch_data))
    {
        // 3.Headphone
        // printk("[headphone]pwmadc get channel5, data[%hu]\n", info.ch_data);
        sstar_jack_report(priv, HEADPHONE);
    }
    else
    {
        // printk("[else]pwmadc get channel5, data[%hu]\n", info.ch_data);
    }

    hrtimer_forward_now(timer, ms_to_ktime(priv->poll_time));

    return HRTIMER_RESTART;
}

static int _pwmadc_init(struct snd_soc_pcm_runtime *runtime)
{
    struct adcmp_config     adc_cfg = {0};
    struct snd_soc_card *   card    = runtime->card;
    struct sstar_jack_priv *priv    = snd_soc_card_get_drvdata(card);
    int                     ret     = 0;

    adc_cfg.inje_en  = 1;
    adc_cfg.inje_mod = 12;
    adc_cfg.regu_en  = 1;
    adc_cfg.regu_mod = 12;
    ret              = sstar_adcmp_set_config(&adc_cfg, 0);
    if (ret)
    {
        printk("pwmadc config fail\n");
        return ret;
    }

    hrtimer_init(&priv->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    priv->timer.function = _pwmadc_timer_callback;
    hrtimer_start(&priv->timer, ms_to_ktime(priv->poll_time), HRTIMER_MODE_REL);
    return 0;
}
#endif

static int sstar_init(struct snd_soc_pcm_runtime *runtime)
{
    struct snd_soc_dai *    codec_dai = asoc_rtd_to_codec(runtime, 0);
    struct device *         dev       = codec_dai->dev;
    struct snd_soc_card *   card      = runtime->card;
    struct sstar_jack_priv *priv      = snd_soc_card_get_drvdata(card);
    enum of_gpio_flags      flags;
    int                     det;
    int                     ret;

    det = of_get_named_gpio_flags(dev->of_node, "headset-det-gpio", 0, &flags);

    if (det != -EPROBE_DEFER && gpio_is_valid(det))
    {
        priv->jack.gpio.name   = "Headset detection";
        priv->jack.gpio.report = SND_JACK_HEADPHONE;
        priv->jack.gpio.gpio   = det;
        priv->jack.gpio.invert = !!(flags & OF_GPIO_ACTIVE_LOW);
        ret = of_property_read_u32(dev->of_node, "headset-debounce-time", &priv->jack.gpio.debounce_time);
        if (ret != 0)
        {
            priv->jack.gpio.debounce_time = 150;
        }

        snd_soc_card_jack_new(card, "Headset", SND_JACK_HEADSET, &priv->jack.jack);

        ret = snd_soc_jack_add_gpios(&priv->jack.jack, 1, &priv->jack.gpio);
    }
#if defined(CONFIG_SSTAR_ADCMP)
    else
    {
        int i = 0;
        // PWM jack
        ret = of_property_read_u32(dev->of_node, "headset-adc-channel", &priv->adc_chn);

        if (!ret)
        {
            ret =
                of_property_read_u32_array(dev->of_node, "headset-adc-value-unplug", &priv->adc_values[UNPLUG * 2], 2);
            ret |= of_property_read_u32_array(dev->of_node, "headset-adc-value-headphone",
                                              &priv->adc_values[HEADPHONE * 2], 2);
            ret |= of_property_read_u32_array(dev->of_node, "headset-adc-value-headset", &priv->adc_values[HEADSET * 2],
                                              2);
            for (i = 0; i < 6; i++)
            {
                printk("adc_value[%d] = %d\n", i, priv->adc_values[i]);
            }
            ret = of_property_read_u32(dev->of_node, "headset-adc-poll-time", &priv->poll_time);
            if (ret != 0)
            {
                priv->poll_time = 1000;
            }

            snd_soc_card_jack_new(card, "Headset", SND_JACK_HEADSET, &priv->jack.jack);
            _pwmadc_init(runtime);
        }
    }
#endif

    return 0;
}

static void sstar_exit(struct snd_soc_pcm_runtime *runtime)
{
#if defined(CONFIG_SSTAR_ADCMP)
    struct snd_soc_card *   card = runtime->card;
    struct sstar_jack_priv *priv = snd_soc_card_get_drvdata(card);

    hrtimer_cancel(&priv->timer);
#endif
}

static int sstar_mc_hw_startup(struct snd_pcm_substream *substream)
{
    return 0;
}

static const struct snd_soc_ops sstar_mc_ops = {
    .startup = sstar_mc_hw_startup,
};

SND_SOC_DAILINK_DEFS(private, DAILINK_COMP_ARRAY(COMP_EMPTY()), DAILINK_COMP_ARRAY(COMP_CODEC(NULL, "private-hifi")),
                     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(private2, DAILINK_COMP_ARRAY(COMP_EMPTY()), DAILINK_COMP_ARRAY(COMP_CODEC(NULL, "private2-hifi")),
                     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(private3, DAILINK_COMP_ARRAY(COMP_EMPTY()), DAILINK_COMP_ARRAY(COMP_CODEC(NULL, "private3-hifi")),
                     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(auxiliary, DAILINK_COMP_ARRAY(COMP_EMPTY()),
                     DAILINK_COMP_ARRAY(COMP_CODEC(NULL, "auxiliary-hifi")), DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(auxiliary2, DAILINK_COMP_ARRAY(COMP_EMPTY()),
                     DAILINK_COMP_ARRAY(COMP_CODEC(NULL, "auxiliary-2-hifi")), DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(auxiliary3, DAILINK_COMP_ARRAY(COMP_EMPTY()),
                     DAILINK_COMP_ARRAY(COMP_CODEC(NULL, "auxiliary-3-hifi")), DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(auxiliary4, DAILINK_COMP_ARRAY(COMP_EMPTY()),
                     DAILINK_COMP_ARRAY(COMP_CODEC(NULL, "auxiliary-4-hifi")), DAILINK_COMP_ARRAY(COMP_EMPTY()));
static struct snd_soc_dai_link sstar_asoc_card_dailinks[] = {
    [0] =
        {
            .name        = "HiFi",
            .stream_name = "HiFi PCM",
            .init        = sstar_init,
            .exit        = sstar_exit,
            .ops         = &sstar_mc_ops,
            .dai_fmt     = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBM_CFM,
            SND_SOC_DAILINK_REG(private),
        },
    [1] =
        {
            .name        = "aux-snd",
            .stream_name = "aux-snd-control",
            .init        = sstar_init,
            .exit        = sstar_exit,
            .ops         = &sstar_mc_ops,
            .dai_fmt     = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS,
            SND_SOC_DAILINK_REG(auxiliary),
        },
    [2] =
        {
            .name        = "aux2-snd",
            .stream_name = "aux2-snd-control",
            .init        = sstar_init,
            .exit        = sstar_exit,
            .ops         = &sstar_mc_ops,
            .dai_fmt     = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS,
            SND_SOC_DAILINK_REG(auxiliary2),
        },
    [3] =
        {
            .name        = "aux3-snd",
            .stream_name = "aux3-snd-control",
            .init        = sstar_init,
            .exit        = sstar_exit,
            .ops         = &sstar_mc_ops,
            .dai_fmt     = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS,
            SND_SOC_DAILINK_REG(auxiliary3),
        },
    [4] =
        {
            .name        = "aux4-snd",
            .stream_name = "aux4-snd-control",
            .init        = sstar_init,
            .exit        = sstar_exit,
            .ops         = &sstar_mc_ops,
            .dai_fmt     = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS,
            SND_SOC_DAILINK_REG(auxiliary4),
        },
};

static struct snd_soc_dai_link sstar_asoc2_card_dailinks[] = {
    [0] =
        {
            .name        = "HiFi",
            .stream_name = "HiFi PCM",
            .init        = sstar_init,
            .exit        = sstar_exit,
            .ops         = &sstar_mc_ops,
            .dai_fmt     = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS,
            SND_SOC_DAILINK_REG(private2),
        },
};

static struct snd_soc_dai_link sstar_asoc3_card_dailinks[] = {
    [0] =
        {
            .name        = "HiFi",
            .stream_name = "HiFi PCM",
            .init        = sstar_init,
            .exit        = sstar_exit,
            .ops         = &sstar_mc_ops,
            .dai_fmt     = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS,
            SND_SOC_DAILINK_REG(private3),
        },
};

static struct snd_soc_card sstar_asoc_card[] = {{
                                                    .name      = "sstar-asoc-card",
                                                    .owner     = THIS_MODULE,
                                                    .dai_link  = sstar_asoc_card_dailinks,
                                                    .num_links = ARRAY_SIZE(sstar_asoc_card_dailinks),
                                                },
                                                {
                                                    .name      = "sstar-asoc2-card",
                                                    .owner     = THIS_MODULE,
                                                    .dai_link  = sstar_asoc2_card_dailinks,
                                                    .num_links = ARRAY_SIZE(sstar_asoc2_card_dailinks),
                                                },
                                                {
                                                    .name      = "sstar-asoc3-card",
                                                    .owner     = THIS_MODULE,
                                                    .dai_link  = sstar_asoc3_card_dailinks,
                                                    .num_links = ARRAY_SIZE(sstar_asoc3_card_dailinks),
                                                }};

static int sstar_asoc_card_remove(struct platform_device *pdev)
{
    mhal_audio_module_deinit();
    mhal_audio_deinit();

    return 0;
}

static int sstar_asoc_card_probe(struct platform_device *pdev)
{
    int                        ret        = 0;
    struct snd_soc_card *      card       = NULL;
    struct device *            dev        = &pdev->dev;
    struct device_node *       np         = pdev->dev.of_node;
    struct device_node *       np_cpu_d0  = NULL;
    struct device_node *       np_codec   = NULL;
    struct device_node *       np_tmp     = NULL;
    const char *               codec_name = NULL;
    struct sstar_jack_priv *   priv       = NULL;
    int                        i          = 0;
    const struct of_device_id *of_id;
    kernel_ulong_t             index = 0;

    of_id = of_match_device(sstar_asoc_card_dt_ids, &pdev->dev);
    if (!of_id)
        return -EINVAL;
    index = (kernel_ulong_t)of_id->data;

    /* Parse DTS for cpu controller. */
    np_tmp    = of_find_node_by_name(np, "sstar-audio-card,cpu");
    np_cpu_d0 = of_parse_phandle(np_tmp, "sound-dai", 0);

    if (!np_cpu_d0)
    {
        dev_err(&pdev->dev, "Property 'sstar,cpu-controller missing or invalid\n");
        return -EINVAL;
    }

    /* Parse DTS for codec. */
    np_tmp = of_find_node_by_name(np, "sstar-audio-card,codec");
    card   = &(sstar_asoc_card[index]);
    for (i = 0;; i++)
    {
        np_codec = of_parse_phandle(np_tmp, "sound-dai", i);
        if (!np_codec)
        {
            if (i == 0)
            {
                dev_err(dev, "At least one of codecs should be specified\n");
                return -EINVAL;
            }
            else
            {
                break;
            }
        }
        if (!of_device_is_available(np_codec))
        {
            break;
        }
        codec_name = devm_kzalloc(&pdev->dev, CODEC_NAME_BYTES, GFP_KERNEL);
        if (!codec_name)
            return -ENOMEM;

        if (of_property_read_string(np_codec, "codec-name", &codec_name) != 0) {
            /* If codec-name is not specified, use the compatible string to determine DAI name */
            const char *compatible;
            if (of_property_read_string(np_codec, "compatible", &compatible) == 0) {
                if (strcmp(compatible, "acm8816") == 0) {
                    strncpy(codec_name, "acm8816-hifi", CODEC_NAME_BYTES - 1);
                }
            }
        }
        dev_info(dev, "i = %d, codec_name = %s\n", i, codec_name ? codec_name : "NULL");
        card->dai_link[i].codecs->of_node    = np_codec;
        card->dai_link[i].codecs->dai_name   = codec_name;
        card->dai_link[i].cpus->of_node      = np_cpu_d0;
        card->dai_link[i].platforms->of_node = np_cpu_d0;
        card->num_links                      = i + 1;
    }

    card->dev = dev;

    priv = devm_kzalloc(dev, sizeof(struct sstar_jack_priv), GFP_KERNEL);
    if (!priv)
        return -ENOMEM;

    memset(priv, 0, sizeof(struct sstar_jack_priv));

    snd_soc_card_set_drvdata(card, priv);

    /* Parse card name. */
    ret = snd_soc_of_parse_card_name(card, "sstar-audio-card, name");
    if (ret)
    {
        dev_err(&pdev->dev, "Soc parse card name failed %d\n", ret);
        return ret;
    }

    /* register the soc card */
    ret = devm_snd_soc_register_card(&pdev->dev, card);
    if (ret)
    {
        dev_err(&pdev->dev, "Soc register card failed %d\n", ret);
        return ret;
    }

    return ret;
}

MODULE_DEVICE_TABLE(of, sstar_asoc_card_dt_ids);

static struct platform_driver sstar_asoc_card_driver = {
    .probe  = sstar_asoc_card_probe,
    .remove = sstar_asoc_card_remove,
    .driver =
        {
            .name           = DRV_NAME,
            .pm             = &snd_soc_pm_ops,
            .of_match_table = sstar_asoc_card_dt_ids,
        },
};

module_platform_driver(sstar_asoc_card_driver);

MODULE_AUTHOR("SSTAR");
MODULE_DESCRIPTION("SigmaStar ASoC machine driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DRV_NAME);
