#include "xdecode.h"
#include <algorithm>

XDecode::XDecode() 
    : codec_ctx_(nullptr), codec_(nullptr), initialized_(false),
      hw_device_ctx_(nullptr), hw_pixel_format_(AV_PIX_FMT_NONE) {
}

XDecode::~XDecode() {
    Close();
}

bool XDecode::Init(const DecodeConfig& config) {
    if (initialized_) {
        LOG_WARN("Decoder already initialized");
        return true;
    }
    
    config_ = config;
    
    if (!DecodeUtils::ValidateConfig(config_)) {
        LOG_ERROR("Invalid decode configuration");
        return false;
    }
    
    if (!CreateDecoder()) {
        LOG_ERROR("Failed to create decoder");
        return false;
    }
    
    if (!ConfigureDecoder()) {
        LOG_ERROR("Failed to configure decoder");
        Close();
        return false;
    }
    
    if (avcodec_open2(codec_ctx_, codec_, nullptr) < 0) {
        LOG_ERROR("Failed to open decoder");
        Close();
        return false;
    }
    
    initialized_ = true;
    start_time_ = std::chrono::steady_clock::now();
    
    LOG_INFO("Decoder initialized: %s, threads=%d, hw_accel=%s",
             GetDecoderInfo().c_str(), codec_ctx_->thread_count,
             config_.enable_hw_accel ? config_.hw_device.c_str() : "disabled");
    
    return true;
}

bool XDecode::Decode(const AVPacket* packet) {
    if (!initialized_) {
        LOG_ERROR("Decoder not initialized");
        return false;
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // 发送包到解码器
    int ret = avcodec_send_packet(codec_ctx_, packet);
    if (ret < 0) {
        if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
            LOG_ERROR("Failed to send packet to decoder: %s", Utils::AVErrorToString(ret).c_str());
            return false;
        }
    }
    
    // 接收解码后的帧
    AVFrame* frame = av_frame_alloc();
    if (!frame) {
        LOG_ERROR("Failed to allocate frame");
        return false;
    }
    
    bool success = true;
    while (ret >= 0) {
        ret = avcodec_receive_frame(codec_ctx_, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            LOG_ERROR("Failed to receive frame from decoder: %s", Utils::AVErrorToString(ret).c_str());
            success = false;
            break;
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto decode_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        ProcessFrame(frame);
        UpdateStats(frame, decode_time.count());
        
        av_frame_unref(frame);
    }
    
    av_frame_free(&frame);
    return success;
}

bool XDecode::Flush() {
    if (!initialized_) {
        return false;
    }
    
    LOG_INFO("Flushing decoder...");
    return Decode(nullptr);  // 发送nullptr表示flush
}

void XDecode::Close() {
    if (codec_ctx_) {
        avcodec_free_context(&codec_ctx_);
        codec_ctx_ = nullptr;
    }
    
    if (hw_device_ctx_) {
        av_buffer_unref(&hw_device_ctx_);
        hw_device_ctx_ = nullptr;
    }
    
    codec_ = nullptr;
    initialized_ = false;
    
    LOG_INFO("Decoder closed");
}

DecodeStats XDecode::GetStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    DecodeStats stats = stats_;
    
    // 计算平均FPS
    if (stats_.total_time_ms > 0) {
        stats.avg_fps = (stats_.frames_decoded * 1000.0) / stats_.total_time_ms;
    }
    
    // 计算平均解码时间
    if (stats_.frames_decoded > 0) {
        stats.avg_decode_time_ms = stats_.total_time_ms / (double)stats_.frames_decoded;
    }
    
    return stats;
}

bool XDecode::SetParameter(const std::string& key, const std::string& value) {
    if (!codec_ctx_) {
        LOG_ERROR("Decoder not created");
        return false;
    }
    
    if (initialized_) {
        LOG_WARN("Cannot set parameter after decoder is opened");
        return false;
    }
    
    int ret = av_opt_set(codec_ctx_->priv_data, key.c_str(), value.c_str(), 0);
    if (ret < 0) {
        LOG_ERROR("Failed to set parameter %s=%s: %s", 
                 key.c_str(), value.c_str(), Utils::AVErrorToString(ret).c_str());
        return false;
    }
    
    LOG_INFO("Set parameter: %s=%s", key.c_str(), value.c_str());
    return true;
}

std::string XDecode::GetDecoderInfo() const {
    if (!codec_) {
        return "Unknown";
    }
    
    return std::string(codec_->name) + " (" + codec_->long_name + ")";
}

bool XDecode::CreateDecoder() {
    // 子类实现
    return false;
}

bool XDecode::ConfigureDecoder() {
    if (!codec_ctx_) {
        return false;
    }
    
    // 基本参数
    if (config_.width > 0 && config_.height > 0) {
        codec_ctx_->width = config_.width;
        codec_ctx_->height = config_.height;
    }
    
    codec_ctx_->pix_fmt = config_.pixel_format;
    
    // 多线程配置
    if (config_.enable_multithreading) {
        codec_ctx_->thread_count = config_.thread_count > 0 ? 
            config_.thread_count : DecodeUtils::GetRecommendedThreadCount();
        codec_ctx_->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;
    } else {
        codec_ctx_->thread_count = 1;
    }
    
    // 硬件加速
    if (config_.enable_hw_accel && !config_.hw_device.empty()) {
        if (SetupHardwareAccel()) {
            LOG_INFO("Hardware acceleration enabled: %s", config_.hw_device.c_str());
        } else {
            LOG_WARN("Failed to setup hardware acceleration, using software decoding");
        }
    }
    
    return true;
}

void XDecode::ProcessFrame(AVFrame* frame) {
    if (config_.frame_callback) {
        config_.frame_callback(frame);
    }
}

void XDecode::UpdateStats(const AVFrame* frame, int64_t decode_time_us) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    stats_.frames_decoded++;
    stats_.total_time_ms += decode_time_us / 1000;
    
    // 估算解码字节数（基于帧大小）
    if (frame->width > 0 && frame->height > 0) {
        size_t frame_size = frame->width * frame->height * 3 / 2; // YUV420P估算
        stats_.bytes_decoded += frame_size;
    }
}

bool XDecode::SetupHardwareAccel() {
    AVHWDeviceType hw_type = av_hwdevice_find_type_by_name(config_.hw_device.c_str());
    if (hw_type == AV_HWDEVICE_TYPE_NONE) {
        LOG_ERROR("Unknown hardware device type: %s", config_.hw_device.c_str());
        return false;
    }
    
    int ret = av_hwdevice_ctx_create(&hw_device_ctx_, hw_type, nullptr, nullptr, 0);
    if (ret < 0) {
        LOG_ERROR("Failed to create hardware device context: %s", Utils::AVErrorToString(ret).c_str());
        return false;
    }
    
    codec_ctx_->hw_device_ctx = av_buffer_ref(hw_device_ctx_);
    
    // 查找硬件像素格式
    for (int i = 0; ; i++) {
        const AVCodecHWConfig* config = avcodec_get_hw_config(codec_, i);
        if (!config) {
            break;
        }
        
        if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
            config->device_type == hw_type) {
            hw_pixel_format_ = config->pix_fmt;
            codec_ctx_->pix_fmt = hw_pixel_format_;
            break;
        }
    }
    
    return true;
}

// H264Decoder实现
H264Decoder::H264Decoder() {
}

H264Decoder::~H264Decoder() {
}

bool H264Decoder::Init(const DecodeConfig& config) {
    return XDecode::Init(config);
}

std::string H264Decoder::GetDecoderInfo() const {
    return "H.264/AVC Decoder";
}

bool H264Decoder::CreateDecoder() {
    // 优先使用硬件解码器
    if (config_.enable_hw_accel) {
        if (config_.hw_device == "cuda") {
            codec_ = avcodec_find_decoder_by_name("h264_cuvid");
        } else if (config_.hw_device == "vaapi") {
            codec_ = avcodec_find_decoder_by_name("h264_vaapi");
        } else if (config_.hw_device == "qsv") {
            codec_ = avcodec_find_decoder_by_name("h264_qsv");
        }
    }

    // 回退到软件解码器
    if (!codec_) {
        codec_ = avcodec_find_decoder(AV_CODEC_ID_H264);
    }

    if (!codec_) {
        LOG_ERROR("H.264 decoder not found");
        return false;
    }

    codec_ctx_ = avcodec_alloc_context3(codec_);
    if (!codec_ctx_) {
        LOG_ERROR("Failed to allocate H.264 decoder context");
        return false;
    }

    return true;
}

bool H264Decoder::ConfigureDecoder() {
    if (!XDecode::ConfigureDecoder()) {
        return false;
    }

    SetH264SpecificOptions();
    return true;
}

void H264Decoder::SetH264SpecificOptions() {
    // H.264特定选项
    if (codec_ctx_->priv_data) {
        // 启用错误恢复
        av_opt_set(codec_ctx_->priv_data, "err_detect", "ignore_err", 0);
    }
}

// H265Decoder实现
H265Decoder::H265Decoder() {
}

H265Decoder::~H265Decoder() {
}

bool H265Decoder::Init(const DecodeConfig& config) {
    return XDecode::Init(config);
}

std::string H265Decoder::GetDecoderInfo() const {
    return "H.265/HEVC Decoder";
}

bool H265Decoder::CreateDecoder() {
    // 优先使用硬件解码器
    if (config_.enable_hw_accel) {
        if (config_.hw_device == "cuda") {
            codec_ = avcodec_find_decoder_by_name("hevc_cuvid");
        } else if (config_.hw_device == "vaapi") {
            codec_ = avcodec_find_decoder_by_name("hevc_vaapi");
        } else if (config_.hw_device == "qsv") {
            codec_ = avcodec_find_decoder_by_name("hevc_qsv");
        }
    }

    // 回退到软件解码器
    if (!codec_) {
        codec_ = avcodec_find_decoder(AV_CODEC_ID_HEVC);
    }

    if (!codec_) {
        LOG_ERROR("H.265 decoder not found");
        return false;
    }

    codec_ctx_ = avcodec_alloc_context3(codec_);
    if (!codec_ctx_) {
        LOG_ERROR("Failed to allocate H.265 decoder context");
        return false;
    }

    return true;
}

bool H265Decoder::ConfigureDecoder() {
    if (!XDecode::ConfigureDecoder()) {
        return false;
    }

    SetH265SpecificOptions();
    return true;
}

void H265Decoder::SetH265SpecificOptions() {
    // H.265特定选项
    if (codec_ctx_->priv_data) {
        // 启用错误恢复
        av_opt_set(codec_ctx_->priv_data, "err_detect", "ignore_err", 0);
    }
}
