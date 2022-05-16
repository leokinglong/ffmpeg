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
 * audio noise filter.
 *
 * Based on af_aresample.c
 */

#include "libavutil/avstring.h"
#include "libavutil/channel_layout.h"
#include "libavutil/opt.h"
#include "libavutil/samplefmt.h"
#include "libavutil/avassert.h"
#include "avfilter.h"
#include "audio.h"
#include "internal.h"
#include "NS_API.h"

typedef struct ANoiseCancellerContext {
    const AVClass *class;

    int channels;
    int sample_rate;
    void *l_handler;
} ANoiseCancellerContext;

#define OFFSET(x) offsetof(ANoiseCancellerContext, x)
#define A AV_OPT_FLAG_AUDIO_PARAM|AV_OPT_FLAG_FILTERING_PARAM

static av_cold int init(AVFilterContext *ctx)
{
    ANoiseCancellerContext *s = ctx->priv;

    s->l_handler = NULL;

    return 0;
}

static av_cold void uninit(AVFilterContext *ctx)
{
    ANoiseCancellerContext *s = ctx->priv;
    //clear resource
    terminate_NS_API(s->l_handler);
    s->l_handler = NULL;
    av_log(ctx, AV_LOG_INFO,"uninit anoisecanceller resource success\n");
}

static int filter_frame(AVFilterLink *inlink, AVFrame *in)
{
    AVFilterContext *ctx = inlink->dst;
    ANoiseCancellerContext *s = ctx->priv;
    AVFilterLink *outlink = ctx->outputs[0];
    AVFrame *out = NULL;
    char *data;
    int out_size, ret;

    if (s->l_handler != NULL) {
        if (s->channels != in->channels || s->sample_rate != in->sample_rate) {
            terminate_NS_API(s->l_handler);
            s->l_handler = NULL;
            av_log(ctx, AV_LOG_INFO,"channels or sample_rate change uninit anoisecanceller\n");
        }
    }

    if (s->l_handler == NULL) {
        av_log(ctx, AV_LOG_INFO,
        "\n"
        "config_input param is :\n"
        "Input l_channel:    %d\n"
        "Input l_sampleRate:   %d\n",
        in->channels,in->sample_rate
        );

        s->channels = in->channels;
        s->sample_rate = in->sample_rate;

        s->l_handler = init_NS_API(s->channels, s->sample_rate, kVeryHigh);
        if (s->l_handler == NULL) {
            av_log(ctx, AV_LOG_INFO,"init anoisecanceller resource failed\n");
            return -1;
        }
        av_log(ctx, AV_LOG_INFO,"init anoisecanceller resource success\n");
    } 


    data = (char*)runNS_API(s->l_handler, in->data[0], in->nb_samples*s->channels, &out_size);
    av_log(ctx, AV_LOG_DEBUG,
           "\n"
           "Input frameSize:    %d\n"
           "output frameSize: %d\n",
           in->nb_samples*s->channels,
           out_size
    );

    out = ff_get_audio_buffer(outlink, out_size/s->channels);
    if (!out) {
        ret = AVERROR(ENOMEM);
        goto fail;
    }
    ret = av_frame_copy_props(out, in);
    if (ret < 0)
        goto fail;

    memcpy(out->data[0], data, out_size*sizeof(int16_t));
    av_frame_free(&in);

    return ff_filter_frame(outlink, out);

    fail:
    av_frame_free(&in);
    av_frame_free(&out);
    return ret;
}

static const AVFilterPad anoisecanceller_inputs[] = {
    {
        .name         = "default",
        .type         = AVMEDIA_TYPE_AUDIO,
        .filter_frame = filter_frame,
    },
};

static const AVFilterPad anoisecanceller_outputs[] = {
    {
        .name          = "default",
        .type          = AVMEDIA_TYPE_AUDIO,
    },
};

const AVFilter ff_af_anoisecanceller = {
    .name          = "anoisecanceller",
    .description   = NULL_IF_CONFIG_SMALL("audio noise canceller."),
    .priv_size     = sizeof(ANoiseCancellerContext),
    .init          = init,
    .uninit        = uninit,
    FILTER_INPUTS(anoisecanceller_inputs),
    FILTER_OUTPUTS(anoisecanceller_outputs),
    .flags         = AVFILTER_FLAG_SUPPORT_TIMELINE_INTERNAL,
};
