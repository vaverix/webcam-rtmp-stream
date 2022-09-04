#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>

typedef struct stream_ctx_t
{
	char* output_path;
	char* output_format;
	AVInputFormat* ifmt, * ifmt_a;
	AVFormatContext* ifmt_ctx, * ifmt_ctx_a, * ofmt_ctx;
	AVCodec* in_codec, * in_codec_a, * out_codec, * out_codec_a;
	AVStream* in_stream, * in_stream_a, * out_stream, * out_stream_a;
	AVCodecContext* in_codec_ctx, * in_codec_ctx_a, * out_codec_ctx, * out_codec_ctx_a;
} stream_ctx_t;

bool end_stream;

void stream_video(const char* device_index, const char* adevice_index, const char* output_path, const char* output_format, int width, int height, int fps);
int init_device_and_input_context(stream_ctx_t* stream_ctx, const char* device_family, const char* device_index, int width, int height, int fps);
int init_adevice_and_input_context(stream_ctx_t* stream_ctx, const char* adevice_family, const char* adevice_index);
int init_output_avformat_context(stream_ctx_t* stream_ctx, const char* format_name);
int init_io_context(stream_ctx_t* stream_ctx, const char* output_path);
void set_codec_params(stream_ctx_t* stream_ctx, int width, int height, int fps);
void set_acodec_params(stream_ctx_t* stream_ctx);
int init_codec_stream(stream_ctx_t* stream_ctx);
int init_acodec_stream(stream_ctx_t* stream_ctx);
struct SwsContext* initialize_video_sample_scaler(stream_ctx_t* stream_ctx);
//struct SwsContext* initialize_audio_resampler(stream_ctx_t* stream_ctx);
char* concat_str(const char* s1, const char* s2);
const char* get_device_family();
const char* get_adevice_family();
void handle_signal(int signal);
