#pragma once

#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <chrono>
#include <functional>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

// 日志宏 - C++17兼容版本
#define LOG_DEBUG(fmt, ...) printf("[DEBUG] " fmt "\n", ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  printf("[INFO]  " fmt "\n", ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  printf("[WARN]  " fmt "\n", ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) printf("[ERROR] " fmt "\n", ##__VA_ARGS__)

// 错误码定义
enum class ErrorCode {
    SUCCESS = 0,
    INVALID_PARAM = -1,
    MEMORY_ERROR = -2,
    CODEC_ERROR = -3,
    FORMAT_ERROR = -4,
    NETWORK_ERROR = -5,
    TIMEOUT_ERROR = -6,
    UNKNOWN_ERROR = -999
};

// 像素格式枚举
enum class PixelFormat {
    UNKNOWN = 0,
    YUV420P,
    YUV422P,
    YUV444P,
    RGB24,
    RGBA,
    BGR24,
    BGRA,
    NV12,
    NV21
};

// 编码器类型
enum class CodecType {
    UNKNOWN = 0,
    H264,
    H265,
    VP8,
    VP9,
    AV1
};

// 硬件加速类型
enum class HWAccelType {
    NONE = 0,
    CUDA,
    DXVA2,
    QSV,
    VAAPI,
    VIDEOTOOLBOX
};

// 媒体类型
enum class MediaType {
    UNKNOWN = 0,
    VIDEO,
    AUDIO,
    SUBTITLE
};

// 时间戳结构
struct Timestamp {
    int64_t pts = AV_NOPTS_VALUE;
    int64_t dts = AV_NOPTS_VALUE;
    AVRational time_base = {1, 1000000};
    
    double ToSeconds() const {
        if (pts == AV_NOPTS_VALUE) return -1.0;
        return av_q2d(time_base) * pts;
    }
    
    static Timestamp FromSeconds(double seconds, AVRational tb = {1, 1000000}) {
        Timestamp ts;
        ts.time_base = tb;
        ts.pts = ts.dts = static_cast<int64_t>(seconds / av_q2d(tb));
        return ts;
    }
};

// 视频参数结构
struct VideoParams {
    int width = 0;
    int height = 0;
    PixelFormat pixel_format = PixelFormat::YUV420P;
    AVRational frame_rate = {25, 1};
    AVRational time_base = {1, 25};
    int bit_rate = 1000000;
    int gop_size = 50;
    
    bool IsValid() const {
        return width > 0 && height > 0;
    }
};

// 音频参数结构
struct AudioParams {
    int sample_rate = 44100;
    int channels = 2;
    AVSampleFormat sample_format = AV_SAMPLE_FMT_S16;
    int bit_rate = 128000;
    
    bool IsValid() const {
        return sample_rate > 0 && channels > 0;
    }
};

// 编码参数结构
struct EncodeParams {
    VideoParams video;
    AudioParams audio;
    CodecType codec_type = CodecType::H264;
    HWAccelType hw_accel = HWAccelType::NONE;
    std::string preset = "medium";
    std::string tune = "";
    int crf = 23;  // 质量因子
    bool use_b_frames = true;
    
    bool IsValid() const {
        return video.IsValid() && codec_type != CodecType::UNKNOWN;
    }
};

// 解码参数结构
struct DecodeParams {
    HWAccelType hw_accel = HWAccelType::NONE;
    int thread_count = 0;  // 0表示自动检测
    bool low_delay = false;
    
    bool IsValid() const {
        return true;  // 解码参数通常都是有效的
    }
};

// 工具函数
class Utils {
public:
    // FFmpeg错误码转字符串
    static std::string AVErrorToString(int error_code) {
        char error_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(error_code, error_buf, sizeof(error_buf));
        return std::string(error_buf);
    }
    
    // 像素格式转换
    static AVPixelFormat ToAVPixelFormat(PixelFormat format) {
        switch (format) {
            case PixelFormat::YUV420P: return AV_PIX_FMT_YUV420P;
            case PixelFormat::YUV422P: return AV_PIX_FMT_YUV422P;
            case PixelFormat::YUV444P: return AV_PIX_FMT_YUV444P;
            case PixelFormat::RGB24:   return AV_PIX_FMT_RGB24;
            case PixelFormat::RGBA:    return AV_PIX_FMT_RGBA;
            case PixelFormat::BGR24:   return AV_PIX_FMT_BGR24;
            case PixelFormat::BGRA:    return AV_PIX_FMT_BGRA;
            case PixelFormat::NV12:    return AV_PIX_FMT_NV12;
            case PixelFormat::NV21:    return AV_PIX_FMT_NV21;
            default: return AV_PIX_FMT_NONE;
        }
    }
    
    static PixelFormat FromAVPixelFormat(AVPixelFormat format) {
        switch (format) {
            case AV_PIX_FMT_YUV420P: return PixelFormat::YUV420P;
            case AV_PIX_FMT_YUV422P: return PixelFormat::YUV422P;
            case AV_PIX_FMT_YUV444P: return PixelFormat::YUV444P;
            case AV_PIX_FMT_RGB24:   return PixelFormat::RGB24;
            case AV_PIX_FMT_RGBA:    return PixelFormat::RGBA;
            case AV_PIX_FMT_BGR24:   return PixelFormat::BGR24;
            case AV_PIX_FMT_BGRA:    return PixelFormat::BGRA;
            case AV_PIX_FMT_NV12:    return PixelFormat::NV12;
            case AV_PIX_FMT_NV21:    return PixelFormat::NV21;
            default: return PixelFormat::UNKNOWN;
        }
    }
    
    // 编码器ID转换
    static AVCodecID ToAVCodecID(CodecType type) {
        switch (type) {
            case CodecType::H264: return AV_CODEC_ID_H264;
            case CodecType::H265: return AV_CODEC_ID_HEVC;
            case CodecType::VP8:  return AV_CODEC_ID_VP8;
            case CodecType::VP9:  return AV_CODEC_ID_VP9;
            case CodecType::AV1:  return AV_CODEC_ID_AV1;
            default: return AV_CODEC_ID_NONE;
        }
    }
    
    // 获取当前时间戳(毫秒)
    static int64_t GetCurrentTimeMs() {
        auto now = std::chrono::steady_clock::now();
        auto duration = now.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    }
    
    // 睡眠指定毫秒数
    static void SleepMs(int ms) {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }
};

// RAII包装器
template<typename T, typename Deleter>
class RAIIWrapper {
public:
    RAIIWrapper(T* ptr, Deleter deleter) : ptr_(ptr), deleter_(deleter) {}
    ~RAIIWrapper() { if (ptr_) deleter_(ptr_); }
    
    RAIIWrapper(const RAIIWrapper&) = delete;
    RAIIWrapper& operator=(const RAIIWrapper&) = delete;
    
    RAIIWrapper(RAIIWrapper&& other) noexcept : ptr_(other.ptr_), deleter_(other.deleter_) {
        other.ptr_ = nullptr;
    }
    
    RAIIWrapper& operator=(RAIIWrapper&& other) noexcept {
        if (this != &other) {
            if (ptr_) deleter_(ptr_);
            ptr_ = other.ptr_;
            deleter_ = other.deleter_;
            other.ptr_ = nullptr;
        }
        return *this;
    }
    
    T* get() const { return ptr_; }
    T* release() { T* tmp = ptr_; ptr_ = nullptr; return tmp; }
    void reset(T* ptr = nullptr) { if (ptr_) deleter_(ptr_); ptr_ = ptr; }
    
    explicit operator bool() const { return ptr_ != nullptr; }
    T& operator*() const { return *ptr_; }
    T* operator->() const { return ptr_; }
    
private:
    T* ptr_;
    Deleter deleter_;
};

// FFmpeg对象的RAII包装器 - 使用lambda来处理函数签名
using AVFramePtr = RAIIWrapper<AVFrame, std::function<void(AVFrame*)>>;
using AVPacketPtr = RAIIWrapper<AVPacket, std::function<void(AVPacket*)>>;
using AVFormatContextPtr = RAIIWrapper<AVFormatContext, std::function<void(AVFormatContext*)>>;
using AVCodecContextPtr = RAIIWrapper<AVCodecContext, std::function<void(AVCodecContext*)>>;
using SwsContextPtr = RAIIWrapper<SwsContext, std::function<void(SwsContext*)>>;

// 创建RAII包装器的辅助函数
inline AVFramePtr MakeAVFrame() {
    return AVFramePtr(av_frame_alloc(), [](AVFrame* p) { av_frame_free(&p); });
}

inline AVPacketPtr MakeAVPacket() {
    return AVPacketPtr(av_packet_alloc(), [](AVPacket* p) { av_packet_free(&p); });
}

inline AVFormatContextPtr MakeAVFormatContext() {
    return AVFormatContextPtr(avformat_alloc_context(), [](AVFormatContext* p) { avformat_free_context(p); });
}

inline AVCodecContextPtr MakeAVCodecContext(const AVCodec* codec) {
    return AVCodecContextPtr(avcodec_alloc_context3(codec), [](AVCodecContext* p) { avcodec_free_context(&p); });
}
