#include "xencode.h"
#include <algorithm>

XEncode::XEncode() 
    : codec_ctx_(nullptr), codec_(nullptr), initialized_(false),
      hw_device_ctx_(nullptr), hw_pixel_format_(AV_PIX_FMT_NONE) {
}

XEncode::~XEncode() {
    Close();
}

bool XEncode::Init(const EncodeConfig& config) {
    if (initialized_) {
        LOG_WARN("Encoder already initialized");
        return true;
    }
    
    config_ = config;
    
    if (!EncodeUtils::ValidateConfig(config_)) {
        LOG_ERROR("Invalid encode configuration");
        return false;
    }
    
    if (!CreateEncoder()) {
        LOG_ERROR("Failed to create encoder");
        return false;
    }
    
    if (!ConfigureEncoder()) {
        LOG_ERROR("Failed to configure encoder");
        Close();
        return false;
    }
    
    if (avcodec_open2(codec_ctx_, codec_, nullptr) < 0) {
        LOG_ERROR("Failed to open encoder");
        Close();
        return false;
    }
    
    initialized_ = true;
    start_time_ = std::chrono::steady_clock::now();
    
    LOG_INFO("Encoder initialized: %s, %dx%d, %.2f fps, %ld bps",
             GetEncoderInfo().c_str(), config_.width, config_.height,
             av_q2d(config_.frame_rate), config_.bit_rate);
    
    return true;
}

bool XEncode::Encode(const AVFrame* frame) {
    if (!initialized_) {
        LOG_ERROR("Encoder not initialized");
        return false;
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // 发送帧到编码器
    int ret = avcodec_send_frame(codec_ctx_, frame);
    if (ret < 0) {
        LOG_ERROR("Failed to send frame to encoder: %s", Utils::AVErrorToString(ret).c_str());
        return false;
    }
    
    // 接收编码后的包
    AVPacket* packet = av_packet_alloc();
    if (!packet) {
        LOG_ERROR("Failed to allocate packet");
        return false;
    }
    
    bool success = true;
    while (ret >= 0) {
        ret = avcodec_receive_packet(codec_ctx_, packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            LOG_ERROR("Failed to receive packet from encoder: %s", Utils::AVErrorToString(ret).c_str());
            success = false;
            break;
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto encode_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        ProcessPacket(packet);
        UpdateStats(packet, encode_time.count());
        
        av_packet_unref(packet);
    }
    
    av_packet_free(&packet);
    return success;
}

bool XEncode::Flush() {
    if (!initialized_) {
        return false;
    }
    
    LOG_INFO("Flushing encoder...");
    return Encode(nullptr);  // 发送nullptr表示flush
}

void XEncode::Close() {
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
    
    LOG_INFO("Encoder closed");
}

EncodeStats XEncode::GetStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    EncodeStats stats = stats_;
    
    // 计算平均FPS
    if (stats_.encode_time_ms > 0) {
        stats.avg_fps = (stats_.frames_encoded * 1000.0) / stats_.encode_time_ms;
    }
    
    // 计算平均码率
    if (stats_.frames_encoded > 0 && config_.frame_rate.den > 0) {
        double duration_sec = stats_.frames_encoded * config_.frame_rate.den / (double)config_.frame_rate.num;
        if (duration_sec > 0) {
            stats.avg_bitrate = (stats_.bytes_encoded * 8.0) / duration_sec;
        }
    }
    
    return stats;
}

bool XEncode::SetParameter(const std::string& key, const std::string& value) {
    if (!codec_ctx_) {
        LOG_ERROR("Encoder not created");
        return false;
    }
    
    if (initialized_) {
        LOG_WARN("Cannot set parameter after encoder is opened");
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

std::string XEncode::GetEncoderInfo() const {
    if (!codec_) {
        return "Unknown";
    }
    
    return std::string(codec_->name) + " (" + codec_->long_name + ")";
}

bool XEncode::CreateEncoder() {
    // 子类实现
    return false;
}

bool XEncode::ConfigureEncoder() {
    if (!codec_ctx_) {
        return false;
    }
    
    // 基本参数
    codec_ctx_->width = config_.width;
    codec_ctx_->height = config_.height;
    codec_ctx_->pix_fmt = config_.pixel_format;
    codec_ctx_->time_base = config_.time_base;
    codec_ctx_->framerate = config_.frame_rate;
    
    // 码率控制
    codec_ctx_->bit_rate = config_.bit_rate;
    codec_ctx_->gop_size = config_.gop_size;
    codec_ctx_->max_b_frames = config_.max_b_frames;
    
    // 质量参数
    if (config_.crf >= 0) {
        codec_ctx_->flags |= AV_CODEC_FLAG_QSCALE;
        codec_ctx_->global_quality = config_.crf * FF_QP2LAMBDA;
    }
    
    codec_ctx_->qmin = config_.qmin;
    codec_ctx_->qmax = config_.qmax;
    
    // 全局头
    if (config_.use_global_header) {
        codec_ctx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
    
    // 硬件加速
    if (config_.enable_hw_accel && !config_.hw_device.empty()) {
        if (SetupHardwareAccel()) {
            LOG_INFO("Hardware acceleration enabled: %s", config_.hw_device.c_str());
        } else {
            LOG_WARN("Failed to setup hardware acceleration, using software encoding");
        }
    }
    
    return true;
}

void XEncode::ProcessPacket(AVPacket* packet) {
    if (config_.packet_callback) {
        config_.packet_callback(packet);
    }
}

void XEncode::UpdateStats(const AVPacket* packet, int64_t encode_time_us) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    stats_.frames_encoded++;
    stats_.bytes_encoded += packet->size;
    stats_.encode_time_ms += encode_time_us / 1000;
    
    // 更新质量信息（如果可用）
    if (packet->flags & AV_PKT_FLAG_KEY) {
        // 关键帧
    }
}

bool XEncode::SetupHardwareAccel() {
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

// H264Encoder实现
H264Encoder::H264Encoder() {
}

H264Encoder::~H264Encoder() {
}

bool H264Encoder::Init(const EncodeConfig& config) {
    return XEncode::Init(config);
}

std::string H264Encoder::GetEncoderInfo() const {
    return "H.264/AVC Encoder";
}

bool H264Encoder::CreateEncoder() {
    // 优先使用硬件编码器
    if (config_.enable_hw_accel) {
        if (config_.hw_device == "cuda") {
            codec_ = avcodec_find_encoder_by_name("h264_nvenc");
        } else if (config_.hw_device == "vaapi") {
            codec_ = avcodec_find_encoder_by_name("h264_vaapi");
        } else if (config_.hw_device == "qsv") {
            codec_ = avcodec_find_encoder_by_name("h264_qsv");
        }
    }

    // 回退到软件编码器
    if (!codec_) {
        codec_ = avcodec_find_encoder_by_name("libx264");
        if (!codec_) {
            codec_ = avcodec_find_encoder(AV_CODEC_ID_H264);
        }
    }

    if (!codec_) {
        LOG_ERROR("H.264 encoder not found");
        return false;
    }

    codec_ctx_ = avcodec_alloc_context3(codec_);
    if (!codec_ctx_) {
        LOG_ERROR("Failed to allocate H.264 encoder context");
        return false;
    }

    return true;
}

bool H264Encoder::ConfigureEncoder() {
    if (!XEncode::ConfigureEncoder()) {
        return false;
    }

    SetH264SpecificOptions();
    return true;
}

void H264Encoder::SetH264SpecificOptions() {
    // 设置profile和level
    if (!config_.profile.empty()) {
        av_opt_set(codec_ctx_->priv_data, "profile", config_.profile.c_str(), 0);
    }

    if (!config_.level.empty()) {
        av_opt_set(codec_ctx_->priv_data, "level", config_.level.c_str(), 0);
    }

    // 设置预设
    std::string preset_name = XEncodeFactory::GetPresetName(config_.preset);
    if (!preset_name.empty()) {
        av_opt_set(codec_ctx_->priv_data, "preset", preset_name.c_str(), 0);
    }

    // 设置CRF
    if (config_.crf >= 0) {
        av_opt_set_int(codec_ctx_->priv_data, "crf", config_.crf, 0);
    }

    // 其他H.264特定选项
    av_opt_set(codec_ctx_->priv_data, "tune", "film", 0);  // 针对电影内容优化
}

// H265Encoder实现
H265Encoder::H265Encoder() {
}

H265Encoder::~H265Encoder() {
}

bool H265Encoder::Init(const EncodeConfig& config) {
    return XEncode::Init(config);
}

std::string H265Encoder::GetEncoderInfo() const {
    return "H.265/HEVC Encoder";
}

bool H265Encoder::CreateEncoder() {
    // 优先使用硬件编码器
    if (config_.enable_hw_accel) {
        if (config_.hw_device == "cuda") {
            codec_ = avcodec_find_encoder_by_name("hevc_nvenc");
        } else if (config_.hw_device == "vaapi") {
            codec_ = avcodec_find_encoder_by_name("hevc_vaapi");
        } else if (config_.hw_device == "qsv") {
            codec_ = avcodec_find_encoder_by_name("hevc_qsv");
        }
    }

    // 回退到软件编码器
    if (!codec_) {
        codec_ = avcodec_find_encoder_by_name("libx265");
        if (!codec_) {
            codec_ = avcodec_find_encoder(AV_CODEC_ID_HEVC);
        }
    }

    if (!codec_) {
        LOG_ERROR("H.265 encoder not found");
        return false;
    }

    codec_ctx_ = avcodec_alloc_context3(codec_);
    if (!codec_ctx_) {
        LOG_ERROR("Failed to allocate H.265 encoder context");
        return false;
    }

    return true;
}

bool H265Encoder::ConfigureEncoder() {
    if (!XEncode::ConfigureEncoder()) {
        return false;
    }

    SetH265SpecificOptions();
    return true;
}

void H265Encoder::SetH265SpecificOptions() {
    // 设置预设
    std::string preset_name = XEncodeFactory::GetPresetName(config_.preset);
    if (!preset_name.empty()) {
        av_opt_set(codec_ctx_->priv_data, "preset", preset_name.c_str(), 0);
    }

    // 设置CRF
    if (config_.crf >= 0) {
        av_opt_set_int(codec_ctx_->priv_data, "crf", config_.crf, 0);
    }

    // H.265特定选项
    av_opt_set(codec_ctx_->priv_data, "tune", "grain", 0);  // 保留颗粒感
}
