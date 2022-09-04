// WebcamRtmpStream.cpp

#define _CRT_SECURE_NO_WARNINGS
#include "WebcamRtmpStream.h"

int main(int argc, char* argv[])
{
    if (argc != 8)
    {
        fprintf(stderr, "Usage: %s [video_device] [audio_device] [output_path] [output_format] [width] [height] [fps]\n", argv[0]);
        return 1;
    }

    const char* device = argv[1];
    const char* adevice = argv[2];
    const char* output_path = argv[3];
    const char* output_format = argv[4];
    int width = atoi(argv[5]);
    int height = atoi(argv[6]);
    int fps = atoi(argv[7]);

    end_stream = false;
    signal(SIGINT, handle_signal);

    stream_video(device, adevice, output_path, output_format, width, height, fps);

    return 0;
}

void stream_video(const char* device_index, const char* adevice_index, const char* output_path, const char* output_format, int width, int height, int fps)
{
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 9, 100)
    av_register_all();
#endif
    avdevice_register_all();
    avformat_network_init();

    const char* device_family = get_device_family();
    const char* adevice_family = get_adevice_family();

    stream_ctx_t* stream_ctx = malloc(sizeof(stream_ctx_t));
    stream_ctx->output_path = malloc(strlen(output_path) + 1);
    stream_ctx->output_format = malloc(strlen(output_format) + 1);
    stream_ctx->ifmt = NULL;
    stream_ctx->ifmt_a = NULL;
    stream_ctx->ifmt_ctx = NULL;
    stream_ctx->ifmt_ctx_a = NULL;
    stream_ctx->ofmt_ctx = NULL;
    stream_ctx->out_codec = NULL;
    stream_ctx->out_codec_a = NULL;
    stream_ctx->out_stream = NULL;
    stream_ctx->out_stream_a = NULL;
    stream_ctx->out_codec_ctx = NULL;
    stream_ctx->out_codec_ctx_a = NULL;

    memcpy(stream_ctx->output_path, output_path, strlen(output_path));
    stream_ctx->output_path[strlen(output_path)] = '\0';
    memcpy(stream_ctx->output_format, output_format, strlen(output_format));
    stream_ctx->output_format[strlen(output_format)] = '\0';

    if (init_device_and_input_context(stream_ctx, device_family, device_index, width, height, fps) != 0)
    {
        return;
    }

    if (init_adevice_and_input_context(stream_ctx, adevice_family, adevice_index) != 0)
    {
        return;
    }

    init_output_avformat_context(stream_ctx, output_format);
    init_io_context(stream_ctx, output_path);

    stream_ctx->out_codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    stream_ctx->out_stream = avformat_new_stream(stream_ctx->ofmt_ctx, stream_ctx->out_codec);
    stream_ctx->out_codec_ctx = avcodec_alloc_context3(stream_ctx->out_codec);

    stream_ctx->out_codec_a = avcodec_find_encoder(AV_CODEC_ID_AAC);
    stream_ctx->out_stream_a = avformat_new_stream(stream_ctx->ofmt_ctx, stream_ctx->out_codec_a);
    stream_ctx->out_codec_ctx_a = avcodec_alloc_context3(stream_ctx->out_codec_a);

    set_codec_params(stream_ctx, width, height, fps);
    set_acodec_params(stream_ctx);
    init_codec_stream(stream_ctx);
    init_acodec_stream(stream_ctx);

    stream_ctx->out_stream->codecpar->extradata = stream_ctx->out_codec_ctx->extradata;
    stream_ctx->out_stream->codecpar->extradata_size = stream_ctx->out_codec_ctx->extradata_size;

    stream_ctx->out_stream_a->codecpar->extradata = stream_ctx->out_codec_ctx_a->extradata;
    stream_ctx->out_stream_a->codecpar->extradata_size = stream_ctx->out_codec_ctx_a->extradata_size;

    av_dump_format(stream_ctx->ofmt_ctx, 0, output_path, 1);

    if (avformat_write_header(stream_ctx->ofmt_ctx, NULL) != 0)
    {
        fprintf(stderr, "could not write header to ouput context!\n");
        return;
    }

    AVFrame* frame = av_frame_alloc();
    AVFrame* frame_a = av_frame_alloc();
    AVFrame* outframe = av_frame_alloc();
    AVFrame* outframe_a = av_frame_alloc();
    AVPacket* pkt = av_packet_alloc();
    AVPacket* pkt_a = av_packet_alloc();

    int nbytes = av_image_get_buffer_size(stream_ctx->out_codec_ctx->pix_fmt, stream_ctx->out_codec_ctx->width, stream_ctx->out_codec_ctx->height, 32);
    uint8_t* video_outbuf = (uint8_t*)av_malloc(nbytes);
    av_image_fill_arrays(outframe->data, outframe->linesize, video_outbuf, AV_PIX_FMT_YUV420P, stream_ctx->out_codec_ctx->width, stream_ctx->out_codec_ctx->height, 1);
    outframe->width = width;
    outframe->height = height;
    outframe->format = stream_ctx->out_codec_ctx->pix_fmt;

    struct SwsContext* swsctx = initialize_video_sample_scaler(stream_ctx);
    //struct SwsContext* swsctx_a = initialize_audio_resampler(stream_ctx);
    av_new_packet(pkt, 0);
    av_new_packet(pkt_a, 0);

    long pts = 0;
    long pts_a = 0;

    while (!end_stream)
    {
        if (av_read_frame(stream_ctx->ifmt_ctx, pkt) >= 0) {
            frame = av_frame_alloc();
            if (avcodec_send_packet(stream_ctx->in_codec_ctx, pkt) != 0)
            {
                fprintf(stderr, "error sending packet to input video codec context!\n");
                break;
            }

            if (avcodec_receive_frame(stream_ctx->in_codec_ctx, frame) != 0)
            {
                fprintf(stderr, "error receiving frame from input video codec context!\n");
                break;
            }

            av_packet_unref(pkt);
            av_new_packet(pkt, 0);

            sws_scale(swsctx, (const uint8_t* const*)frame->data, frame->linesize, 0, stream_ctx->in_codec_ctx->height, outframe->data, outframe->linesize);
            av_frame_free(&frame);
            outframe->pts = pts++;

            if (avcodec_send_frame(stream_ctx->out_codec_ctx, outframe) != 0)
            {
                fprintf(stderr, "error sending frame to output video codec context!\n");
                break;
            }

            if (avcodec_receive_packet(stream_ctx->out_codec_ctx, pkt) != 0)
            {
                fprintf(stderr, "error receiving packet from output video codec context!\n");
                break;
            }

            pkt->pts = av_rescale_q(pkt->pts, stream_ctx->out_codec_ctx->time_base, stream_ctx->out_stream->time_base);
            pkt->dts = av_rescale_q(pkt->dts, stream_ctx->out_codec_ctx->time_base, stream_ctx->out_stream->time_base);

            av_interleaved_write_frame(stream_ctx->ofmt_ctx, pkt);
            av_packet_unref(pkt);
            av_new_packet(pkt, 0);
            av_frame_free(&outframe);
        }
        /*
        if (av_read_frame(stream_ctx->ifmt_ctx_a, pkt_a) >= 0) {
            frame_a = av_frame_alloc();
            if (avcodec_send_packet(stream_ctx->in_codec_ctx_a, pkt_a) != 0)
            {
                fprintf(stderr, "error sending packet to input audio codec context!\n");
                break;
            }

            if (avcodec_receive_frame(stream_ctx->in_codec_ctx_a, frame_a) != 0)
            {
                fprintf(stderr, "error receiving frame from input audio codec context!\n");
                break;
            }

            av_packet_unref(pkt_a);
            av_new_packet(pkt_a, 0);

            // TODO: RESAMPLE AUDIO HERE
            outframe_a = frame_a;
            av_frame_free(&frame_a);
            outframe_a->pts = pts_a++;

            if (avcodec_send_frame(stream_ctx->out_codec_ctx_a, outframe_a) != 0)
            {
                fprintf(stderr, "error sending frame to output audio codec context!\n");
                break;
            }

            if (avcodec_receive_packet(stream_ctx->out_codec_ctx_a, pkt_a) != 0)
            {
                fprintf(stderr, "error receiving packet from output audio codec context!\n");
                break;
            }

            pkt_a->pts = av_rescale_q(pkt_a->pts, stream_ctx->out_codec_ctx_a->time_base, stream_ctx->out_stream_a->time_base);
            pkt_a->dts = av_rescale_q(pkt_a->dts, stream_ctx->out_codec_ctx_a->time_base, stream_ctx->out_stream_a->time_base);

            av_interleaved_write_frame(stream_ctx->ofmt_ctx, pkt_a);
            av_packet_unref(pkt_a);
            av_new_packet(pkt_a, 0);
            av_frame_free(&outframe_a);
        }
        */
    }

    av_write_trailer(stream_ctx->ofmt_ctx);
    av_frame_free(&outframe);
    av_frame_free(&outframe_a);
    avio_close(stream_ctx->ofmt_ctx->pb);
    avformat_free_context(stream_ctx->ofmt_ctx);
    avio_close(stream_ctx->ifmt_ctx->pb);
    avio_close(stream_ctx->ifmt_ctx_a->pb);
    avformat_free_context(stream_ctx->ifmt_ctx);
    avformat_free_context(stream_ctx->ifmt_ctx_a);

    fprintf(stderr, "done.\n");
}

int init_device_and_input_context(stream_ctx_t* stream_ctx, const char* device_family, const char* device_index, int width, int height, int fps)
{
    char fps_str[5], width_str[5], height_str[5];
    sprintf(fps_str, "%d", fps);
    sprintf(width_str, "%d", width);
    sprintf(height_str, "%d", height);

    char* tmp = concat_str(width_str, "x");
    char* size = concat_str(tmp, height_str);
    free(tmp);

    stream_ctx->ifmt = av_find_input_format(device_family);

    AVDictionary* options = NULL;
    av_dict_set(&options, "video_size", size, 0);
    av_dict_set(&options, "framerate", fps_str, 0);
    av_dict_set(&options, "pixel_format", "uyvy422", 0);
    av_dict_set(&options, "probesize", "7000000", 0);

    free(size);

    if (avformat_open_input(&stream_ctx->ifmt_ctx, device_index, stream_ctx->ifmt, &options) != 0)
    {
        fprintf(stderr, "cannot initialize video input device!\n");
        return 1;
    }

    avformat_find_stream_info(stream_ctx->ifmt_ctx, 0);
    // av_dump_format(stream_ctx->ifmt_ctx, 0, device_family, 0);

    stream_ctx->in_codec = avcodec_find_decoder(stream_ctx->ifmt_ctx->streams[0]->codecpar->codec_id);
    stream_ctx->in_stream = avformat_new_stream(stream_ctx->ifmt_ctx, stream_ctx->in_codec);
    stream_ctx->in_codec_ctx = avcodec_alloc_context3(stream_ctx->in_codec);

    AVDictionary* codec_options = NULL;
    av_dict_set(&codec_options, "framerate", fps_str, 0);
    av_dict_set(&codec_options, "preset", "superfast", 0);

    avcodec_parameters_to_context(stream_ctx->in_codec_ctx, stream_ctx->ifmt_ctx->streams[0]->codecpar);
    if (avcodec_open2(stream_ctx->in_codec_ctx, stream_ctx->in_codec, &codec_options) != 0)
    {
        fprintf(stderr, "cannot initialize video decoder!\n");
        return 1;
    }

    return 0;
}

int init_adevice_and_input_context(stream_ctx_t* stream_ctx, const char* adevice_family, const char* adevice_index)
{
    stream_ctx->ifmt_a = av_find_input_format(adevice_family);
    AVDictionary* options = NULL;

    if (avformat_open_input(&stream_ctx->ifmt_ctx_a, adevice_index, stream_ctx->ifmt_a, &options) != 0)
    {
        fprintf(stderr, "cannot initialize audio input device!\n");
        return 1;
    }

    avformat_find_stream_info(stream_ctx->ifmt_ctx_a, 0);

    stream_ctx->in_codec_a = avcodec_find_decoder(stream_ctx->ifmt_ctx_a->streams[0]->codecpar->codec_id);
    stream_ctx->in_stream_a = avformat_new_stream(stream_ctx->ifmt_ctx_a, stream_ctx->in_codec_a);
    stream_ctx->in_codec_ctx_a = avcodec_alloc_context3(stream_ctx->in_codec_a);

    AVDictionary* codec_options = NULL;
    const char* channels = "2";
    av_dict_set(&codec_options, "channels", channels, 0);
    //avcodec_parameters_to_context(stream_ctx->in_codec_ctx_a, stream_ctx->ifmt_ctx_a->streams[0]->codecpar);
    if (avcodec_open2(stream_ctx->in_codec_ctx_a, stream_ctx->in_codec_a, &codec_options) != 0)
    {
        fprintf(stderr, "cannot initialize audio decoder!\n");
        return 1;
    }

    return 0;
}

int init_output_avformat_context(stream_ctx_t* stream_ctx, const char* format_name)
{
    if (avformat_alloc_output_context2(&stream_ctx->ofmt_ctx, NULL, format_name, NULL) != 0)
    {
        fprintf(stderr, "cannot initialize output format context!\n");
        return 1;
    }

    return 0;
}

int init_io_context(stream_ctx_t* stream_ctx, const char* output_path)
{
    if (avio_open2(&stream_ctx->ofmt_ctx->pb, output_path, AVIO_FLAG_WRITE, NULL, NULL) != 0)
    {
        fprintf(stderr, "could not open IO context!\n");
        return 1;
    }

    return 0;
}

void set_codec_params(stream_ctx_t* stream_ctx, int width, int height, int fps)
{
    const AVRational dst_fps = { fps, 1 };

    stream_ctx->out_codec_ctx->codec_tag = 0;
    stream_ctx->out_codec_ctx->codec_id = AV_CODEC_ID_H264;
    stream_ctx->out_codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
    stream_ctx->out_codec_ctx->width = width;
    stream_ctx->out_codec_ctx->height = height;
    stream_ctx->out_codec_ctx->gop_size = 12;
    stream_ctx->out_codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    stream_ctx->out_codec_ctx->framerate = dst_fps;
    stream_ctx->out_codec_ctx->time_base = av_inv_q(dst_fps);

    if (stream_ctx->ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
    {
        stream_ctx->out_codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
}

void set_acodec_params(stream_ctx_t* stream_ctx)
{
    stream_ctx->out_codec_ctx_a->channels = 2;
    stream_ctx->out_codec_ctx_a->channel_layout = av_get_default_channel_layout(2);
    stream_ctx->out_codec_ctx_a->sample_rate = stream_ctx->ifmt_ctx_a->streams[0]->codec->sample_rate;
    stream_ctx->out_codec_ctx_a->sample_fmt = stream_ctx->out_codec_a->sample_fmts[0];
    stream_ctx->out_codec_ctx_a->bit_rate = 32000;
    stream_ctx->out_codec_ctx_a->time_base.num = 1;
    stream_ctx->out_codec_ctx_a->time_base.den = stream_ctx->out_codec_ctx_a->sample_rate;
    stream_ctx->out_codec_ctx_a->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

    if (stream_ctx->ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
    {
        stream_ctx->out_codec_ctx_a->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
}

int init_codec_stream(stream_ctx_t* stream_ctx)
{
    if (avcodec_parameters_from_context(stream_ctx->out_stream->codecpar, stream_ctx->out_codec_ctx) != 0)
    {
        fprintf(stderr, "could not initialize video stream codec parameters!\n");
        return 1;
    }

    AVDictionary* codec_options = NULL;
#ifndef _WIN32
    av_dict_set(&codec_options, "profile", "high", 0);
#endif // !_WIN32
    av_dict_set(&codec_options, "preset", "superfast", 0);
    av_dict_set(&codec_options, "tune", "zerolatency", 0);

    // open video encoder
    if (avcodec_open2(stream_ctx->out_codec_ctx, stream_ctx->out_codec, &codec_options) != 0)
    {
        fprintf(stderr, "could not open video encoder!\n");
        return 1;
    }

    return 0;
}

int init_acodec_stream(stream_ctx_t* stream_ctx)
{
    if (avcodec_parameters_from_context(stream_ctx->out_stream_a->codecpar, stream_ctx->out_codec_ctx_a) != 0)
    {
        fprintf(stderr, "could not initialize audio stream codec parameters!\n");
        return 1;
    }

    /*
    AVDictionary* codec_options = NULL;
    if (avcodec_open2(stream_ctx->out_codec_ctx, stream_ctx->out_codec, &codec_options) != 0)
    {
        fprintf(stderr, "could not open audio encoder!\n");
        return 1;
    }
    */

    return 0;
}

struct SwsContext* initialize_video_sample_scaler(stream_ctx_t* stream_ctx)
{
    struct SwsContext* swsctx = sws_getContext(stream_ctx->in_codec_ctx->width, stream_ctx->in_codec_ctx->height, stream_ctx->in_codec_ctx->pix_fmt, stream_ctx->out_codec_ctx->width, stream_ctx->out_codec_ctx->height, stream_ctx->out_codec_ctx->pix_fmt, SWS_BICUBIC, NULL, NULL, NULL);
    
    if (!swsctx)
    {
        fprintf(stderr, "could not initialize sample scaler!");
    }

    return swsctx;
}
/*
struct SwsContext* initialize_audio_resampler(stream_ctx_t* stream_ctx)
{
    struct SwsContext* swsctx_a = swr_alloc_set_opts(NULL,
        av_get_default_channel_layout(stream_ctx->out_codec_ctx_a->channels),
        stream_ctx->out_codec_ctx_a->sample_fmt,
        stream_ctx->out_codec_ctx_a->sample_rate,
        av_get_default_channel_layout(stream_ctx->ifmt_ctx_a->streams[0]->codec->channels),
        stream_ctx->ifmt_ctx_a->streams[0]->codec->sample_fmt,
        stream_ctx->ifmt_ctx_a->streams[0]->codec->sample_rate,
        0, NULL);
    if (swr_init(swsctx_a) < 0) {
        fprintf(stderr, "could not initialize audio resampler!");
    }
    return swsctx_a;
}
*/
char* concat_str(const char* s1, const char* s2)
{
    const size_t len1 = strlen(s1);
    const size_t len2 = strlen(s2);
    char* result = malloc(len1 + len2 + 1);
    memcpy(result, s1, len1);
    memcpy(result + len1, s2, len2 + 1);
    return result;
}

const char* get_device_family()
{
#ifdef _WIN32
    const char* device_family = "dshow";
#elif __APPLE__
    const char* device_family = "avfoundation";
#elif __linux__
    const char* device_family = "v4l2";
#endif

    return device_family;
}

const char* get_adevice_family()
{
#ifdef _WIN32
    const char* adevice_family = "dshow";
#elif __APPLE__
    const char* adevice_family = "avfoundation";
#elif __linux__
    const char* adevice_family = "alsa";
#endif

    return adevice_family;
}

void handle_signal(int signal)
{
    fprintf(stderr, "Caught SIGINT, exiting now...\n");
    end_stream = true;
}
