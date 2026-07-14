/*
 * acm8816.c -- ACM8816 ALSA SoC audio CODEC driver
 *
 * Copyright (C) 2023 Your Name
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <sound/soc-dai.h>

#include "acm8816.h"

char *acm8816_state_names[] = {"Digital Off", "Analog Off", "Driver Off", "Launch"};

/* ACM8816私有数据结构 */
struct acm8816_priv {
    struct i2c_client *i2c;  /* I2C客户端 */
    struct regmap *regmap;   /* 寄存器映射 */
    int sysclk;              /* 系统时钟频率 */
    int format;              /* 音频格式 */
    int rate;                /* 采样率 */
    bool muted;              /* 静音状态 */
    struct mutex lock;       /* 互斥锁 */
    struct dentry *debugfs_dir; /* debugfs目录 */
    struct dentry *debugfs_regs;
    struct dentry *debugfs_temp;
    struct dentry *debugfs_pvdd;
    struct dentry *debugfs_gvdd;
    struct dentry *debugfs_ntc;
    struct dentry *debugfs_status;
    struct dentry *debugfs_warn_clear; /* 告警清除接口 */
    struct dentry *debugfs_readme;
    struct dentry *debugfs_sample_rate; /* 采样率读取接口 */
    struct dentry *debugfs_dump; /* 寄存器dump接口 */
    unsigned int last_reg_addr; /* 最后一次访问的寄存器地址 */
    loff_t file_pos; /* 文件位置，用于支持多次读取 */
};

/* 寄存器配置 */
static const struct reg_default acm8816_reg_defaults[] = {
    {0x00, 0x00},
    {0x04, 0x00},
    {0xfc, 0x86},
    {0xfd, 0x25},
    {0xfe, 0x15},
    {0x00, 0x01},
    {0x02, 0x20},
    {0x00, 0x00},
    {ACM8816_REG_STATE_CTRL, 0x00},
    {ACM8816_REG_PROCESSING_CTRL1, 0x1e},
    {ACM8816_REG_PROCESSING_CTRL2, 0xF0},
    {ACM8816_REG_I2S_DATA_FORMAT1, 0x00},
    {ACM8816_REG_I2S_DATA_FORMAT2, 0x00},
};

static const struct reg_default acm8816_dsp_reg_defaults[] = {
    { 0x00, 0x00 },
    { 0x04, 0x00 },
    { 0x00, 0x00 },
    { 0x00, 0x00 },
    { 0x00, 0x00 },
    { 0x00, 0x00 },
    { 0x00, 0x00 },
    //Lookahead Delay Buffer
    //EQ
    //EQ1 - left; Setting: Type - All Pass; Fc - 1000; Gain - 0.0; Q - 0.707; Bandwidth - 1000
    { 0x00, 0x06 },
    { 0x0c, 0x08 },
    { 0x0d, 0x00 },
    { 0x0e, 0x00 },
    { 0x0f, 0x00 },
    { 0x10, 0x00 },
    { 0x11, 0x00 },
    { 0x12, 0x00 },
    { 0x13, 0x00 },
    { 0x14, 0x00 },
    { 0x15, 0x00 },
    { 0x16, 0x00 },
    { 0x17, 0x00 },
    { 0x18, 0x00 },
    { 0x19, 0x00 },
    { 0x1a, 0x00 },
    { 0x1b, 0x00 },
    { 0x1c, 0x00 },
    { 0x1d, 0x00 },
    { 0x1e, 0x00 },
    { 0x1f, 0x00 },
    //EQ2 - left; Setting: Type - All Pass; Fc - 1000; Gain - 0.0; Q - 0.707; Bandwidth - 1000
    { 0x20, 0x08 },
    { 0x21, 0x00 },
    { 0x22, 0x00 },
    { 0x23, 0x00 },
    { 0x24, 0x00 },
    { 0x25, 0x00 },
    { 0x26, 0x00 },
    { 0x27, 0x00 },
    { 0x28, 0x00 },
    { 0x29, 0x00 },
    { 0x2a, 0x00 },
    { 0x2b, 0x00 },
    { 0x2c, 0x00 },
    { 0x2d, 0x00 },
    { 0x2e, 0x00 },
    { 0x2f, 0x00 },
    { 0x30, 0x00 },
    { 0x31, 0x00 },
    { 0x32, 0x00 },
    { 0x33, 0x00 },
    //EQ3 - left; Setting: Type - All Pass; Fc - 1000; Gain - 0.0; Q - 0.707; Bandwidth - 1000
    { 0x34, 0x08 },
    { 0x35, 0x00 },
    { 0x36, 0x00 },
    { 0x37, 0x00 },
    { 0x38, 0x00 },
    { 0x39, 0x00 },
    { 0x3a, 0x00 },
    { 0x3b, 0x00 },
    { 0x3c, 0x00 },
    { 0x3d, 0x00 },
    { 0x3e, 0x00 },
    { 0x3f, 0x00 },
    { 0x40, 0x00 },
    { 0x41, 0x00 },
    { 0x42, 0x00 },
    { 0x43, 0x00 },
    { 0x44, 0x00 },
    { 0x45, 0x00 },
    { 0x46, 0x00 },
    { 0x47, 0x00 },
    //EQ4 - left; Setting: Type - All Pass; Fc - 1000; Gain - 0.0; Q - 0.707; Bandwidth - 1000
    { 0x48, 0x08 },
    { 0x49, 0x00 },
    { 0x4a, 0x00 },
    { 0x4b, 0x00 },
    { 0x4c, 0x00 },
    { 0x4d, 0x00 },
    { 0x4e, 0x00 },
    { 0x4f, 0x00 },
    { 0x50, 0x00 },
    { 0x51, 0x00 },
    { 0x52, 0x00 },
    { 0x53, 0x00 },
    { 0x54, 0x00 },
    { 0x55, 0x00 },
    { 0x56, 0x00 },
    { 0x57, 0x00 },
    { 0x58, 0x00 },
    { 0x59, 0x00 },
    { 0x5a, 0x00 },
    { 0x5b, 0x00 },
    //EQ5 - left; Setting: Type - All Pass; Fc - 1000; Gain - 0.0; Q - 0.707; Bandwidth - 1000
    { 0x5c, 0x08 },
    { 0x5d, 0x00 },
    { 0x5e, 0x00 },
    { 0x5f, 0x00 },
    { 0x60, 0x00 },
    { 0x61, 0x00 },
    { 0x62, 0x00 },
    { 0x63, 0x00 },
    { 0x64, 0x00 },
    { 0x65, 0x00 },
    { 0x66, 0x00 },
    { 0x67, 0x00 },
    { 0x68, 0x00 },
    { 0x69, 0x00 },
    { 0x6a, 0x00 },
    { 0x6b, 0x00 },
    { 0x6c, 0x00 },
    { 0x6d, 0x00 },
    { 0x6e, 0x00 },
    { 0x6f, 0x00 },
    //EQ6 - left; Setting: Type - All Pass; Fc - 1000; Gain - 0.0; Q - 0.707; Bandwidth - 1000
    { 0x70, 0x08 },
    { 0x71, 0x00 },
    { 0x72, 0x00 },
    { 0x73, 0x00 },
    { 0x74, 0x00 },
    { 0x75, 0x00 },
    { 0x76, 0x00 },
    { 0x77, 0x00 },
    { 0x78, 0x00 },
    { 0x79, 0x00 },
    { 0x7a, 0x00 },
    { 0x7b, 0x00 },
    { 0x7c, 0x00 },
    { 0x7d, 0x00 },
    { 0x7e, 0x00 },
    { 0x7f, 0x00 },
    { 0x80, 0x00 },
    { 0x81, 0x00 },
    { 0x82, 0x00 },
    { 0x83, 0x00 },
    //EQ7 - left; Setting: Type - All Pass; Fc - 1000; Gain - 0.0; Q - 0.707; Bandwidth - 1000
    { 0x84, 0x08 },
    { 0x85, 0x00 },
    { 0x86, 0x00 },
    { 0x87, 0x00 },
    { 0x88, 0x00 },
    { 0x89, 0x00 },
    { 0x8a, 0x00 },
    { 0x8b, 0x00 },
    { 0x8c, 0x00 },
    { 0x8d, 0x00 },
    { 0x8e, 0x00 },
    { 0x8f, 0x00 },
    { 0x90, 0x00 },
    { 0x91, 0x00 },
    { 0x92, 0x00 },
    { 0x93, 0x00 },
    { 0x94, 0x00 },
    { 0x95, 0x00 },
    { 0x96, 0x00 },
    { 0x97, 0x00 },
    //EQ8 - left; Setting: Type - All Pass; Fc - 1000; Gain - 0.0; Q - 0.707; Bandwidth - 1000
    { 0x98, 0x08 },
    { 0x99, 0x00 },
    { 0x9a, 0x00 },
    { 0x9b, 0x00 },
    { 0x9c, 0x00 },
    { 0x9d, 0x00 },
    { 0x9e, 0x00 },
    { 0x9f, 0x00 },
    { 0xa0, 0x00 },
    { 0xa1, 0x00 },
    { 0xa2, 0x00 },
    { 0xa3, 0x00 },
    { 0xa4, 0x00 },
    { 0xa5, 0x00 },
    { 0xa6, 0x00 },
    { 0xa7, 0x00 },
    { 0xa8, 0x00 },
    { 0xa9, 0x00 },
    { 0xaa, 0x00 },
    { 0xab, 0x00 },
    //EQ9 - left; Setting: Type - All Pass; Fc - 1000; Gain - 0.0; Q - 0.707; Bandwidth - 1000
    { 0xac, 0x08 },
    { 0xad, 0x00 },
    { 0xae, 0x00 },
    { 0xaf, 0x00 },
    { 0xb0, 0x00 },
    { 0xb1, 0x00 },
    { 0xb2, 0x00 },
    { 0xb3, 0x00 },
    { 0xb4, 0x00 },
    { 0xb5, 0x00 },
    { 0xb6, 0x00 },
    { 0xb7, 0x00 },
    { 0xb8, 0x00 },
    { 0xb9, 0x00 },
    { 0xba, 0x00 },
    { 0xbb, 0x00 },
    { 0xbc, 0x00 },
    { 0xbd, 0x00 },
    { 0xbe, 0x00 },
    { 0xbf, 0x00 },
    //EQ10 - left; Setting: Type - All Pass; Fc - 1000; Gain - 0.0; Q - 0.707; Bandwidth - 1000
    { 0xc0, 0x08 },
    { 0xc1, 0x00 },
    { 0xc2, 0x00 },
    { 0xc3, 0x00 },
    { 0xc4, 0x00 },
    { 0xc5, 0x00 },
    { 0xc6, 0x00 },
    { 0xc7, 0x00 },
    { 0xc8, 0x00 },
    { 0xc9, 0x00 },
    { 0xca, 0x00 },
    { 0xcb, 0x00 },
    { 0xcc, 0x00 },
    { 0xcd, 0x00 },
    { 0xce, 0x00 },
    { 0xcf, 0x00 },
    { 0xd0, 0x00 },
    { 0xd1, 0x00 },
    { 0xd2, 0x00 },
    { 0xd3, 0x00 },
    //EQ11 - left; Setting: Type - All Pass; Fc - 1000; Gain - 0.0; Q - 0.707; Bandwidth - 1000
    { 0xd4, 0x08 },
    { 0xd5, 0x00 },
    { 0xd6, 0x00 },
    { 0xd7, 0x00 },
    { 0xd8, 0x00 },
    { 0xd9, 0x00 },
    { 0xda, 0x00 },
    { 0xdb, 0x00 },
    { 0xdc, 0x00 },
    { 0xdd, 0x00 },
    { 0xde, 0x00 },
    { 0xdf, 0x00 },
    { 0xe0, 0x00 },
    { 0xe1, 0x00 },
    { 0xe2, 0x00 },
    { 0xe3, 0x00 },
    { 0xe4, 0x00 },
    { 0xe5, 0x00 },
    { 0xe6, 0x00 },
    { 0xe7, 0x00 },
    //EQ12 - left; Setting: Type - All Pass; Fc - 1000; Gain - 0.0; Q - 0.707; Bandwidth - 1000
    { 0xe8, 0x08 },
    { 0xe9, 0x00 },
    { 0xea, 0x00 },
    { 0xeb, 0x00 },
    { 0xec, 0x00 },
    { 0xed, 0x00 },
    { 0xee, 0x00 },
    { 0xef, 0x00 },
    { 0xf0, 0x00 },
    { 0xf1, 0x00 },
    { 0xf2, 0x00 },
    { 0xf3, 0x00 },
    { 0xf4, 0x00 },
    { 0xf5, 0x00 },
    { 0xf6, 0x00 },
    { 0xf7, 0x00 },
    { 0xf8, 0x00 },
    { 0xf9, 0x00 },
    { 0xfa, 0x00 },
    { 0xfb, 0x00 },
    //EQ13 - left; Setting: Type - All Pass; Fc - 1000; Gain - 0.0; Q - 0.707; Bandwidth - 1000
    { 0xfc, 0x08 },
    { 0xfd, 0x00 },
    { 0xfe, 0x00 },
    { 0xff, 0x00 },
    { 0x00, 0x07 },
    { 0x04, 0x00 },
    { 0x05, 0x00 },
    { 0x06, 0x00 },
    { 0x07, 0x00 },
    { 0x08, 0x00 },
    { 0x09, 0x00 },
    { 0x0a, 0x00 },
    { 0x0b, 0x00 },
    { 0x0c, 0x00 },
    { 0x0d, 0x00 },
    { 0x0e, 0x00 },
    { 0x0f, 0x00 },
    { 0x10, 0x00 },
    { 0x11, 0x00 },
    { 0x12, 0x00 },
    { 0x13, 0x00 },
    //EQ14 - left; Setting: Type - All Pass; Fc - 1000; Gain - 0.0; Q - 0.707; Bandwidth - 1000
    { 0x14, 0x08 },
    { 0x15, 0x00 },
    { 0x16, 0x00 },
    { 0x17, 0x00 },
    { 0x18, 0x00 },
    { 0x19, 0x00 },
    { 0x1a, 0x00 },
    { 0x1b, 0x00 },
    { 0x1c, 0x00 },
    { 0x1d, 0x00 },
    { 0x1e, 0x00 },
    { 0x1f, 0x00 },
    { 0x20, 0x00 },
    { 0x21, 0x00 },
    { 0x22, 0x00 },
    { 0x23, 0x00 },
    { 0x24, 0x00 },
    { 0x25, 0x00 },
    { 0x26, 0x00 },
    { 0x27, 0x00 },
    //EQ15 - left; Setting: Type - All Pass; Fc - 1000; Gain - 0.0; Q - 0.707; Bandwidth - 1000
    { 0x28, 0x08 },
    { 0x29, 0x00 },
    { 0x2a, 0x00 },
    { 0x2b, 0x00 },
    { 0x2c, 0x00 },
    { 0x2d, 0x00 },
    { 0x2e, 0x00 },
    { 0x2f, 0x00 },
    { 0x30, 0x00 },
    { 0x31, 0x00 },
    { 0x32, 0x00 },
    { 0x33, 0x00 },
    { 0x34, 0x00 },
    { 0x35, 0x00 },
    { 0x36, 0x00 },
    { 0x37, 0x00 },
    { 0x38, 0x00 },
    { 0x39, 0x00 },
    { 0x3a, 0x00 },
    { 0x3b, 0x00 },
    //Pre volume
    // AlphaTime
    { 0x00, 0x04 },
    { 0x44, 0x00 },
    { 0x45, 0xe2 },
    { 0x46, 0xc4 },
    { 0x47, 0x6b },
    //ClassD Control Registers
    { 0x00, 0x01 },
    { 0x01, 0x00 },
    { 0x00, 0x00 },
    { 0x11, 0xc0 },
    { 0x02, 0x00 },
    { 0x06, 0x00 },
    { 0x05, 0x01 },
    { 0x03, 0x40 },
    { 0x01, 0x84 },
    { 0x00, 0x00 },
    { 0x04, 0x02 },
    { 0x00, 0x00 },
    { 0x00, 0x00 },
    { 0x00, 0x00 },
    { 0x00, 0x00 },
    { 0x00, 0x00 },
    { 0x00, 0x00 },
    { 0x00, 0x00 },
    { 0x00, 0x00 },
    { 0x00, 0x00 },
    { 0x00, 0x00 },
    { 0x00, 0x00 },
    { 0x00, 0x00 },
    { 0x00, 0x00 },
    { 0x00, 0x02 },
    { 0x19, 0x00 },
    { 0x1b, 0x80 },
    { 0x00, 0x03 },
    { 0x13, 0x90 },
    { 0x14, 0x90 },
    { 0x17, 0x18 },
    { 0x00, 0x00 },
    { 0x00, 0x00 },
    { 0x00, 0x00 },
    { 0x00, 0x00 },
    { 0x00, 0x00 },
    { 0x00, 0x00 },
    { 0x00, 0x00 },
    { 0x00, 0x00 },
    { 0x00, 0x00 },
    { 0x00, 0x00 },
    { 0x00, 0x00 },
    { 0x00, 0x00 },
    { 0x00, 0x00 },
    { 0x00, 0x00 },
    { 0x00, 0x00 },
    { 0x00, 0x00 },
    { 0x00, 0x00 },
    { 0x00, 0x00 },
    { 0x00, 0x00 },
    { 0x00, 0x00 },
    { 0x00, 0x00 },
    { 0x05 ,0x20 } 
};

/* 寄存器访问设置 */
static const struct regmap_config acm8816_regmap_config = {
    .reg_bits = 8,
    .val_bits = 8,
    .max_register = ACM8816_REG_CRC_CHECKSUM,
    .cache_type = REGCACHE_NONE,
    .reg_defaults = acm8816_reg_defaults,
    .num_reg_defaults = ARRAY_SIZE(acm8816_reg_defaults),
};

/* 设置系统时钟 */
static int acm8816_set_dai_sysclk(struct snd_soc_dai *codec_dai,
                                   int clk_id, unsigned int freq, int dir)
{
    struct snd_soc_component *component = codec_dai->component;
    struct acm8816_priv *priv = snd_soc_component_get_drvdata(component);
    
    dev_info(&priv->i2c->dev, "Enter acm8816_set_dai_sysclk - clk_id: %d, freq: %u Hz, dir: %d\n", 
             clk_id, freq, dir);
    return 0;
}

/* 设置DAI格式 */
static int acm8816_set_dai_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
    struct snd_soc_component *component = codec_dai->component;
    struct acm8816_priv *priv = snd_soc_component_get_drvdata(component);
    //int ret = 0;
    
    dev_info(&priv->i2c->dev, "Enter acm8816_set_dai_fmt - fmt: 0x%x\n", fmt);
    
    /* 保存格式信息 */
    priv->format = fmt;
    // 默认配置，不做修改
    
    return 0;
}


/* 设置偏置电平 */
static int acm8816_set_bias_level(struct snd_soc_component *component, 
                                  enum snd_soc_bias_level level)
{
    struct acm8816_priv *priv = snd_soc_component_get_drvdata(component);
    int ret = 0;
    
    dev_info(&priv->i2c->dev, "Enter acm8816_set_bias_level - level: %d\n", level);
    
    /* 根据目标偏置电平直接设置状态，不依赖old_bias */
    regmap_write(priv->regmap, 0x00, 0x00);
    switch (level) {
    case SND_SOC_BIAS_ON:
        dev_info(&priv->i2c->dev, "Setting bias level to ON\n");
        /* 确保CODEC处于播放状态 */
        regmap_write(priv->regmap, ACM8816_REG_STATE_CTRL, ACM8816_STATE_CTRL_STATE_PLAY);
        msleep(100);
        regmap_write(priv->regmap, ACM8816_REG_STATE_CTRL, ACM8816_STATE_CTRL_STATE_OFF);
        msleep(100);
        regmap_write(priv->regmap, ACM8816_REG_STATE_CTRL, ACM8816_STATE_CTRL_STATE_PLAY);
        break;
        
    case SND_SOC_BIAS_PREPARE:
        dev_info(&priv->i2c->dev, "Setting bias level to PREPARE\n");
        /* 确保CODEC准备好进行数据处理 */
        //regmap_write(priv->regmap, ACM8816_REG_STATE_CTRL, ACM8816_STATE_CTRL_STATE_PREPARE);
        break;
        
    case SND_SOC_BIAS_STANDBY:
        dev_info(&priv->i2c->dev, "Setting bias level to STANDBY\n");
        /* 将CODEC设置为空闲状态，允许音量控制寄存器写入 */
        //regmap_write(priv->regmap, ACM8816_REG_STATE_CTRL, ACM8816_STATE_CTRL_STATE_STANDBY);
        break;
        
    case SND_SOC_BIAS_OFF:
        dev_info(&priv->i2c->dev, "Setting bias level to OFF\n");
        /* 关闭CODEC核心功能 */
        regmap_write(priv->regmap, ACM8816_REG_STATE_CTRL, ACM8816_STATE_CTRL_STATE_OFF);
        break;
    }
    
    return ret;
}

/* 设置PCM硬件参数 */
static int acm8816_pcm_hw_params(struct snd_pcm_substream *substream,
                                 struct snd_pcm_hw_params *params,
                                 struct snd_soc_dai *dai)
{
    struct snd_soc_component *component = dai->component;
    struct acm8816_priv *priv = snd_soc_component_get_drvdata(component);
    unsigned int rate = params_rate(params);
    unsigned int channels = params_channels(params);
    unsigned int format = params_format(params);
    int sample_width = snd_pcm_format_width(format);
    unsigned char format_reg = 0;
    //unsigned char clk_format_reg = 0;
    
    dev_info(&priv->i2c->dev, "Enter acm8816_pcm_hw_params - rate: %u Hz, channels: %u, format: 0x%x, sample width: %d bits\n", 
             rate, channels, format, sample_width);
    
    /* 采样带宽(位深度)信息已经通过sample_width获取
     * 可以根据需要在这里添加对采样带宽的处理逻辑
     */
    switch (sample_width) {
    case 16:
        format_reg |= 0x00;
        break;
    // case 20:
    //     format_reg |= 0x01;
    //     break;
    // case 24:
    //     format_reg |= 0x02;
    //     break;
    case 32:
        format_reg |= 0x03;
        break;
    default:
        dev_err(&priv->i2c->dev, "Unsupported sample width: %d bits\n", sample_width);
        return -EINVAL;
    }
    
    /* 设置I2S数据格式 */
    regmap_write(priv->regmap, 0x00, 0x00);
    regmap_write(priv->regmap, ACM8816_REG_I2S_DATA_FORMAT1, format_reg);
    
    // /* 对于192000采样率，设置blck固定为128*采样率 */
    // if (rate == 192000) {
    //     dev_info(&priv->i2c->dev, "Setting blck to 128*sample_rate for 192000 Hz\n");
    //     /* BCLK_MASK: 0x3 << 4 设置BCLK比例为128FS
    //      * 0x3 << 4 = 0x30
    //      */
    //     clk_format_reg = (0x3 << 4);
    //     regmap_write(priv->regmap, ACM8816_REG_I2S_CLK_FORMAT_RPT1, clk_format_reg);
    // }

    if(substream->stream == SNDRV_PCM_STREAM_PLAYBACK){
        acm8816_set_bias_level(component, SND_SOC_BIAS_ON);
    }

    return 0;
}


/* 启动PCM */
static int acm8816_pcm_startup(struct snd_pcm_substream *substream,
                               struct snd_soc_dai *dai)
{
    struct snd_soc_component *component = dai->component;
    struct acm8816_priv *priv = snd_soc_component_get_drvdata(component);
    
    dev_info(&priv->i2c->dev, "Enter acm8816_pcm_startup - stream: %s\n", 
             substream->stream == SNDRV_PCM_STREAM_PLAYBACK ? "Playback" : "Capture");
    
    // if(substream->stream == SNDRV_PCM_STREAM_PLAYBACK){
    //     acm8816_set_bias_level(component, SND_SOC_BIAS_ON);
    // }
    
    return 0;
}

/* 停止PCM */
static void acm8816_pcm_shutdown(struct snd_pcm_substream *substream, struct snd_soc_dai *dai)
{
    struct snd_soc_component *component = dai->component;
    struct acm8816_priv *priv = snd_soc_component_get_drvdata(component);
    
    dev_info(&priv->i2c->dev, "Enter acm8816_pcm_shutdown - stream: %s\n", 
             substream->stream == SNDRV_PCM_STREAM_PLAYBACK ? "Playback" : "Capture");
    
    if(substream->stream == SNDRV_PCM_STREAM_PLAYBACK){
        /* 将CODEC状态从播放模式切换到空闲模式 */
        regmap_write(priv->regmap, 0x00, 0x00);
        regmap_write(priv->regmap, ACM8816_REG_STATE_CTRL, ACM8816_STATE_CTRL_STATE_OFF);
    }
}

/* DAI操作结构体 */
static const struct snd_soc_dai_ops acm8816_dai_ops = {
    .set_sysclk = acm8816_set_dai_sysclk,
    .set_fmt = acm8816_set_dai_fmt,
    .hw_params = acm8816_pcm_hw_params,
    .startup = acm8816_pcm_startup,
    .shutdown = acm8816_pcm_shutdown,
};

/* DAI驱动定义 - 支持播放和录音 */
static struct snd_soc_dai_driver acm8816_dai = {
    .name = "acm8816-hifi",
    .playback = {
        .channels_min = 1,
        .channels_max = 1,
        .rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_192000,
        .formats = SNDRV_PCM_FMTBIT_S16_LE  | SNDRV_PCM_FMTBIT_S32_LE,
    },

    .capture = {
        .channels_min = 1,
        .channels_max = 1,
        .rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_192000,
        .formats = SNDRV_PCM_FMTBIT_S16_LE  | SNDRV_PCM_FMTBIT_S32_LE,
    },
    .ops = &acm8816_dai_ops,
};



/* DAPM Widgets定义 */
static const struct snd_soc_dapm_widget acm8816_dapm_widgets[] = {
    /* Digital Audio Interface */
    SND_SOC_DAPM_OUTPUT("Playback"),
    SND_SOC_DAPM_INPUT("Capture"),
    
    /* DAC路径 */
    SND_SOC_DAPM_DAC("ACM8816 DAC", NULL, 0, 0, NULL),
    SND_SOC_DAPM_MIXER("Output Mixer", SND_SOC_NOPM, 0, 0, NULL, 0),
    SND_SOC_DAPM_OUTPUT("Headphone"),
    SND_SOC_DAPM_OUTPUT("Speaker"),
    
    /* ADC路径 */
    SND_SOC_DAPM_ADC("ACM8816 ADC", NULL, 0, 0, NULL),
    SND_SOC_DAPM_MIXER("Input Mixer", SND_SOC_NOPM, 0, 0, NULL, 0),
    SND_SOC_DAPM_INPUT("Microphone"),
    
};

/* DAPM路由定义 */
static const struct snd_soc_dapm_route acm8816_dapm_routes[] = {
    /* 播放路径 */
    { "ACM8816 DAC", NULL, "Playback" },
    { "Output Mixer", NULL, "ACM8816 DAC" },
    { "Headphone", NULL, "Output Mixer" },
    { "Speaker", NULL, "Output Mixer" },

    /* 录音路径 */
    { "Capture", NULL, "ACM8816 ADC" },
    { "ACM8816 ADC", NULL, "Input Mixer" },
    { "Input Mixer", NULL, "Microphone" },
};

static int acm8816_get_pvdd(struct acm8816_priv *priv)
{
    int pvdd =  0;
    unsigned int val = 0;
    int ret;

    mutex_lock(&priv->lock);
    /* 读取PVDD电压寄存器 */
    regmap_write(priv->regmap, 0x00, 0x00);
    ret = regmap_read(priv->regmap, ACM8816_REG_PVDD_SNS_RPT, &val);
    mutex_unlock(&priv->lock);
    
    if (ret) {
        dev_err(&priv->i2c->dev, "Failed to get PVDD voltage: %d\n", ret);
        return 0;
    }
    
    /* 根据数据手册公式：电压 = 85 * code / 255 (单位: V) */
    pvdd = 85 * val*1000 / 255;
    dev_info(&priv->i2c->dev, "PVDD code=0x%02x, voltage=%dmV\n", val, pvdd);
    
    return pvdd;
}

/* 读取芯片温度 */
static int acm8816_get_temperature(struct acm8816_priv *priv)
{
    int temp = 0;
    unsigned int val = 0;
    int ret;

    mutex_lock(&priv->lock);
    /* 读取温度寄存器 */
    regmap_write(priv->regmap, 0x00, 0x00);
    ret = regmap_read(priv->regmap, ACM8816_REG_TEMP_SNS_RPT, &val);
    mutex_unlock(&priv->lock);
    
    if (ret) {
        dev_err(&priv->i2c->dev, "Failed to get temperature: %d\n", ret);
        return 0;
    }
    
    /* 根据数据手册公式：温度 = -57 + code */
    temp = -57 + val;
    dev_info(&priv->i2c->dev, "Temperature code=0x%02x, temperature=%d°C\n", val, temp);
    
    return temp;
}

/* 读取GVDD电压 */
static int acm8816_get_gvdd(struct acm8816_priv *priv)
{
    int gvdd = 0;
    unsigned int val = 0;
    int ret;

    mutex_lock(&priv->lock);
    /* 读取GVDD电压寄存器 */
    regmap_write(priv->regmap, 0x00, 0x00);
    ret = regmap_read(priv->regmap, ACM8816_REG_GVDD_SNS_RPT, &val);
    mutex_unlock(&priv->lock);
    
    if (ret) {
        dev_err(&priv->i2c->dev, "Failed to get GVDD voltage: %d\n", ret);
        return 0;
    }
    
    /* 根据数据手册公式：电压 = 8 * code / 255 (单位: V) */
    gvdd = 8 * val *1000 / 255;
    dev_info(&priv->i2c->dev, "GVDD code=0x%02x, voltage=%dmV\n", val, gvdd);
    
    return gvdd;
}

/* 读取NTC电压 */
static int acm8816_get_ntc_voltage(struct acm8816_priv *priv)
{
    int ntc_voltage = 0;
    unsigned int val = 0;
    int ret;

    mutex_lock(&priv->lock);
    /* 读取NTC电压寄存器 */
    regmap_write(priv->regmap, 0x00, 0x00);
    ret = regmap_read(priv->regmap, ACM8816_REG_NTC_SNS_RPT, &val);
    mutex_unlock(&priv->lock);
    
    if (ret) {
        dev_err(&priv->i2c->dev, "Failed to get NTC voltage: %d\n", ret);
        return 0;
    }
    
    /* 根据数据手册公式：电压 = 2.5 * code / 255 (单位: V) */
    ntc_voltage = 25*100 * val / 255;
    dev_info(&priv->i2c->dev, "NTC code=0x%02x, voltage=%dmV\n", val, ntc_voltage);
    
    return ntc_voltage;
}

/* 启用采样功能 */
static int acm8816_enable_sensing(struct acm8816_priv *priv)
{
    int ret;
    unsigned int val;

    mutex_lock(&priv->lock);
    regmap_write(priv->regmap, 0x00, 0x00);
    /* 读取采样控制寄存器 */
    ret = regmap_read(priv->regmap, ACM8816_REG_PTSNS_CTRL, &val);
    if (ret) {
        mutex_unlock(&priv->lock);
        dev_err(&priv->i2c->dev, "Failed to read sensing control: %d\n", ret);
        return ret;
    }
    
    /* 使能PVDD、GVDD、温度和NTC采样 */
    val |= 0x0F;  /* 低4位分别是PVDD、GVDD、温度和NTC采样使能位 */
    
    /* 写回采样控制寄存器 */
    regmap_write(priv->regmap, 0x00, 0x00);
    ret = regmap_write(priv->regmap, ACM8816_REG_PTSNS_CTRL, val);
    
    mutex_unlock(&priv->lock);
    
    if (ret) {
        dev_err(&priv->i2c->dev, "Failed to enable sensing: %d\n", ret);
        return ret;
    }
    
    dev_info(&priv->i2c->dev, "Sensing enabled: PVDD, GVDD, Temperature, NTC\n");
    return 0;
}

/* debugfs寄存器读写接口实现 */
static ssize_t acm8816_reg_read(struct file *file, char __user *buf,
                               size_t count, loff_t *ppos)
{
    struct acm8816_priv *priv = file->private_data;
    char temp[64];
    ssize_t ret;
    unsigned int reg_val;
    
    /* 限制输出大小 */
    if (count < sizeof(temp)) {
        return -EINVAL;
    }
    
    mutex_lock(&priv->lock);
    /* 读取最后一次写入的寄存器地址对应的值 */
    regmap_write(priv->regmap, 0x00, 0x00);
    ret = regmap_read(priv->regmap, priv->last_reg_addr, &reg_val);
    mutex_unlock(&priv->lock);
    
    if (ret < 0) {
        dev_err(&priv->i2c->dev, "Failed to read register 0x%x: %zd\n", priv->last_reg_addr, ret);
        return ret;
    }
    
    /* 格式化输出结果 */
    scnprintf(temp, sizeof(temp), "Register 0x%x = 0x%x\n", priv->last_reg_addr, reg_val);
    
    /* 复制结果到用户空间 */
    if (copy_to_user(buf, temp, strlen(temp))) {
        return -EFAULT;
    }
    
    /* 更新文件位置但不限制只读取一次 */
    if (*ppos == 0) {
        *ppos = strlen(temp);
        return *ppos;
    }
    return 0;
}

static ssize_t acm8816_reg_write(struct file *file, const char __user *buf,
                                size_t count, loff_t *ppos)
{
    struct acm8816_priv *priv = file->private_data;
    char temp[64];
    ssize_t ret;
    unsigned int reg_addr, reg_val;
    char *p;
    int args;
    
    /* 限制输入大小 */
    if (count > sizeof(temp) - 1) {
        return -EINVAL;
    }
    
    /* 读取用户输入 */
    if (copy_from_user(temp, buf, count)) {
        return -EFAULT;
    }
    temp[count] = '\0';
    
    /* 解析输入 */
    p = strstrip(temp);
    args = sscanf(p, "%x %x", &reg_addr, &reg_val);
    
    /* 检查寄存器地址是否有效 */
    if (reg_addr > ACM8816_RESET) {
        return -EINVAL;
    }
    
    mutex_lock(&priv->lock);
    
    /* 保存寄存器地址供读取使用 */
    priv->last_reg_addr = reg_addr;
    
    /* 如果提供了寄存器值，则写入 */
    if (args == 2) {
        regmap_write(priv->regmap, 0x00, 0x00);
        ret = regmap_write(priv->regmap, reg_addr, reg_val);
        if (ret < 0) {
            dev_err(&priv->i2c->dev, "Failed to write register 0x%x: %zd\n", reg_addr, ret);
            mutex_unlock(&priv->lock);
            return ret;
        }
        dev_info(&priv->i2c->dev, "Wrote 0x%x to register 0x%x\n", reg_val, reg_addr);
    } else if (args == 1) {
        /* 只提供了寄存器地址，仅保存用于后续读取 */
        dev_info(&priv->i2c->dev, "Set register address to 0x%x\n", reg_addr);
    } else {
        mutex_unlock(&priv->lock);
        return -EINVAL;
    }
    
    mutex_unlock(&priv->lock);
    
    return count;
}

static int acm8816_reg_open(struct inode *inode, struct file *file)
{
    file->private_data = inode->i_private;
    return 0;
}

static const struct file_operations acm8816_reg_fops = {
    .owner = THIS_MODULE,
    .open = acm8816_reg_open,
    .read = acm8816_reg_read,
    .write = acm8816_reg_write,
    .llseek = default_llseek,
};

/* 温度读取接口 */
static ssize_t acm8816_temp_read(struct file *file, char __user *buf,
                                size_t count, loff_t *ppos)
{
    struct acm8816_priv *priv = file->private_data;
    char temp[64];
    int temperature;
    
    /* 避免重复读取 */
    if (*ppos > 0) {
        return 0;
    }
    
    /* 获取温度 */
    temperature = acm8816_get_temperature(priv);
    
    /* 格式化输出 */
    scnprintf(temp, sizeof(temp), "Temperature: %d°C\n", temperature);
    
    /* 复制到用户空间 */
    if (copy_to_user(buf, temp, strlen(temp))) {
        return -EFAULT;
    }
    
    *ppos = strlen(temp);
    return *ppos;
}

static const struct file_operations acm8816_temp_fops = {
    .owner = THIS_MODULE,
    .open = acm8816_reg_open,
    .read = acm8816_temp_read,
};

/* PVDD电压读取接口 */
static ssize_t acm8816_pvdd_read(struct file *file, char __user *buf,
                                size_t count, loff_t *ppos)
{
    struct acm8816_priv *priv = file->private_data;
    char temp[64];
    int pvdd;
    
    /* 避免重复读取 */
    if (*ppos > 0) {
        return 0;
    }
    
    /* 获取PVDD电压 */
    pvdd = acm8816_get_pvdd(priv);
    
    /* 格式化输出 */
    scnprintf(temp, sizeof(temp), "PVDD Voltage: %dmV\n", pvdd);
    
    /* 复制到用户空间 */
    if (copy_to_user(buf, temp, strlen(temp))) {
        return -EFAULT;
    }
    
    *ppos = strlen(temp);
    return *ppos;
}

static const struct file_operations acm8816_pvdd_fops = {
    .owner = THIS_MODULE,
    .open = acm8816_reg_open,
    .read = acm8816_pvdd_read,
};

/* GVDD电压读取接口 */
static ssize_t acm8816_gvdd_read(struct file *file, char __user *buf,
                                size_t count, loff_t *ppos)
{
    struct acm8816_priv *priv = file->private_data;
    char temp[64];
    int gvdd;
    
    /* 避免重复读取 */
    if (*ppos > 0) {
        return 0;
    }
    
    /* 获取GVDD电压 */
    gvdd = acm8816_get_gvdd(priv);
    
    /* 格式化输出 */
    scnprintf(temp, sizeof(temp), "GVDD Voltage: %dmV\n", gvdd);
    
    /* 复制到用户空间 */
    if (copy_to_user(buf, temp, strlen(temp))) {
        return -EFAULT;
    }
    
    *ppos = strlen(temp);
    return *ppos;
}

static const struct file_operations acm8816_gvdd_fops = {
    .owner = THIS_MODULE,
    .open = acm8816_reg_open,
    .read = acm8816_gvdd_read,
};

/* NTC电压读取接口 */
static ssize_t acm8816_ntc_read(struct file *file, char __user *buf,
                                size_t count, loff_t *ppos)
{
    struct acm8816_priv *priv = file->private_data;
    char temp[64];
    int ntc_voltage;
    
    /* 避免重复读取 */
    if (*ppos > 0) {
        return 0;
    }
    
    /* 获取NTC电压 */
    ntc_voltage = acm8816_get_ntc_voltage(priv);
    
    /* 格式化输出 */
    scnprintf(temp, sizeof(temp), "NTC Voltage: %dmV\n", ntc_voltage);
    
    /* 复制到用户空间 */
    if (copy_to_user(buf, temp, strlen(temp))) {
        return -EFAULT;
    }
    
    *ppos = strlen(temp);
    return *ppos;
}

static const struct file_operations acm8816_ntc_fops = {
    .owner = THIS_MODULE,
    .open = acm8816_reg_open,
    .read = acm8816_ntc_read,
};


/* 告警状态读取接口 */
static ssize_t acm8816_status_read(struct file *file, char __user *buf,
                                   size_t count, loff_t *ppos)
{
	struct acm8816_priv *priv = file->private_data;
	char *temp; /* 堆上分配的缓冲区，避免栈帧过大 */
	unsigned int state_reg_val, warn_reg_val, fault_reg_val, fault2_reg_val, curr_set_val,temperature,pvdd, gvdd, ntc_voltage;
	int ret = 0, len;
	const size_t buf_size = 4096;
	
	/* 避免重复读取 */
	if (*ppos > 0) {
		return 0;
	}
	
	/* 堆上分配缓冲区 */
	temp = kmalloc(buf_size, GFP_KERNEL);
	if (!temp) {
		return -ENOMEM;
	}
	
	memset(temp, 0, buf_size);
	
	mutex_lock(&priv->lock);
    regmap_write(priv->regmap, 0x00, 0x00);
	/* 读取告警和故障寄存器值 */
	ret = regmap_read(priv->regmap, ACM8816_REG_WARN_RPT, &warn_reg_val);
	if (ret >= 0) {
		ret = regmap_read(priv->regmap, ACM8816_REG_FAULT_RPT1, &fault_reg_val);
	}
	if (ret >= 0) {
		ret = regmap_read(priv->regmap, ACM8816_REG_FAULT_RPT2, &fault2_reg_val);
	}
    if (ret >= 0){  
        ret = regmap_read(priv->regmap, ACM8816_REG_STATE_RPT, &state_reg_val);
    }
    if (ret >= 0){  
        ret = regmap_read(priv->regmap, ACM8816_REG_STATE_CTRL, &curr_set_val);
        curr_set_val &= 0x03;
    }
	mutex_unlock(&priv->lock);
    
    /* 获取温度、PVDD、GVDD、NTC电压 */
    temperature = acm8816_get_temperature(priv);
    pvdd = acm8816_get_pvdd(priv);
    gvdd = acm8816_get_gvdd(priv);
    ntc_voltage = acm8816_get_ntc_voltage(priv);
	
	if (ret < 0) {
		dev_err(&priv->i2c->dev, "Failed to read warning/fault registers: %d\n", ret);
		kfree(temp);
		return ret;
	}
	
	/* 格式化输出告警状态 */
	len = scnprintf(temp, buf_size, "ACM8816 State Warning and Fault Status:\n");
	len += scnprintf(temp + len, buf_size - len, "==========================================================\n");
	if(curr_set_val != state_reg_val){
		len += scnprintf(temp + len, buf_size - len, "Usr Set: \033[31m0x%02x(%s)\033[0m, Current State: \033[31m0x%02x(%s)\033[0m\n"
            , curr_set_val, acm8816_state_names[curr_set_val], state_reg_val, acm8816_state_names[state_reg_val]);
	}else{
        len += scnprintf(temp + len, buf_size - len, "Usr Set: \033[32m0x%02x(%s)\033[0m, Current State: \033[32m0x%02x(%s)\033[0m\n"
            , curr_set_val, acm8816_state_names[curr_set_val], state_reg_val, acm8816_state_names[state_reg_val]);
    }
    len += scnprintf(temp + len, buf_size - len, "[Sensor Data]\n");
    len += scnprintf(temp + len, buf_size - len, "Temperature: %d°C\n", temperature);
    len += scnprintf(temp + len, buf_size - len, "PVDD Voltage: %dmV\n", pvdd);
    len += scnprintf(temp + len, buf_size - len, "GVDD Voltage: %dmV\n", gvdd);
    len += scnprintf(temp + len, buf_size - len, "NTC Voltage: %dmV\n", ntc_voltage);

	/* 警告寄存器部分 */
	len += scnprintf(temp + len, buf_size - len, "[Warning Register (ACM8816_REG_WARN_RPT 0x19)]\n");
	len += scnprintf(temp + len, buf_size - len, "Raw register value: 0x%02x\n", warn_reg_val);
	
	/* 解析位0: OTW (Over temperature warning) */
	len += scnprintf(temp + len, buf_size - len, "OTW (Over Temperature Warning): %s\n",
	               (warn_reg_val & BIT(0)) ? "\033[31mALARM\033[0m" : "\033[32mNORMAL\033[0m");
	
	/* 解析位1: OTW_NTC (Over temperature warning based on external NTC) */
	len += scnprintf(temp + len, buf_size - len, "OTW_NTC (NTC Over Temperature Warning): %s\n",
	               (warn_reg_val & BIT(1)) ? "\033[31mALARM\033[0m" : "\033[32mNORMAL\033[0m");
	
	/* 解析位2: CLIP (Clip warning) */
	len += scnprintf(temp + len, buf_size - len, "CLIP (Clipping Warning): %s\n",
	               (warn_reg_val & BIT(2)) ? "\033[31mALARM\033[0m" : "\033[32mNORMAL\033[0m");
	
	len += scnprintf(temp + len, buf_size - len, "\n\n");
	
	/* 故障寄存器部分 */
	len += scnprintf(temp + len, buf_size - len, "[Fault Register (ACM8816_REG_FAULT_RPT1 0x17)]\n");
	len += scnprintf(temp + len, buf_size - len, "Raw register value: 0x%02x\n", fault_reg_val);
	
	/* 解析位0: OC (Over current fault) */
	len += scnprintf(temp + len, buf_size - len, "OC (Over Current Fault): %s\n",
	               (fault_reg_val & BIT(0)) ? "\033[31mFAULT\033[0m" : "\033[32mNORMAL\033[0m");
	
	/* 解析位1: BST_UV (BST undervoltage fault) */
	len += scnprintf(temp + len, buf_size - len, "BST_UV (BST Undervoltage Fault): %s\n",
	               (fault_reg_val & BIT(1)) ? "\033[31mFAULT\033[0m" : "\033[32mNORMAL\033[0m");
	
	/* 解析位2: AVCC_UV (AVCC undervoltage fault) */
	len += scnprintf(temp + len, buf_size - len, "AVCC_UV (AVCC Undervoltage Fault): %s\n",
	               (fault_reg_val & BIT(2)) ? "\033[31mFAULT\033[0m" : "\033[32mNORMAL\033[0m");
	
	/* 解析位3: AVCC_OV (AVCC overvoltage fault) */
	len += scnprintf(temp + len, buf_size - len, "AVCC_OV (AVCC Overvoltage Fault): %s\n",
	               (fault_reg_val & BIT(3)) ? "\033[31mFAULT\033[0m" : "\033[32mNORMAL\033[0m");
	
	/* 解析位4: PVDD_UV (PVDD undervoltage fault) */
	len += scnprintf(temp + len, buf_size - len, "PVDD_UV (PVDD Undervoltage Fault): %s\n",
	               (fault_reg_val & BIT(4)) ? "\033[31mFAULT\033[0m" : "\033[32mNORMAL\033[0m");
	
	/* 解析位5: PVDD_OV (PVDD overvoltage fault) */
	len += scnprintf(temp + len, buf_size - len, "PVDD_OV (PVDD Overvoltage Fault): %s\n",
	               (fault_reg_val & BIT(5)) ? "\033[31mFAULT\033[0m" : "\033[32mNORMAL\033[0m");
	
	/* 解析位6: OTSD (Over temperature shutdown fault) */
	len += scnprintf(temp + len, buf_size - len, "OTSD (Over Temperature Shutdown Fault): %s\n",
	               (fault_reg_val & BIT(6)) ? "\033[31mFAULT\033[0m" : "\033[32mNORMAL\033[0m");
	
	/* 解析位7: OTSD_NTC (Over temperature shutdown fault based on external NTC) */
	len += scnprintf(temp + len, buf_size - len, "OTSD_NTC (NTC Over Temperature Shutdown Fault): %s\n",
	               (fault_reg_val & BIT(7)) ? "\033[31mFAULT\033[0m" : "\033[32mNORMAL\033[0m");

	len += scnprintf(temp + len, buf_size - len, "\n\n");
	/* 故障寄存器2部分 */
	len += scnprintf(temp + len, buf_size - len, "[Fault Register 2 (ACM8816_REG_FAULT_RPT2 0x18)]\n");
	len += scnprintf(temp + len, buf_size - len, "Raw register value: 0x%02x\n", fault2_reg_val);
	
	/* 解析位2: CLK_FAULT (Clock fault report) */
	len += scnprintf(temp + len, buf_size - len, "CLK_FAULT (Clock Fault): %s\n",
	               (fault2_reg_val & BIT(2)) ? "\033[31mFAULT\033[0m" : "\033[32mNORMAL\033[0m");
	
	/* 解析位3: DC (DC output fault report) */
	len += scnprintf(temp + len, buf_size - len, "DC (DC Output Fault): %s\n",
	               (fault2_reg_val & BIT(3)) ? "\033[31mFAULT\033[0m" : "\033[32mNORMAL\033[0m");
	
	/* 解析位4: CBC_WARN (Cycle-by-cycle current limit warning report) */
	len += scnprintf(temp + len, buf_size - len, "CBC_WARN (Cycle-by-cycle Current Limit Warning): %s\n",
	               (fault2_reg_val & BIT(4)) ? "\033[31mWARNING\033[0m" : "\033[32mNORMAL\033[0m");
	
	/* 解析位5: CBC_FAULT (Cycle-by-cycle current limit fault report) */
	len += scnprintf(temp + len, buf_size - len, "CBC_FAULT (Cycle-by-cycle Current Limit Fault): %s\n",
	               (fault2_reg_val & BIT(5)) ? "\033[31mFAULT\033[0m" : "\033[32mNORMAL\033[0m");
	
	/* 解析位6: GVDD_UV (GVDD undervoltage fault report) */
	len += scnprintf(temp + len, buf_size - len, "GVDD_UV (GVDD Undervoltage Fault): %s\n",
	               (fault2_reg_val & BIT(6)) ? "\033[31mFAULT\033[0m" : "\033[32mNORMAL\033[0m");
	
	/* 解析位7: GVDD_OV (GVDD overvoltage fault report) */
	len += scnprintf(temp + len, buf_size - len, "GVDD_OV (GVDD Overvoltage Fault): %s\n",
	               (fault2_reg_val & BIT(7)) ? "\033[31mFAULT\033[0m" : "\033[32mNORMAL\033[0m");

	len += scnprintf(temp + len, buf_size - len, "==========================================================\n");
	
	/* 复制到用户空间 */
	if (copy_to_user(buf, temp, len)) {
		kfree(temp);
		return -EFAULT;
	}
	
	*ppos = len;
	kfree(temp); /* 释放堆内存 */
	return *ppos;
}


static ssize_t acm8816_dump_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	struct acm8816_priv *priv = file->private_data;
	char *temp;
	int ret = 0, len = 0;
	unsigned int reg_value;
	const size_t buf_size = 4096;
	int i, j;

	/* 避免重复读取 */
	if (*ppos > 0) {
		return 0;
	}

	/* 堆上分配缓冲区 */
	temp = kmalloc(buf_size, GFP_KERNEL);
	if (!temp) {
		return -ENOMEM;
	}

	memset(temp, 0, buf_size);

	/* 打印列标题 (00-0F) */
	len += scnprintf(temp + len, buf_size - len, "   ");
	for (i = 0; i < 16; i++) {
		len += scnprintf(temp + len, buf_size - len, " %02x", i);
	}
	len += scnprintf(temp + len, buf_size - len, "\n");

	mutex_lock(&priv->lock);
	/* 读取并打印寄存器值，从寄存器1到0x35 */
	for (i = 0; i <= 0x35 / 16; i++) {
		/* 打印行号 */
		len += scnprintf(temp + len, buf_size - len, "%02x:", i * 16);

		/* 第一行特殊处理，第一个位置留空 */
		if (i == 0) {
			len += scnprintf(temp + len, buf_size - len, "   ");
			/* 从寄存器1开始打印15个值 */
			for (j = 1; j < 16 && (i * 16 + j) <= 0x35; j++) {
				ret = regmap_read(priv->regmap, i * 16 + j, &reg_value);
				if (ret >= 0) {
					len += scnprintf(temp + len, buf_size - len, " %02x", reg_value);
				} else {
					len += scnprintf(temp + len, buf_size - len, " --");
				}
			}
		} else {
			/* 其他行正常打印16个值 */
			for (j = 0; j < 16 && (i * 16 + j) <= 0x35; j++) {
				ret = regmap_read(priv->regmap, i * 16 + j, &reg_value);
				if (ret >= 0) {
					len += scnprintf(temp + len, buf_size - len, " %02x", reg_value);
				} else {
					len += scnprintf(temp + len, buf_size - len, " --");
				}
			}
		}
		len += scnprintf(temp + len, buf_size - len, "\n");
	}
	mutex_unlock(&priv->lock);

	/* 复制到用户空间 */
	if (copy_to_user(buf, temp, len)) {
		kfree(temp);
		return -EFAULT;
	}

	*ppos = len;
	kfree(temp);
	return *ppos;
}

static const struct file_operations acm8816_dump_fops = {
	.owner = THIS_MODULE,
	.open = acm8816_reg_open,
	.read = acm8816_dump_read,
};

/* 告警清除写入函数 */
static ssize_t acm8816_warn_clear_write(struct file *file, const char __user *buf,
                                  size_t count, loff_t *ppos)
{
	struct acm8816_priv *priv = file->private_data;
	int ret;
	
	mutex_lock(&priv->lock);
	/* 写入ACM8816_REG_AMP_CTRL1寄存器的bit7来清除告警 */
	ret = regmap_update_bits(priv->regmap, ACM8816_REG_AMP_CTRL1,
	                       ACM8816_AMP_CTRL1_FAULT_CLR_MASK,
	                       ACM8816_AMP_CTRL1_FAULT_CLR_MASK);
	mutex_unlock(&priv->lock);
	
	if (ret < 0) {
		dev_err(&priv->i2c->dev, "Failed to clear warnings: %d\n", ret);
		return ret;
	}
	
	return count;
}

static const struct file_operations acm8816_warn_clear_fops = {
	.owner = THIS_MODULE,
	.open = acm8816_reg_open,
	.write = acm8816_warn_clear_write,
};


/* README文件内容 */
static const char acm8816_readme_content[] = 
"ACM8816 Debugfs Interface\n"
"----------------------------\n"
"This directory provides debugging interfaces for the ACM8816 codec.\n"
"\n"
"Available files:\n"
"- regs: Read/Write registers directly (format: \"reg_addr\" to set address, \"reg_addr reg_val\" for write)\n"
"- temp: Read temperature sensor value (in Celsius)\n"
"- pvdd: Read PVDD voltage (in volts)\n"
"- gvdd: Read GVDD voltage (in volts)\n"
"- ntc: Read NTC voltage (in volts)\n"
"- status: Read status warning fault registers\n"
"- warnClear: Clear all warnings and faults\n"
"- mute: Control DAC mute (0 for unmute, 1 for mute)\n"
"- sample_rate: Read actual sample rate detected from I2S line\n"
"- dump: Dump register values from 0x01 to 0x35 in i2cdump format\n"
"\n"
"Example usage:\n"
"1. Read register 0x01 in one command:\n"
"   echo \"0x01\" > /sys/kernel/debug/acm8816/regs && cat /sys/kernel/debug/acm8816/regs\n"
"\n"
"2. Write to register 0x01:\n"
"   echo \"0x01 0x80\" > /sys/kernel/debug/acm8816/regs\n"
"\n"
"3. Read temperature:\n"
"   cat /sys/kernel/debug/acm8816/temp\n"
"\n"
"4. Read PVDD voltage:\n"
"   cat /sys/kernel/debug/acm8816/pvdd\n"
"\n"
"5. Read GVDD voltage:\n"
"   cat /sys/kernel/debug/acm8816/gvdd\n"
"\n"
"6. Read NTC voltage:\n"
"   cat /sys/kernel/debug/acm8816/ntc\n"
"\n"
"7. Read status warning fault registers:\n"
"   cat /sys/kernel/debug/acm8816/status\n"
"\n"
"8. Clear all warnings and faults:\n"
"    echo 1 > /sys/kernel/debug/acm8816/warnClear\n"
"\n"
"9. Read actual sample rate from I2S line:\n"
"    cat /sys/kernel/debug/acm8816/sample_rate\n"
"\n"
"Note: Be careful when writing to registers as it may affect the codec's behavior.\n";

static ssize_t acm8816_readme_read(struct file *file, char __user *buf,
                                  size_t count, loff_t *ppos)
{
    return simple_read_from_buffer(buf, count, ppos, acm8816_readme_content,
                                  sizeof(acm8816_readme_content) - 1);
}

/* 采样率读取函数 */
static ssize_t acm8816_sample_rate_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    struct acm8816_priv *priv = file->private_data;
    unsigned int reg_val;
    int ret;
    char sample_rate_str[64];
    int rate = 0;

    /* 只允许单次读取 */
    if (*ppos != 0)
        return 0;

    /* 读取I2S时钟格式报告寄存器 */
    mutex_lock(&priv->lock);
    ret = regmap_read(priv->regmap, ACM8816_REG_I2S_CLK_FORMAT_RPT1, &reg_val);
    mutex_unlock(&priv->lock);

    if (ret < 0) {
        dev_err(&priv->i2c->dev, "Failed to read sample rate register: %d\n", ret);
        return ret;
    }

    /* 提取低4位的采样率信息 */
    reg_val &= ACM8816_REG_I2S_CLK_FORMAT_RPT1_SAMPLE_RATE;

    /* 根据寄存器值映射到实际采样率 */
    switch (reg_val) {
        case 0x06: rate = 32000; break;
        case 0x08: rate = 44100; break;
        case 0x09: rate = 48000; break;
        case 0x0A: rate = 88200; break;
        case 0x0B: rate = 96000; break;
        case 0x0D: rate = 192000; break;
        default: rate = 0; /* 未知采样率 */
    }

    /* 格式化采样率信息 */
    if (rate > 0) {
        snprintf(sample_rate_str, sizeof(sample_rate_str), "%d Hz\n", rate);
    } else {
        snprintf(sample_rate_str, sizeof(sample_rate_str), "Unknown sample rate (0x%x)\n", reg_val);
    }

    /* 复制到用户空间 */
    ret = simple_read_from_buffer(buf, count, ppos, sample_rate_str, strlen(sample_rate_str));
    if (ret > 0)
        *ppos += ret;

    return ret;
}

static const struct file_operations acm8816_sample_rate_fops = {
    .owner = THIS_MODULE,
    .open = acm8816_reg_open,
    .read = acm8816_sample_rate_read,
};

static const struct file_operations acm8816_readme_fops = {
    .owner = THIS_MODULE,
    .read = acm8816_readme_read,
};

static const struct file_operations acm8816_status_fops = {
	.owner = THIS_MODULE,
	.open = acm8816_reg_open,
	.read = acm8816_status_read,
};

/* 创建debugfs接口 */
static int acm8816_debugfs_create(struct acm8816_priv *priv)
{
    
    /* 创建debugfs目录 */
    priv->debugfs_dir = debugfs_create_dir("acm8816", NULL);
    if (!priv->debugfs_dir) {
        dev_err(&priv->i2c->dev, "Failed to create debugfs directory\n");
        return -ENOMEM;
    }
    
    /* 创建寄存器读写接口文件 */
    priv->debugfs_regs = debugfs_create_file("regs", 0644, priv->debugfs_dir, priv, &acm8816_reg_fops);
    if (!priv->debugfs_regs) {
        dev_err(&priv->i2c->dev, "Failed to create regs file\n");
        debugfs_remove_recursive(priv->debugfs_dir);
        return -ENOMEM;
    }
    
    /* 创建温度读取文件 */
    priv->debugfs_temp = debugfs_create_file("temperature", 0444, priv->debugfs_dir, priv, &acm8816_temp_fops);
    if (!priv->debugfs_temp) {
        dev_err(&priv->i2c->dev, "Failed to create temp file\n");
        debugfs_remove_recursive(priv->debugfs_dir);
        return -ENOMEM;
    }
    
    /* 创建PVDD电压读取文件 */
    priv->debugfs_pvdd = debugfs_create_file("pvdd", 0444, priv->debugfs_dir, priv, &acm8816_pvdd_fops);
    if (!priv->debugfs_pvdd) {
        dev_err(&priv->i2c->dev, "Failed to create pvdd file\n");
        debugfs_remove_recursive(priv->debugfs_dir);
        return -ENOMEM;
    }
    
    /* 创建GVDD电压读取文件 */
    priv->debugfs_gvdd = debugfs_create_file("gvdd", 0444, priv->debugfs_dir, priv, &acm8816_gvdd_fops);
    if (!priv->debugfs_gvdd) {
        dev_err(&priv->i2c->dev, "Failed to create gvdd file\n");
        debugfs_remove_recursive(priv->debugfs_dir);
        return -ENOMEM;
    }
    
    /* 创建NTC电压读取文件 */
    priv->debugfs_ntc = debugfs_create_file("ntc", 0444, priv->debugfs_dir, priv, &acm8816_ntc_fops);
    if (!priv->debugfs_ntc) {
        dev_err(&priv->i2c->dev, "Failed to create ntc file\n");
        debugfs_remove_recursive(priv->debugfs_dir);
        return -ENOMEM;
    }
    
    /* 创建告警状态读取文件 */
    priv->debugfs_status = debugfs_create_file("status", 0444, priv->debugfs_dir, priv, &acm8816_status_fops);
    if (!priv->debugfs_status) {
        dev_err(&priv->i2c->dev, "Failed to create status file\n");
        debugfs_remove_recursive(priv->debugfs_dir);
        return -ENOMEM;
    }
    
    /* 创建告警清除写入文件 */
    priv->debugfs_warn_clear = debugfs_create_file("warnClear", 0200, priv->debugfs_dir, priv, &acm8816_warn_clear_fops);
    if (!priv->debugfs_warn_clear) {
        dev_err(&priv->i2c->dev, "Failed to create warnClear file\n");
        debugfs_remove_recursive(priv->debugfs_dir);
        return -ENOMEM;
    }
    
    /* 创建采样率读取文件 */
    priv->debugfs_sample_rate = debugfs_create_file("sample_rate", 0444, priv->debugfs_dir, priv, &acm8816_sample_rate_fops);
    if (!priv->debugfs_sample_rate) {
        dev_err(&priv->i2c->dev, "Failed to create sample_rate file\n");
        debugfs_remove_recursive(priv->debugfs_dir);
        return -ENOMEM;
    }
    
    /* 创建寄存器dump文件 */
    priv->debugfs_dump = debugfs_create_file("dump", 0444, priv->debugfs_dir, priv, &acm8816_dump_fops);
    if (!priv->debugfs_dump) {
        dev_err(&priv->i2c->dev, "Failed to create dump file\n");
        debugfs_remove_recursive(priv->debugfs_dir);
        return -ENOMEM;
    }

    /* 创建README文件，说明如何使用 */
    priv->debugfs_readme = debugfs_create_file("README", 0444, priv->debugfs_dir, (void *)acm8816_readme_content, &acm8816_readme_fops);
    if (!priv->debugfs_readme) {
        dev_err(&priv->i2c->dev, "Failed to create README file\n");
        debugfs_remove_recursive(priv->debugfs_dir);
        return -ENOMEM;
    }
    
    dev_info(&priv->i2c->dev, "Debugfs interfaces created successfully\n");
    return 0;
}

/* 清理debugfs接口 */
static void acm8816_debugfs_remove(struct acm8816_priv *priv)
{
    if (priv->debugfs_dir) {
        debugfs_remove_recursive(priv->debugfs_dir);
        priv->debugfs_dir = NULL;
        dev_info(&priv->i2c->dev, "Debugfs interfaces removed\n");
    }
}

/* 设置DAC音量 */
static int acm8816_set_dac_volume(struct snd_soc_component *component, int reg, int channel, int volume)
{
    struct acm8816_priv *priv = snd_soc_component_get_drvdata(component);
    int ret = 0;
    unsigned int digital_gain = ACM8816_VOLUME_CTRL_VOL_DEFAULT;
    unsigned int analog_gain = ACM8816_AMP_CTRL2_ANA_GAIN_MIN;  // 最小模拟增益（默认值）
    
    /* 音量范围检查：0-49对应50个档位 */
    if (volume < 0 || volume > 49) {
        dev_err(&priv->i2c->dev, "Invalid volume value: %d (must be 0-49)\n", volume);
        return -EINVAL;
    }

    /* 音量计算算法：前19档控制数字增益，后31档控制模拟增益 */
    if (volume < 19) {
        /* 数字增益范围：从最小值到默认值，19个档位 */
        digital_gain = ACM8816_VOLUME_CTRL_VOL_DEFAULT - (19 - volume);
        analog_gain = ACM8816_AMP_CTRL2_ANA_GAIN_MIN;  // 模拟增益保持最小值
    } else {
        /* 模拟增益范围：从最小值到最大值，31个档位 */
        digital_gain = ACM8816_VOLUME_CTRL_VOL_DEFAULT;  // 数字增益保持默认值
        analog_gain = ACM8816_AMP_CTRL2_ANA_GAIN_MIN - (volume - 19);
    }

    dev_info(&priv->i2c->dev, "Setting DAC volume: %d, digital_gain: 0x%x, analog_gain: 0x%x\n", 
             volume, digital_gain, analog_gain);
    
    mutex_lock(&priv->lock);
    
    /* 写入数字增益寄存器 */
    regmap_write(priv->regmap, 0x00, 0x00);
    ret = regmap_write(priv->regmap, ACM8816_REG_VOLUME_CTRL, digital_gain);
    if (ret < 0) {
        dev_err(&priv->i2c->dev, "Failed to write digital gain: %d\n", ret);
        mutex_unlock(&priv->lock);
        return ret;
    }
    
    /* 写入模拟增益寄存器 */
    ret = regmap_update_bits(priv->regmap, ACM8816_REG_AMP_CTRL2, 
                           ACM8816_AMP_CTRL2_ANA_GAIN_MASK, analog_gain);
    if (ret < 0) {
        dev_err(&priv->i2c->dev, "Failed to write analog gain: %d\n", ret);
        mutex_unlock(&priv->lock);
        return ret;
    }
    
    mutex_unlock(&priv->lock);
    
    return ret;
}

/* 获取DAC音量 */
static int acm8816_get_dac_volume(struct snd_soc_component *component, int reg, int channel)
{
    struct acm8816_priv *priv = snd_soc_component_get_drvdata(component);
    int volume = 0;
    unsigned int digital_gain = ACM8816_VOLUME_CTRL_VOL_DEFAULT;
    unsigned int analog_gain = ACM8816_AMP_CTRL2_ANA_GAIN_MIN;
    int ret = 0;
    
    mutex_lock(&priv->lock);
    
    /* 读取数字增益寄存器 */
    regmap_write(priv->regmap, 0x00, 0x00);
    ret = regmap_read(priv->regmap, ACM8816_REG_VOLUME_CTRL, &digital_gain);
    if (ret < 0) {
        dev_err(&priv->i2c->dev, "Failed to read digital gain: %d\n", ret);
        mutex_unlock(&priv->lock);
        return ret;
    }
    
    /* 读取模拟增益寄存器 */
    ret = regmap_read(priv->regmap, ACM8816_REG_AMP_CTRL2, &analog_gain);
    if (ret < 0) {
        dev_err(&priv->i2c->dev, "Failed to read analog gain: %d\n", ret);
        mutex_unlock(&priv->lock);
        return ret;
    }
    
    /* 提取模拟增益位 */
    analog_gain &= ACM8816_AMP_CTRL2_ANA_GAIN_MASK;
    
    /* 根据数字增益和模拟增益计算音量档位 - 与set函数逻辑完全匹配 */
    if (analog_gain == ACM8816_AMP_CTRL2_ANA_GAIN_MIN) {
        /* 前19挡：只使用数字增益控制 */
        volume = ACM8816_VOLUME_CTRL_VOL_DEFAULT - digital_gain;
        /* 确保音量在0-18范围内 */
        if (volume < 0)
            volume = 0;
        else if (volume > 18)
            volume = 18;
    } else {
        /* 后31挡：只使用模拟增益控制，数字增益保持默认值 */
        volume = 19 + (ACM8816_AMP_CTRL2_ANA_GAIN_MIN - analog_gain);
        /* 确保音量在19-49范围内 */
        if (volume < 19)
            volume = 19;
        else if (volume > 49)
            volume = 49;
    }
    
    mutex_unlock(&priv->lock);
    
    dev_info(&priv->i2c->dev, "Get DAC volume: %d, digital_gain: 0x%x, analog_gain: 0x%x\n", 
             volume, digital_gain, analog_gain);
    
    return volume;
}

/* SOC_SINGLE_EXT_TLV宏所需的包装函数 */
static int acm8816_get_dac_volume_kctl(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
    struct acm8816_priv *priv = snd_soc_component_get_drvdata(component);
    int volume;
    int ret;
    
    ret = acm8816_get_dac_volume(component, ACM8816_REG_VOLUME_CTRL, 0);
    if (ret < 0) {
        dev_err(&priv->i2c->dev, "Failed to get DAC volume: %d\n", ret);
        return ret;
    }
    
    volume = ret;
    ucontrol->value.integer.value[0] = volume;
    
    return 0;
}

static int acm8816_set_dac_volume_kctl(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
    int volume = ucontrol->value.integer.value[0];
    
    return acm8816_set_dac_volume(component, ACM8816_REG_VOLUME_CTRL, 0, volume);
}

/* 设置DAC静音 */
static int acm8816_set_dac_mute(struct snd_soc_component *component, int reg, int channel, bool mute)
{
    struct acm8816_priv *priv = snd_soc_component_get_drvdata(component);
    int ret = 0;
    
    dev_info(&priv->i2c->dev, "Setting DAC mute to: %s\n", mute ? "on" : "off");
    
    mutex_lock(&priv->lock);
    priv->muted = mute;
    regmap_write(priv->regmap, 0x00, 0x00);
    if (mute) {
        /* 静音 - 写入最小音量(0x00) */
        ret = regmap_update_bits(priv->regmap, ACM8816_REG_STATE_CTRL,
                                 ACM8816_STATE_CTRL_MUTE_MASK, ACM8816_STATE_CTRL_MUTE_ON);
    } else {
        /* 取消静音 - 恢复之前的音量设置
         * 注意：这里需要保存之前的音量值，当前实现简化处理
         */
        ret = regmap_update_bits(priv->regmap, ACM8816_REG_STATE_CTRL,
                                 ACM8816_STATE_CTRL_MUTE_MASK, ACM8816_STATE_CTRL_MUTE_OFF);
    }
    
    if (ret) {
        dev_err(&priv->i2c->dev, "Failed to set DAC mute: %d\n", ret);
    }
    mutex_unlock(&priv->lock);
    
    return ret;
}


static int acm8816_get_dac_mute(struct snd_soc_component *component, int reg, int channel)
{
    struct acm8816_priv *priv = snd_soc_component_get_drvdata(component);
    int ret = 0;
    unsigned int state_reg;
    bool is_muted;
    
    mutex_lock(&priv->lock);
    regmap_write(priv->regmap, 0x00, 0x00);
    /* 读取状态控制寄存器 */
    ret = regmap_read(priv->regmap, ACM8816_REG_STATE_CTRL, &state_reg);
    if (ret < 0) {
        dev_err(&priv->i2c->dev, "Failed to read state control register: %d\n", ret);
        mutex_unlock(&priv->lock);
        return ret;
    }
    
    /* 检查静音位 */
    is_muted = (state_reg & ACM8816_STATE_CTRL_MUTE_MASK) ? true : false;
    
    /* 更新私有数据中的静音状态以保持同步 */
    priv->muted = is_muted;
    
    dev_info(&priv->i2c->dev, "Current DAC mute state: %s\n", is_muted ? "muted" : "unmuted");
    
    mutex_unlock(&priv->lock);
    return is_muted;
}

/* Wrapper function for get_dac_mute to match SOC_SINGLE_EXT signature */
static int acm8816_get_dac_mute_kctl(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
    int mute_val = acm8816_get_dac_mute(component, 0, 0);
    
    if (mute_val < 0)
        return mute_val;
    
    ucontrol->value.integer.value[0] = mute_val;
    return 0;
}

/* Wrapper function for set_dac_mute to match SOC_SINGLE_EXT signature */
static int acm8816_set_dac_mute_kctl(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
    int mute_val = ucontrol->value.integer.value[0];
    
    return acm8816_set_dac_mute(component, 0, 0, mute_val);
}

/* SoC控件定义 */
static const struct snd_kcontrol_new acm8816_snd_controls[] = {
    /* DAC音量控制 */
    SOC_SINGLE_EXT("Playback Volume", 0, 0, 49, 0,
                      acm8816_get_dac_volume_kctl, acm8816_set_dac_volume_kctl),
    
    /* DAC静音控制 */
    SOC_SINGLE_EXT("Playback Switch", 0, 0, 1, 1,
                  acm8816_get_dac_mute_kctl, acm8816_set_dac_mute_kctl),
};

/* 组件驱动操作 - 使用Linux 6.1.55版本的component API */
static const struct snd_soc_component_driver acm8816_component_driver = {
    .name = "acm8816-codec",
    .endianness = 1,
    .legacy_dai_naming = 0,
    .set_bias_level = acm8816_set_bias_level,
    .controls = acm8816_snd_controls,
    .num_controls = ARRAY_SIZE(acm8816_snd_controls),
    .dapm_widgets = acm8816_dapm_widgets,
    .num_dapm_widgets = ARRAY_SIZE(acm8816_dapm_widgets),
    .dapm_routes = acm8816_dapm_routes,
    .num_dapm_routes = ARRAY_SIZE(acm8816_dapm_routes),
    /* 在Linux 6.1.55中，component驱动没有bias_level和bias_level_post成员
     * 偏置电平初始化通过dapm系统处理
     */
};

static int acm8816_i2c_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
    struct acm8816_priv *priv;
    int ret;
    
    dev_info(&i2c->dev, "Enter acm8816_i2c_probe - device name: %s\n", id ? id->name : "unknown");
    
    /* 分配私有数据结构 */
    priv = devm_kzalloc(&i2c->dev, sizeof(*priv), GFP_KERNEL);
    if (!priv) {
        return -ENOMEM;
    }
    
    /* 初始化必要的数据结构以避免空指针 */
    priv->i2c = i2c;
    mutex_init(&priv->lock);
    i2c_set_clientdata(i2c, priv);
    
    /* 初始化寄存器地址，默认为0x00 */
    priv->last_reg_addr = 0x00;
    priv->file_pos = 0;
    
    /* 初始化regmap */
    priv->regmap = devm_regmap_init_i2c(i2c, &acm8816_regmap_config);
    if (IS_ERR(priv->regmap)) {
        ret = PTR_ERR(priv->regmap);
        dev_err(&i2c->dev, "Failed to allocate regmap: %d\n", ret);
        return ret;
    }
    // msleep(10);
    // for (int i = 0; i < ARRAY_SIZE(acm8816_dsp_reg_defaults); i++) {
    //     if (acm8816_dsp_reg_defaults[i].reg != 0x00 || acm8816_dsp_reg_defaults[i].def != 0x00) {
    //         regmap_write(priv->regmap, acm8816_dsp_reg_defaults[i].reg, 
    //                     acm8816_dsp_reg_defaults[i].def);
    //         dev_info(&priv->i2c->dev, "  Writing reg 0x%02x = 0x%02x\n",
    //                 acm8816_dsp_reg_defaults[i].reg, acm8816_dsp_reg_defaults[i].def);
    //     }
    // }
    /* 注册组件 - 使用Linux 6.1.55版本的component API */
    ret = devm_snd_soc_register_component(&i2c->dev, &acm8816_component_driver, &acm8816_dai, 1);
    if (ret < 0) {
        dev_err(&i2c->dev, "Failed to register component: %d\n", ret);
        return ret;
    }else{
        dev_info(&i2c->dev, "Successfully registered component\n");
    }
    
    /* 创建debugfs接口 */
    ret = acm8816_debugfs_create(priv);
    if (ret < 0) {
        dev_warn(&i2c->dev, "Failed to create debugfs interfaces: %d\n", ret);
        /* 继续执行，因为debugfs接口是可选的 */
    }
    
    /* 启用温度和电压采样功能 */
    ret = acm8816_enable_sensing(priv);
    if (ret < 0) {
        dev_err(&i2c->dev, "Failed to enable sensing: %d\n", ret);
        /* 非致命错误，继续运行但记录错误 */
    }
    
    return 0;
}

/* I2C移除函数 - 使用devm函数时资源会自动释放 */
static void acm8816_i2c_remove(struct i2c_client *i2c)
{
    struct acm8816_priv *priv = i2c_get_clientdata(i2c);
    
    dev_info(&i2c->dev, "Enter acm8816_i2c_remove\n");
    
    /* 清理debugfs接口 */
    acm8816_debugfs_remove(priv);
}

/* I2C设备ID表 */
static const struct i2c_device_id acm8816_id[] = {
    { "acm8816", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, acm8816_id);

/* I2C驱动结构 */
static struct i2c_driver acm8816_i2c_driver = {
    .driver = {
        .name = "acm8816",
        .owner = THIS_MODULE,
    },
    .probe = acm8816_i2c_probe,
    .remove = acm8816_i2c_remove,
    .id_table = acm8816_id,
};

module_i2c_driver(acm8816_i2c_driver);

MODULE_DESCRIPTION("ACM8816 ALSA SoC audio CODEC driver");
MODULE_AUTHOR("solid");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("i2c:acm8816");