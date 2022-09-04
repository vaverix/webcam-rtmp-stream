#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>

#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavdevice/avdevice.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h" 
#include "libavutil/timestamp.h"
#include "libavutil/rational.h"
#include "libavutil/time.h"
#include "libavutil/pixfmt.h"
#include "libavutil/pixdesc.h"
#include "libavutil/opt.h"
#include "libavutil/channel_layout.h"
#include "libavutil/samplefmt.h"
#include "libswresample/swresample.h"
#include "libavutil/audio_fifo.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"

typedef struct stream_ctx_t
{
	char* output_path;
	char* output_format;
	char* device_index;
	char* adevice_index;
	int width, height, fps, stream_index, audio_stream_index;
	AVInputFormat* ifmt, * ifmt_a;
	AVFormatContext* ifmt_ctx, * ifmt_ctx_a, * ofmt_ctx, * ofmt_ctx_a;
	AVCodec* in_codec, * in_codec_a, * out_codec, * out_codec_a;
	AVStream* in_stream, * in_stream_a, * out_stream, * out_stream_a;
	AVCodecContext* in_codec_ctx, * in_codec_ctx_a, * out_codec_ctx, * out_codec_ctx_a;
	AVFilterGraph* filter_graph;
	AVFilterContext* buffer_sink_ctx;
	AVFilterContext* buffer_src_ctx;
} stream_ctx_t;

bool end_stream;
void handle_signal(int signal);
void clean_up(stream_ctx_t* stream_ctx);
int init(const char* device_index, const char* adevice_index, const char* output_path, const char* output_format, int width, int height, int fps);
int init_video(stream_ctx_t* stream_ctx);
int init_audio(stream_ctx_t* stream_ctx);
void stream(stream_ctx_t* stream_ctx);
int init_audio_sample(stream_ctx_t* stream_ctx);
AVFrame* decode_audio(AVPacket* in_packet, AVFrame* src_audio_frame, AVCodecContext* decode_codectx, AVFilterContext* buffer_sink_ctx, AVFilterContext* buffer_src_ctx);
double av_r2d(AVRational r);
void av_free_context(AVFormatContext* ictx, AVFormatContext* octx);
char* concat_str(const char* s1, const char* s2);
const char* get_device_family();
const char* get_adevice_family();
