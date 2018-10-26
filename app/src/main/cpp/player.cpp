#include <jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/log.h>
#include <pthread.h>
#include <unistd.h>
#include "queue.h"

extern "C" {
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
#include "libavutil/imgutils.h"
}

// Android 打印 Log
#define LOGE(FORMAT,...) __android_log_print(ANDROID_LOG_ERROR, "player", FORMAT, ##__VA_ARGS__);

/**
 * 错误打印
 * @param filename
 * @param err
 */
void print_error(int err) {
    char err_buf[128];
    const char *err_buf_ptr = err_buf;
    if (av_strerror(err, err_buf, sizeof(err_buf_ptr)) < 0) {
        err_buf_ptr = strerror(AVUNERROR(err));
    }
    LOGE("ffmpeg error descript : %s", err_buf_ptr);
}

/**
 * 播放视频流
 * R# 代表申请内存 需要释放或关闭
 */
extern "C"
JNIEXPORT void JNICALL
Java_com_johan_player_Player_playVideo(JNIEnv *env, jobject instance, jstring path_, jobject surface) {
    // 记录结果
    int result;
    // R1 Java String -> C String
    const char *path = env->GetStringUTFChars(path_, 0);
    // 注册 FFmpeg 组件
    av_register_all();
    // R2 初始化 AVFormatContext 上下文
    AVFormatContext *format_context = avformat_alloc_context();
    // 打开视频文件
    result = avformat_open_input(&format_context, path, NULL, NULL);
    if (result < 0) {
        LOGE("Player Error : Can not open video file");
        return;
    }
    // 查找视频文件的流信息
    result = avformat_find_stream_info(format_context, NULL);
    if (result < 0) {
        LOGE("Player Error : Can not find video file stream info");
        return;
    }
    // 查找视频编码器
    int video_stream_index = -1;
    for (int i = 0; i < format_context->nb_streams; i++) {
        // 匹配视频流
        if (format_context->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index = i;
        }
    }
    // 没找到视频流
    if (video_stream_index == -1) {
        LOGE("Player Error : Can not find video stream");
        return;
    }
    // 初始化视频编码器上下文
    AVCodecContext *video_codec_context = avcodec_alloc_context3(NULL);
    avcodec_parameters_to_context(video_codec_context, format_context->streams[video_stream_index]->codecpar);
    // 初始化视频编码器
    AVCodec *video_codec = avcodec_find_decoder(video_codec_context->codec_id);
    if (video_codec == NULL) {
        LOGE("Player Error : Can not find video codec");
        return;
    }
    // R3 打开视频解码器
    result  = avcodec_open2(video_codec_context, video_codec, NULL);
    if (result < 0) {
        LOGE("Player Error : Can not open video codec");
        return;
    }
    // 获取视频的宽高
    int videoWidth = video_codec_context->width;
    int videoHeight = video_codec_context->height;
    // R4 初始化 Native Window 用于播放视频
    ANativeWindow *native_window = ANativeWindow_fromSurface(env, surface);
    if (native_window == NULL) {
        LOGE("Player Error : Can not create native window");
        return;
    }
    // 通过设置宽高限制缓冲区中的像素数量，而非屏幕的物理显示尺寸。
    // 如果缓冲区与物理屏幕的显示尺寸不相符，则实际显示可能会是拉伸，或者被压缩的图像
    result = ANativeWindow_setBuffersGeometry(native_window, videoWidth, videoHeight,WINDOW_FORMAT_RGBA_8888);
    if (result < 0){
        LOGE("Player Error : Can not set native window buffer");
        ANativeWindow_release(native_window);
        return;
    }
    // 定义绘图缓冲区
    ANativeWindow_Buffer window_buffer;
    // 声明数据容器 有3个
    // R5 解码前数据容器 Packet 编码数据
    AVPacket *packet = av_packet_alloc();
    // R6 解码后数据容器 Frame 像素数据 不能直接播放像素数据 还要转换
    AVFrame *frame = av_frame_alloc();
    // R7 转换后数据容器 这里面的数据可以用于播放
    AVFrame *rgba_frame = av_frame_alloc();
    // 数据格式转换准备
    // 输出 Buffer
    int buffer_size = av_image_get_buffer_size(AV_PIX_FMT_RGBA, videoWidth, videoHeight, 1);
    // R8 申请 Buffer 内存
    uint8_t *out_buffer = (uint8_t *) av_malloc(buffer_size * sizeof(uint8_t));
    av_image_fill_arrays(rgba_frame->data, rgba_frame->linesize, out_buffer, AV_PIX_FMT_RGBA, videoWidth, videoHeight, 1);
    // R9 数据格式转换上下文
    struct SwsContext *data_convert_context = sws_getContext(
            videoWidth, videoHeight, video_codec_context->pix_fmt,
            videoWidth, videoHeight, AV_PIX_FMT_RGBA,
            SWS_BICUBIC, NULL, NULL, NULL);
    // 开始读取帧
    while (av_read_frame(format_context, packet) >= 0) {
        // 匹配视频流
        if (packet->stream_index == video_stream_index) {
            // 解码
            result = avcodec_send_packet(video_codec_context, packet);
            if (result < 0 && result != AVERROR(EAGAIN) && result != AVERROR_EOF) {
                LOGE("Player Error : codec step 1 fail");
                return;
            }
            result = avcodec_receive_frame(video_codec_context, frame);
            if (result < 0 && result != AVERROR_EOF) {
                LOGE("Player Error : codec step 2 fail %d", result);
                return;
            }
            // 数据格式转换
            result = sws_scale(
                    data_convert_context,
                    (const uint8_t* const*) frame->data, frame->linesize,
                    0, videoHeight,
                    rgba_frame->data, rgba_frame->linesize);
            if (result <= 0) {
                LOGE("Player Error : data convert fail");
                return;
            }
            // 播放
            result = ANativeWindow_lock(native_window, &window_buffer, NULL);
            if (result < 0) {
                LOGE("Player Error : Can not lock native window");
            } else {
                // 将图像绘制到界面上
                // 注意 : 这里 rgba_frame 一行的像素和 window_buffer 一行的像素长度可能不一致
                // 需要转换好 否则可能花屏
                uint8_t *bits = (uint8_t *) window_buffer.bits;
                for (int h = 0; h < videoHeight; h++) {
                    memcpy(bits + h * window_buffer.stride * 4,
                           out_buffer + h * rgba_frame->linesize[0],
                           rgba_frame->linesize[0]);
                }
                ANativeWindow_unlockAndPost(native_window);
            }
        }
        // 释放 packet 引用
        av_packet_unref(packet);
    }
    // 释放 R9
    sws_freeContext(data_convert_context);
    // 释放 R8
    av_free(out_buffer);
    // 释放 R7
    av_frame_free(&rgba_frame);
    // 释放 R6
    av_frame_free(&frame);
    // 释放 R5
    av_packet_free(&packet);
    // 释放 R4
    ANativeWindow_release(native_window);
    // 关闭 R3
    avcodec_close(video_codec_context);
    // 释放 R2
    avformat_close_input(&format_context);
    // 释放 R1
    env->ReleaseStringUTFChars(path_, path);
}

/**
 * 播放音频流
 * R# 代表申请内存 需要释放或关闭
 */
extern "C"
JNIEXPORT void JNICALL
Java_com_johan_player_Player_playAudio(JNIEnv *env, jobject instance, jstring path_) {
    // 记录结果
    int result;
    // R1 Java String -> C String
    const char *path = env->GetStringUTFChars(path_, 0);
    // 注册组件
    av_register_all();
    // R2 创建 AVFormatContext 上下文
    AVFormatContext *format_context = avformat_alloc_context();
    // R3 打开视频文件
    avformat_open_input(&format_context, path, NULL, NULL);
    // 查找视频文件的流信息
    result = avformat_find_stream_info(format_context, NULL);
    if (result < 0) {
        LOGE("Player Error : Can not find video file stream info");
        return;
    }
    // 查找音频编码器
    int audio_stream_index = -1;
    for (int i = 0; i < format_context->nb_streams; i++) {
        // 匹配音频流
        if (format_context->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_stream_index = i;
        }
    }
    // 没找到音频流
    if (audio_stream_index == -1) {
        LOGE("Player Error : Can not find audio stream");
        return;
    }
    // 初始化音频编码器上下文
    AVCodecContext *audio_codec_context = avcodec_alloc_context3(NULL);
    avcodec_parameters_to_context(audio_codec_context, format_context->streams[audio_stream_index]->codecpar);
    // 初始化音频编码器
    AVCodec *audio_codec = avcodec_find_decoder(audio_codec_context->codec_id);
    if (audio_codec == NULL) {
        LOGE("Player Error : Can not find audio codec");
        return;
    }
    // R4 打开视频解码器
    result  = avcodec_open2(audio_codec_context, audio_codec, NULL);
    if (result < 0) {
        LOGE("Player Error : Can not open audio codec");
        return;
    }
    // 音频重采样准备
    // R5 重采样上下文
    struct SwrContext *swr_context = swr_alloc();
    // 缓冲区
    uint8_t *out_buffer = (uint8_t *) av_malloc(44100 * 2);
    // 输出的声道布局 (双通道 立体音)
    uint64_t out_channel_layout = AV_CH_LAYOUT_STEREO;
    // 输出采样位数 16位
    enum AVSampleFormat out_format = AV_SAMPLE_FMT_S16;
    // 输出的采样率必须与输入相同
    int out_sample_rate = audio_codec_context->sample_rate;
    //swr_alloc_set_opts 将PCM源文件的采样格式转换为自己希望的采样格式
    swr_alloc_set_opts(swr_context,
                       out_channel_layout, out_format, out_sample_rate,
                       audio_codec_context->channel_layout, audio_codec_context->sample_fmt, audio_codec_context->sample_rate,
                       0, NULL);
    swr_init(swr_context);
    // 调用 Java 层创建 AudioTrack
    int out_channels = av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO);
    jclass player_class = env->GetObjectClass(instance);
    jmethodID create_audio_track_method_id = env->GetMethodID(player_class, "createAudioTrack", "(II)V");
    env->CallVoidMethod(instance, create_audio_track_method_id, 44100, out_channels);
    // 播放音频准备
    jmethodID play_audio_track_method_id = env->GetMethodID(player_class, "playAudioTrack", "([BI)V");
    // 声明数据容器 有2个
    // R6 解码前数据容器 Packet 编码数据
    AVPacket *packet = av_packet_alloc();
    // R7 解码后数据容器 Frame MPC数据 还不能直接播放 还要进行重采样
    AVFrame *frame = av_frame_alloc();
    // 开始读取帧
    while (av_read_frame(format_context, packet) >= 0) {
        // 匹配音频流
        if (packet->stream_index == audio_stream_index) {
            // 解码
            result = avcodec_send_packet(audio_codec_context, packet);
            if (result < 0 && result != AVERROR(EAGAIN) && result != AVERROR_EOF) {
                LOGE("Player Error : codec step 1 fail");
                return;
            }
            result = avcodec_receive_frame(audio_codec_context, frame);
            if (result < 0 && result != AVERROR_EOF) {
                LOGE("Player Error : codec step 2 fail");
                return;
            }
            // 重采样
            swr_convert(swr_context, &out_buffer, 44100 * 2, (const uint8_t **) frame->data, frame->nb_samples);
            // 播放音频
            // 调用 Java 层播放 AudioTrack
            int size = av_samples_get_buffer_size(NULL, out_channels, frame->nb_samples, AV_SAMPLE_FMT_S16, 1);
            jbyteArray audio_sample_array = env->NewByteArray(size);
            env->SetByteArrayRegion(audio_sample_array, 0, size, (const jbyte *) out_buffer);
            env->CallVoidMethod(instance, play_audio_track_method_id, audio_sample_array, size);
            env->DeleteLocalRef(audio_sample_array);
        }
        // 释放 packet 引用
        av_packet_unref(packet);
    }
    // 调用 Java 层释放 AudioTrack
    jmethodID release_audio_track_method_id = env->GetMethodID(player_class, "releaseAudioTrack", "()V");
    env->CallVoidMethod(instance, release_audio_track_method_id);
    // 释放 R7
    av_frame_free(&frame);
    // 释放 R6
    av_packet_free(&packet);
    // 释放 R5
    swr_free(&swr_context);
    // 关闭 R4
    avcodec_close(audio_codec_context);
    // 关闭 R3
    avformat_close_input(&format_context);
    // 释放 R2
    avformat_free_context(format_context);
    // 释放 R1
    env->ReleaseStringUTFChars(path_, path);
}

// 状态码
#define SUCCESS_CODE 1
#define FAIL_CODE -1

// C 层播放器结构体
typedef struct _Player {
    // Env
    JavaVM *java_vm;
    // Java 实例
    jobject instance;
    jobject surface;
    // 上下文
    AVFormatContext *format_context;
    // 视频相关
    int video_stream_index;
    AVCodecContext *video_codec_context;
    ANativeWindow *native_window;
    ANativeWindow_Buffer window_buffer;
    uint8_t *video_out_buffer;
    struct SwsContext *sws_context;
    AVFrame *rgba_frame;
    Queue *video_queue;
    // 音频相关
    int audio_stream_index;
    AVCodecContext *audio_codec_context;
    uint8_t *audio_out_buffer;
    struct SwrContext *swr_context;
    int out_channels;
    jmethodID play_audio_track_method_id;
    Queue *audio_queue;
} Player;

// 消费载体
typedef struct _Consumer {
    Player* player;
    int stream_index;
} Consumer;

// 生成函数声明
void* produce(void* arg);
// 消费函数声明
void* consume(void* arg);

// 线程相关
pthread_t produce_id, video_consume_id, audio_consume_id;

/**
 * 初始化播放器
 * @param player
 */
void player_init(Player **player, JNIEnv *env, jobject instance, jobject surface) {
    *player = (Player*) malloc(sizeof(Player));
    JavaVM* java_vm;
    env->GetJavaVM(&java_vm);
    (*player)->java_vm = java_vm;
    (*player)->instance = env->NewGlobalRef(instance);
    (*player)->surface = env->NewGlobalRef(surface);
}

/**
 * 初始化 AVFormat
 * @return
 */
int format_init(Player *player, const char* path) {
    int result;
    av_register_all();
    player->format_context = avformat_alloc_context();
    result = avformat_open_input(&(player->format_context), path, NULL, NULL);
    if (result < 0) {
        LOGE("Player Error : Can not open video file");
        return result;
    }
    result = avformat_find_stream_info(player->format_context, NULL);
    if (result < 0) {
        LOGE("Player Error : Can not find video file stream info");
        return result;
    }
    return SUCCESS_CODE;
}

/**
 * 查找流 index
 * @param player
 * @param type
 * @return
 */
int find_stream_index(Player *player, AVMediaType type) {
    AVFormatContext* format_context = player->format_context;
    for (int i = 0; i < format_context->nb_streams; i++) {
        if (format_context->streams[i]->codecpar->codec_type == type) {
            return i;
        }
    }
    return -1;
}

/**
 * 初始化解码器
 * @param player
 * @param type
 * @return
 */
int codec_init(Player *player, AVMediaType type) {
    int result;
    AVFormatContext *format_context = player->format_context;
    int index = find_stream_index(player, type);
    if (index == -1) {
        LOGE("Player Error : Can not find stream");
        return FAIL_CODE;
    }
    AVCodecContext *codec_context = avcodec_alloc_context3(NULL);
    avcodec_parameters_to_context(codec_context, format_context->streams[index]->codecpar);
    AVCodec *codec = avcodec_find_decoder(codec_context->codec_id);
    result = avcodec_open2(codec_context, codec, NULL);
    if (result < 0) {
        LOGE("Player Error : Can not open codec");
        return FAIL_CODE;
    }
    if (type == AVMEDIA_TYPE_VIDEO) {
        player->video_stream_index = index;
        player->video_codec_context = codec_context;
    } else if (type == AVMEDIA_TYPE_AUDIO) {
        player->audio_stream_index = index;
        player->audio_codec_context = codec_context;
    }
    return SUCCESS_CODE;
}

/**
 * 播放视频准备
 * @param player
 */
int video_prepare(Player *player, JNIEnv *env) {
    AVCodecContext *codec_context = player->video_codec_context;
    int videoWidth = codec_context->width;
    int videoHeight = codec_context->height;
    player->native_window = ANativeWindow_fromSurface(env, player->surface);
    if (player->native_window == NULL) {
        LOGE("Player Error : Can not create native window");
        return FAIL_CODE;
    }
    int result = ANativeWindow_setBuffersGeometry(player->native_window, videoWidth, videoHeight,WINDOW_FORMAT_RGBA_8888);
    if (result < 0){
        LOGE("Player Error : Can not set native window buffer");
        ANativeWindow_release(player->native_window);
        return FAIL_CODE;
    }
    player->rgba_frame = av_frame_alloc();
    int buffer_size = av_image_get_buffer_size(AV_PIX_FMT_RGBA, videoWidth, videoHeight, 1);
    player->video_out_buffer = (uint8_t *) av_malloc(buffer_size * sizeof(uint8_t));
    av_image_fill_arrays(player->rgba_frame->data, player->rgba_frame->linesize, player->video_out_buffer, AV_PIX_FMT_RGBA, videoWidth, videoHeight, 1);
    player->sws_context = sws_getContext(
            videoWidth, videoHeight, codec_context->pix_fmt,
            videoWidth, videoHeight, AV_PIX_FMT_RGBA,
            SWS_BICUBIC, NULL, NULL, NULL);
    return SUCCESS_CODE;
}

/**
 * 播放音频准备
 * @param player
 * @return
 */
int audio_prepare(Player *player, JNIEnv* env) {
    AVCodecContext *codec_context = player->audio_codec_context;
    player->swr_context = swr_alloc();
    player->audio_out_buffer = (uint8_t *) av_malloc(44100 * 2);
    uint64_t out_channel_layout = AV_CH_LAYOUT_STEREO;
    enum AVSampleFormat out_format = AV_SAMPLE_FMT_S16;
    int out_sample_rate = player->audio_codec_context->sample_rate;
    swr_alloc_set_opts(player->swr_context,
                       out_channel_layout, out_format, out_sample_rate,
                       codec_context->channel_layout, codec_context->sample_fmt, codec_context->sample_rate,
                       0, NULL);
    swr_init(player->swr_context);
    player->out_channels = av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO);
    jclass player_class = env->GetObjectClass(player->instance);
    jmethodID create_audio_track_method_id = env->GetMethodID(player_class, "createAudioTrack", "(II)V");
    env->CallVoidMethod(player->instance, create_audio_track_method_id, 44100, player->out_channels);
    player->play_audio_track_method_id = env->GetMethodID(player_class, "playAudioTrack", "([BI)V");
    return SUCCESS_CODE;
}

/**
 * 视频播放
 * @param frame
 */
void video_play(Player* player, AVFrame *frame, JNIEnv *env) {
    int video_height = player->video_codec_context->height;
    int result = sws_scale(
            player->sws_context,
            (const uint8_t* const*) frame->data, frame->linesize,
            0, video_height,
            player->rgba_frame->data, player->rgba_frame->linesize);
    if (result <= 0) {
        LOGE("Player Error : video data convert fail");
        return;
    }
    result = ANativeWindow_lock(player->native_window, &(player->window_buffer), NULL);
    if (result < 0) {
        LOGE("Player Error : Can not lock native window");
    } else {
        uint8_t *bits = (uint8_t *) player->window_buffer.bits;
        for (int h = 0; h < video_height; h++) {
            memcpy(bits + h * player->window_buffer.stride * 4,
                   player->video_out_buffer + h * player->rgba_frame->linesize[0],
                   player->rgba_frame->linesize[0]);
        }
        ANativeWindow_unlockAndPost(player->native_window);
    }
}

/**
 * 音频播放
 * @param frame
 */
void audio_play(Player* player, AVFrame *frame, JNIEnv *env) {
    swr_convert(player->swr_context, &(player->audio_out_buffer), 44100 * 2, (const uint8_t **) frame->data, frame->nb_samples);
    int size = av_samples_get_buffer_size(NULL, player->out_channels, frame->nb_samples, AV_SAMPLE_FMT_S16, 1);
    jbyteArray audio_sample_array = env->NewByteArray(size);
    env->SetByteArrayRegion(audio_sample_array, 0, size, (const jbyte *) player->audio_out_buffer);
    env->CallVoidMethod(player->instance, player->play_audio_track_method_id, audio_sample_array, size);
    env->DeleteLocalRef(audio_sample_array);
}

/**
 * 释放播放器
 * @param player
 */
void player_release(Player* player) {
    avformat_close_input(&(player->format_context));
    av_free(player->video_out_buffer);
    av_free(player->audio_out_buffer);
    avcodec_close(player->video_codec_context);
    ANativeWindow_release(player->native_window);
    sws_freeContext(player->sws_context);
    av_frame_free(&(player->rgba_frame));
    avcodec_close(player->audio_codec_context);
    swr_free(&(player->swr_context));
    queue_destroy(player->video_queue);
    queue_destroy(player->audio_queue);
    player->instance = NULL;
    JNIEnv *env;
    int result = player->java_vm->AttachCurrentThread(&env, NULL);
    if (result != JNI_OK) {
        return;
    }
    env->DeleteGlobalRef(player->instance);
    env->DeleteGlobalRef(player->surface);
    player->java_vm->DetachCurrentThread();
}

/**
 *  初始化线程
 */
void thread_init(Player* player) {
    pthread_create(&produce_id, NULL, produce, player);
    Consumer* video_consumer = (Consumer*) malloc(sizeof(Consumer));
    video_consumer->player = player;
    video_consumer->stream_index = player->video_stream_index;
    pthread_create(&video_consume_id, NULL, consume, video_consumer);
    Consumer* audio_consumer = (Consumer*) malloc(sizeof(Consumer));
    audio_consumer->player = player;
    audio_consumer->stream_index = player->audio_stream_index;
    pthread_create(&audio_consume_id, NULL, consume, audio_consumer);
}

/**
 * 生产函数
 * 循环读取帧 解码 丢到对应的队列中
 * @param arg
 * @return
 */
void* produce(void* arg) {
    Player *player = (Player*) arg;
    AVPacket *packet = av_packet_alloc();
    while (av_read_frame(player->format_context, packet) >= 0) {
        if (packet->stream_index == player->video_stream_index) {
            queue_in(player->video_queue, packet);
        } else if (packet->stream_index == player->audio_stream_index) {
            queue_in(player->audio_queue, packet);
        }
        packet = av_packet_alloc();
    }
    break_block(player->video_queue);
    break_block(player->audio_queue);
    for (;;) {
        if (queue_is_empty(player->video_queue) && queue_is_empty(player->audio_queue)) {
            break;
        }
        sleep(1);
    }
    LOGE("produce finish ------------------- ");
    player_release(player);
    return NULL;
}

/**
 * 消费函数
 * 从队列获取解码数据 同步播放
 * @param arg
 * @return
 */
void* consume(void* arg) {
    Consumer *consumer = (Consumer*) arg;
    Player *player = consumer->player;
    int index = consumer->stream_index;
    JNIEnv *env;
    int result = player->java_vm->AttachCurrentThread(&env, NULL);
    if (result != JNI_OK) {
        LOGE("Player Error : Can not get current thread env");
        pthread_exit(NULL);
        return NULL;
    }
    AVCodecContext *codec_context;
    Queue *queue;
    if (index == player->video_stream_index) {
        codec_context = player->video_codec_context;
        queue = player->video_queue;
        video_prepare(player, env);
    } else if (index == player->audio_stream_index) {
        codec_context = player->audio_codec_context;
        queue = player->audio_queue;
        audio_prepare(player, env);
    }
    AVFrame *frame = av_frame_alloc();
    for (;;) {
        AVPacket *packet = queue_out(queue);
        if (packet == NULL) {
            break;
        }
        result = avcodec_send_packet(codec_context, packet);
        if (result < 0 && result != AVERROR(EAGAIN) && result != AVERROR_EOF) {
            print_error(result);
            LOGE("Player Error : %d codec step 1 fail", index);
            av_packet_free(&packet);
            continue;
        }
        result = avcodec_receive_frame(codec_context, frame);
        if (result < 0 && result != AVERROR_EOF) {
            print_error(result);
            LOGE("Player Error : %d codec step 2 fail", index);
            av_packet_free(&packet);
            continue;
        }
        if (index == player->video_stream_index) {
            video_play(player, frame, env);
        } else if (index == player->audio_stream_index) {
            audio_play(player, frame, env);
        }
        av_packet_free(&packet);
    }
    player->java_vm->DetachCurrentThread();
    LOGE("consume is finish ------------------------------ ");
    return NULL;
}

/**
 * 开始播放
 * @param player
 */
void play_start(Player *player) {
    player->video_queue = (Queue*) malloc(sizeof(Queue));
    player->audio_queue = (Queue*) malloc(sizeof(Queue));
    queue_init(player->video_queue);
    queue_init(player->audio_queue);
    thread_init(player);
}

/**
 * 同步播放音视频
 */
extern "C"
JNIEXPORT void JNICALL
Java_com_johan_player_Player_play(JNIEnv *env, jobject instance, jstring path_, jobject surface) {
    const char *path = env->GetStringUTFChars(path_, 0);
    int result = 1;
    Player* player;
    player_init(&player, env, instance, surface);
    if (result > 0) {
        result = format_init(player, path);
    }
    if (result > 0) {
        result = codec_init(player, AVMEDIA_TYPE_VIDEO);
    }
    if (result > 0) {
        result = codec_init(player, AVMEDIA_TYPE_AUDIO);
    }
    if (result > 0) {
        play_start(player);
    }
    env->ReleaseStringUTFChars(path_, path);
}

/** ========================= 测试生产者和消费者模式代码 =========================
// 线程锁
pthread_mutex_t mutex_id;
// 条件变量
pthread_cond_t produce_condition_id, consume_condition_id;
// 队列
Queue queue;
// 生产数量
#define PRODUCE_COUNT 10
// 目前消费数量
int consume_number = 0;
*/

/**
 * 生产者函数
 * @param arg
 * @return
 */
/** ========================= 测试生产者和消费者模式代码 =========================
void* produce(void* arg) {
    char* name = (char*) arg;
    for (int i = 0; i < PRODUCE_COUNT; i++) {
        // 加锁
        pthread_mutex_lock(&mutex_id);
        // 如果队列满了 等待并释放锁
        // 这里为什么要用 while 呢
        // 因为 C 的锁机制有 "惊扰" 现象
        // 没有达到条件会触发 所以要循环判断
        while (queue_is_full(&queue)) {
            pthread_cond_wait(&produce_condition_id, &mutex_id);
        }
        LOGE("%s produce element : %d", name, i);
        // 入队
        queue_in(&queue, (NodeElement) i);
        // 通知消费者可以继续消费
        pthread_cond_signal(&consume_condition_id);
        // 解锁
        pthread_mutex_unlock(&mutex_id);
        // 模拟耗时
        sleep(1);
    }
    LOGE("%s produce finish", name);
    return NULL;
}
*/

/**
 * 消费函数
 * @param arg
 * @return
 */
/** ========================= 测试生产者和消费者模式代码 =========================
void* consume(void* arg) {
    char* name = (char*) arg;
    while (1) {
        // 加锁
        pthread_mutex_lock(&mutex_id);
        // 如果队列为空 等待
        // 使用 while 的理由同上
        while (queue_is_empty(&queue)) {
            // 如果消费到生产最大数量 不再等待
            if (consume_number == PRODUCE_COUNT) {
                break;
            }
            pthread_cond_wait(&consume_condition_id, &mutex_id);
        }
        // 如果消费到生产最大数量
        // 1.通知还在等待的线程
        // 2.解锁
        if (consume_number == PRODUCE_COUNT) {
            // 通知还在等待消费的线程
            pthread_cond_signal(&consume_condition_id);
            // 解锁
            pthread_mutex_unlock(&mutex_id);
            break;
        }
        // 出队
        NodeElement element = queue_out(&queue);
        consume_number += 1;
        LOGE("%s consume element : %d", name, element);
        // 通知生产者可以继续生产
        pthread_cond_signal(&produce_condition_id);
        // 解锁
        pthread_mutex_unlock(&mutex_id);
        // 模拟耗时
        sleep(1);
    }
    LOGE("%s consume finish", name);
    return  NULL;
}
*/

/**
 * 测试子线程
 */
/** ========================= 测试生产者和消费者模式代码 =========================
extern "C"
JNIEXPORT void JNICALL
Java_com_johan_player_Player_testCThread(JNIEnv *env, jobject instance) {
    // 创建队列
    queue_init(&queue);
    // 线程 ID
    pthread_t tid1, tid2, tid3;
    // 创建线程锁
    pthread_mutex_init(&mutex_id, NULL);
    // 创建条件变量
    pthread_cond_init(&produce_condition_id, NULL);
    pthread_cond_init(&consume_condition_id, NULL);
    // 创建新线程并启动
    // 1个生产线程 2个消费线程
    pthread_create(&tid1, NULL, produce, (void*) "producer1");
    pthread_create(&tid2, NULL, consume, (void*) "consumer1");
    pthread_create(&tid3, NULL, consume, (void*) "consumer2");
    // 阻塞线程
    pthread_join(tid1, NULL);
    pthread_join(tid2, NULL);
    pthread_join(tid3, NULL);
    // 销毁条件变量
    pthread_cond_destroy(&produce_condition_id);
    pthread_cond_destroy(&consume_condition_id);
    // 销毁线程锁
    pthread_mutex_destroy(&mutex_id);
    // 销毁队列
    queue_destroy(&queue);
}
*/
