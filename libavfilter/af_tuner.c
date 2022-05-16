/*
 * Copyright (c) 2012 Michael Niedermayer
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file
 * audio tuner filter.
 *
 * Based on af_aresample.c
 */
#include <stdatomic.h>
#include "libavutil/avstring.h"
#include "libavutil/channel_layout.h"
#include "libavutil/opt.h"
#include "libavutil/samplefmt.h"
#include "libavutil/avassert.h"
#include "avfilter.h"
#include "audio.h"
#include "internal.h"
#include "soxWrapper.h"

typedef struct ATunerContext {
    const AVClass *class;
    int channels;
    int sample_rate;
	int revb_percent; // 混响密度
	int revb_hfdamping; //混响干湿比例
	int revb_roomscale; //混响空间大小
	int revb_depth;  //混响立体声深度
	int revb_predelay;	 //混响前置延迟时间
    int total_gain;  // 总增益值，范围为-10~+10，如果有具体需求，以正式需求为准
    int eq_pattern; // 均衡风格
    int reverb_pattern; // 混响模式
    int eq_band0;
    int eq_band1;
    int eq_band2;
    int eq_band3;
    int eq_band4;
    int eq_band5;
    int eq_band6;
    int eq_band7;
    int eq_band8;
    int eq_band9;
    int switch_flag_before_tune;  // 在进行调音前进行声道互换标识,  0:什么也不做;  1:左右声道互换;  2:将右声道填充为左声道数据; 3:将左声道填充为右声道数据 ;
    int switch_flag_after_tune;   // 在进行调音后进行声道互换标识,  0:什么也不做;  1:左右声道互换;  2:将右声道填充为左声道数据; 3:将左声道填充为右声道数据 ;
    void *l_handler;
} ATunerContext;

#define OFFSET(x) offsetof(ATunerContext, x)
#define A AV_OPT_FLAG_AUDIO_PARAM|AV_OPT_FLAG_FILTERING_PARAM

static const AVOption tuner_options[] = {
    { "revb_percent",            "revb percent",       OFFSET(revb_percent),                  AV_OPT_TYPE_INT,   { .i64 = 0 }, 0,       INT_MAX, A },
    { "revb_hfdamping",          "revb hfdamping",     OFFSET(revb_hfdamping),                AV_OPT_TYPE_INT,   { .i64 = 0 }, 0,       INT_MAX, A },
    { "revb_roomscale",          "revb roomscale",     OFFSET(revb_roomscale),                AV_OPT_TYPE_INT,   { .i64 = 0 }, 0,       INT_MAX, A },
    { "revb_depth",              "revb depth",         OFFSET(revb_depth),                    AV_OPT_TYPE_INT,   { .i64 = 0 }, 0,       INT_MAX, A },
    { "revb_predelay",           "revb predelay",      OFFSET(revb_predelay),                 AV_OPT_TYPE_INT,   { .i64 = 0 }, 0,       INT_MAX, A },
    { "total_gain",              "total gain",         OFFSET(total_gain),                    AV_OPT_TYPE_INT,   { .i64 = 0 }, INT_MIN, INT_MAX, A },
    { "eq_pattern",              "equalizer pattern",  OFFSET(eq_pattern),                    AV_OPT_TYPE_INT,   { .i64 = 0 }, 0,       INT_MAX, A },
    { "reverb_pattern",          "reverb pattern",     OFFSET(reverb_pattern),                AV_OPT_TYPE_INT,   { .i64 = 0 }, 0,       INT_MAX, A },
    { "band0",                   "gain value 0",       OFFSET(eq_band0),                      AV_OPT_TYPE_INT,   { .i64 = 0 }, INT_MIN, INT_MAX, A },
    { "band1",                   "gain value 1",       OFFSET(eq_band1),                      AV_OPT_TYPE_INT,   { .i64 = 0 }, INT_MIN, INT_MAX, A },
    { "band2",                   "gain value 2",       OFFSET(eq_band2),                      AV_OPT_TYPE_INT,   { .i64 = 0 }, INT_MIN, INT_MAX, A },
    { "band3",                   "gain value 3",       OFFSET(eq_band3),                      AV_OPT_TYPE_INT,   { .i64 = 0 }, INT_MIN, INT_MAX, A },
    { "band4",                   "gain value 4",       OFFSET(eq_band4),                      AV_OPT_TYPE_INT,   { .i64 = 0 }, INT_MIN, INT_MAX, A },
    { "band5",                   "gain value 5",       OFFSET(eq_band5),                      AV_OPT_TYPE_INT,   { .i64 = 0 }, INT_MIN, INT_MAX, A },
    { "band6",                   "gain value 6",       OFFSET(eq_band6),                      AV_OPT_TYPE_INT,   { .i64 = 0 }, INT_MIN, INT_MAX, A },
    { "band7",                   "gain value 7",       OFFSET(eq_band7),                      AV_OPT_TYPE_INT,   { .i64 = 0 }, INT_MIN, INT_MAX, A },
    { "band8",                   "gain value 8",       OFFSET(eq_band8),                      AV_OPT_TYPE_INT,   { .i64 = 0 }, INT_MIN, INT_MAX, A },
    { "band9",                   "gain value 9",       OFFSET(eq_band9),                      AV_OPT_TYPE_INT,   { .i64 = 0 }, INT_MIN, INT_MAX, A },
    { "switch_flag_before_tune", "switch flag before", OFFSET(switch_flag_before_tune),       AV_OPT_TYPE_INT,   { .i64 = 0 }, 0,       INT_MAX, A },
    { "switch_flag_after_tune",  "switch flag after",  OFFSET(switch_flag_after_tune),        AV_OPT_TYPE_INT,   { .i64 = 0 }, 0,       INT_MAX, A },
    { "channels",                "channels",           OFFSET(channels),                      AV_OPT_TYPE_INT,   { .i64 = 0 }, 0,       INT_MAX, A },
    { "sample_rate",             "sample rate",        OFFSET(sample_rate),                   AV_OPT_TYPE_INT,   { .i64 = 0 }, 0,       INT_MAX, A },
    { NULL }
};

AVFILTER_DEFINE_CLASS(tuner);

static av_cold int init(AVFilterContext *ctx)
{
    static int seq = 1;
    AudioProfile profile={0};
    int frame_len;
    ATunerContext *s = ctx->priv;

    if (s->l_handler != NULL) {
       av_log(ctx, AV_LOG_ERROR, "s->l_handler is not null, uninit it first\n");
       ATunerContext *s = ctx->priv;
       //clear resource
       terminate_AW_API(s->l_handler);
       s->l_handler = NULL;
       av_log(ctx, AV_LOG_INFO,"uninit tuner resource before init success\n");
    }

    if (s->l_handler == NULL) {
            av_log(ctx, AV_LOG_INFO,
            "\n"
            "config_input param is :\n"
            "channel:    %d\n"
            "sampleRate:   %d\n"
    	    "revb_percent: %d\n"
    	    "revb_hfdamping: %d\n"
    	    "revb_roomscale: %d\n"
    	    "revb_depth: %d\n"
    	    "revb_predelay: %d\n"
            "reverb_pattern: %d\n"
            "eq_pattern: %d\n"
            "eq_profile:%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n"
            "total_gain: %d\n"
            "switch_flag_before_tune: %d\n"
            "switch_flag_after_tune: %d\n",
            s->channels,s->sample_rate,s->revb_percent,s->revb_hfdamping,
            s->revb_roomscale,s->revb_depth,s->revb_predelay,s->reverb_pattern,s->eq_pattern,
            s->eq_band0,s->eq_band1,s->eq_band2,s->eq_band3,s->eq_band4,
            s->eq_band5,s->eq_band6,s->eq_band7,s->eq_band8,s->eq_band9,
            s->total_gain,s->switch_flag_before_tune,s->switch_flag_after_tune);

            int msg_id = atomic_fetch_add(&seq, 3);
            if (msg_id == 0 || msg_id == 0xFFFFFFFF) {
                msg_id = atomic_fetch_add(&seq, 3);
            }

            profile.sampleRate = s->sample_rate;
    	    profile.chanNum = s->channels;
    	    profile.bitsDepth = 16;
    	    profile.frameLen = 1200*s->channels;
    	    profile.wavlen = profile.frameLen+100;
    	    profile.msg_snd_id = msg_id;
    	    profile.msg_rcv_id = msg_id+1;
    	    profile.opt_rcv_id = msg_id+2;

    	    profile.revb_percent = s->revb_percent;
    	    profile.revb_hfdamping = s->revb_hfdamping;
    	    profile.revb_roomscale = s->revb_roomscale;
    	    profile.revb_depth = s->revb_depth;
    	    profile.revb_predelay = s->revb_predelay;
    	    profile.reverb_pattern = s->reverb_pattern;

            profile.eq_pattern = s->eq_pattern;
    	    profile.total_gain = s->total_gain;
            profile.eq_profile[0] = s->eq_band0;
            profile.eq_profile[1] = s->eq_band1;
            profile.eq_profile[2] = s->eq_band2;
            profile.eq_profile[3] = s->eq_band3;
            profile.eq_profile[4] = s->eq_band4;
            profile.eq_profile[5] = s->eq_band5;
            profile.eq_profile[6] = s->eq_band6;
            profile.eq_profile[7] = s->eq_band7;
            profile.eq_profile[8] = s->eq_band8;
            profile.eq_profile[9] = s->eq_band9;

            s->l_handler = init_AW_API(&profile);
            if (s->l_handler == NULL) {
                av_log(ctx, AV_LOG_INFO,"init tuner resource failed\n");
                return -1;
            }
            av_log(ctx, AV_LOG_INFO,"init tuner resource success\n");
        }

    return 0;
}

static av_cold void uninit(AVFilterContext *ctx)
{
    ATunerContext *s = ctx->priv;
    //clear resource
    terminate_AW_API(s->l_handler);
    s->l_handler = NULL;
    av_log(ctx, AV_LOG_INFO,"uninit tuner resource success\n");
}

static void Int2ShortCpy(int16_t * dst, int * src, int len)
{
	int i=0;
	double temp=0;
	if (len==0 || dst==NULL || src== NULL)
		return;
	for (i=0;i<len;i++)
	{
		temp = src[i]/2147483647.99;
		dst[i] = (int16_t)(temp*32767);
	}
}

static int filter_frame(AVFilterLink *inlink, AVFrame *in)
{
    AVFilterContext *ctx = inlink->dst;
    ATunerContext *s = ctx->priv;
    AVFilterLink *outlink = ctx->outputs[0];
    AVFrame *out = NULL;
    AudioProfile profile={0};
    int *data = NULL;
    int frame_len,ret;
    static int seq = 1;

    if (s->l_handler != NULL) {
        if (s->channels != in->channels || s->sample_rate != in->sample_rate) {
            terminate_AW_API(s->l_handler);
            s->l_handler = NULL;
            av_log(ctx, AV_LOG_INFO,"channels or sample_rate change uninit tuner\n");
        }
    }

   if (s->l_handler == NULL) {
        av_log(ctx, AV_LOG_INFO,
        "\n"
        "reinit turner :\n"
        "config_input param is :\n"
        "channel:    %d\n"
        "sampleRate:   %d\n"
	    "revb_percent: %d\n"
	    "revb_hfdamping: %d\n"
	    "revb_roomscale: %d\n"
	    "revb_depth: %d\n"
	    "revb_predelay: %d\n"
        "reverb_pattern: %d\n"
        "eq_pattern: %d\n"
        "eq_profile:%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n"
        "total_gain: %d\n"
        "switch_flag_before_tune: %d\n"
        "switch_flag_after_tune: %d\n",
        in->channels,in->sample_rate,s->revb_percent,s->revb_hfdamping,
        s->revb_roomscale,s->revb_depth,s->revb_predelay,s->reverb_pattern,s->eq_pattern,
        s->eq_band0,s->eq_band1,s->eq_band2,s->eq_band3,s->eq_band4,
        s->eq_band5,s->eq_band6,s->eq_band7,s->eq_band8,s->eq_band9,
        s->total_gain,s->switch_flag_before_tune,s->switch_flag_after_tune);

        int msg_id = atomic_fetch_add(&seq, 3);
        if (msg_id == 0 || msg_id == 0xFFFFFFFF) {
            msg_id = atomic_fetch_add(&seq, 3);
        }

        s->channels = in->channels;
        s->sample_rate = in->sample_rate;

        profile.sampleRate = s->sample_rate;
	    profile.chanNum = s->channels;
	    profile.bitsDepth = 16;
	    profile.frameLen = 1200*s->channels;
	    profile.wavlen = profile.frameLen+100;
	    profile.msg_snd_id = msg_id;
	    profile.msg_rcv_id = msg_id+1;
	    profile.opt_rcv_id = msg_id+2;

	    profile.revb_percent = s->revb_percent;
	    profile.revb_hfdamping = s->revb_hfdamping;
	    profile.revb_roomscale = s->revb_roomscale;
	    profile.revb_depth = s->revb_depth;
	    profile.revb_predelay = s->revb_predelay;
	    profile.reverb_pattern = s->reverb_pattern;

        profile.eq_pattern = s->eq_pattern;
	    profile.total_gain = s->total_gain;
        profile.eq_profile[0] = s->eq_band0;
        profile.eq_profile[1] = s->eq_band1;
        profile.eq_profile[2] = s->eq_band2;
        profile.eq_profile[3] = s->eq_band3;
        profile.eq_profile[4] = s->eq_band4;
        profile.eq_profile[5] = s->eq_band5;
        profile.eq_profile[6] = s->eq_band6;
        profile.eq_profile[7] = s->eq_band7;
        profile.eq_profile[8] = s->eq_band8;
        profile.eq_profile[9] = s->eq_band9;

        s->l_handler = init_AW_API(&profile);
        if (s->l_handler == NULL) {
            av_log(ctx, AV_LOG_INFO,"reinit tuner resource failed\n");
            return -1;
        }
        av_log(ctx, AV_LOG_INFO,"reinit tuner resource success\n");
    }

    out = ff_get_audio_buffer(outlink, in->nb_samples);
    if (!out) {
        ret = AVERROR(ENOMEM);
        goto fail;
    }
    ret = av_frame_copy_props(out, in);
    if (ret < 0)
        goto fail;

    frame_len = in->nb_samples*s->channels;
    data = av_malloc(frame_len*sizeof(int));
    if (data == NULL) {
        ret = AVERROR(ENOMEM);
        goto fail;
    }

    av_log(ctx, AV_LOG_DEBUG,"before runAW_API in_len[%d]\n", frame_len);
    // switch channel before tune
    switch_channel(s->l_handler,s->switch_flag_before_tune,in->data[0],frame_len);
    runAW_API(s->l_handler, in->data[0], frame_len, data);
    av_log(ctx, AV_LOG_DEBUG,"runAW_API success\n");
    Int2ShortCpy((int16_t*)out->data[0], data, frame_len);
    // switch channel after tune
    switch_channel(s->l_handler,s->switch_flag_after_tune,out->data[0],frame_len);

    av_free(data);
    av_frame_free(&in);

    return ff_filter_frame(outlink, out);

    fail:
    av_frame_free(&in);
    av_frame_free(&out);
    return ret;
}

static int process_command(AVFilterContext *ctx, const char *cmd, const char *args,
                           char *res, int res_len, int flags)
{
    ATunerContext *s = ctx->priv;
    int gainValue[10] = {0};

    av_log(ctx, AV_LOG_INFO,
           "command: %s,"
           "args: %s\n",
           cmd,
           args
    );

    if (s->l_handler == NULL) return -1;

    if (sscanf(args, "%d:%d:%d:%d:%d:%d:%d:%d:%d:%d", 
        &gainValue[0], &gainValue[1], &gainValue[2], &gainValue[3], &gainValue[4],
        &gainValue[5], &gainValue[6], &gainValue[7], &gainValue[8], &gainValue[9]) != 10)
            return AVERROR(EINVAL);

    for (int i=0; i<10; i++) {
        if (gainValue[i] == 0xFF) continue;
        if (setEqOption(s->l_handler, i, gainValue[i]) < 0) {
           av_log(ctx, AV_LOG_INFO,"setEqOption[%d] failed\n", i);
        } else {
           av_log(ctx, AV_LOG_INFO,"setEqOption[%d:%d] success\n", i, gainValue[i]);
        }
    }
   
    return 0;
}

static const AVFilterPad tuner_inputs[] = {
    {
        .name         = "default",
        .type         = AVMEDIA_TYPE_AUDIO,
        .filter_frame = filter_frame,
    },
};

static const AVFilterPad tuner_outputs[] = {
    {
        .name          = "default",
        .type          = AVMEDIA_TYPE_AUDIO,
    },
};

const AVFilter ff_af_tuner = {
    .name          = "tuner",
    .description   = NULL_IF_CONFIG_SMALL("audio tuner."),
    .priv_size     = sizeof(ATunerContext),
    .init          = init,
    .uninit        = uninit,
    FILTER_INPUTS(tuner_inputs),
    FILTER_OUTPUTS(tuner_outputs),
    .process_command = process_command,
    .priv_class    = &tuner_class,
    .flags         = AVFILTER_FLAG_SUPPORT_TIMELINE_INTERNAL,
};
