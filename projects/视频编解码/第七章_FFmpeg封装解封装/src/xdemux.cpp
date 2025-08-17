#include "xdemux.h"
#include <algorithm>

XDemux::XDemux() 
    : format_ctx_(nullptr), opened_(false),
      video_stream_index_(-1), audio_stream_index_(-1) {
}

XDemux::~XDemux() {
    Close();
}

bool XDemux::Open(const DemuxConfig& config) {
    if (opened_) {
        LOG_WARN("Demuxer already opened");
        return true;
    }
    
    config_ = config;
    
    if (config_.filename.empty()) {
        LOG_ERROR("Filename is empty");
        return false;
    }
    
    // 打开输入文件
    int ret = avformat_open_input(&format_ctx_, config_.filename.c_str(), nullptr, nullptr);
    if (ret < 0) {
        LOG_ERROR("Failed to open input file: %s", Utils::AVErrorToString(ret).c_str());
        return false;
    }
    
    // 查找流信息
    ret = avformat_find_stream_info(format_ctx_, nullptr);
    if (ret < 0) {
        LOG_ERROR("Failed to find stream info: %s", Utils::AVErrorToString(ret).c_str());
        Close();
        return false;
    }
    
    if (!AnalyzeStreams()) {
        LOG_ERROR("Failed to analyze streams");
        Close();
        return false;
    }
    
    opened_ = true;
    start_time_ = std::chrono::steady_clock::now();
    
    LOG_INFO("Demuxer opened: %s, format=%s, duration=%.2fs",
             config_.filename.c_str(), format_ctx_->iformat->name,
             media_info_.duration_us / 1000000.0);
    
    return true;
}

void XDemux::Close() {
    if (format_ctx_) {
        avformat_close_input(&format_ctx_);
        format_ctx_ = nullptr;
    }
    
    opened_ = false;
    video_stream_index_ = -1;
    audio_stream_index_ = -1;
    
    LOG_INFO("Demuxer closed");
}

bool XDemux::ReadPacket(AVPacket* packet) {
    if (!opened_ || !format_ctx_) {
        LOG_ERROR("Demuxer not opened");
        return false;
    }
    
    if (!packet) {
        LOG_ERROR("Invalid packet pointer");
        return false;
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    int ret = av_read_frame(format_ctx_, packet);
    if (ret < 0) {
        if (ret == AVERROR_EOF) {
            LOG_INFO("End of file reached");
        } else {
            LOG_ERROR("Failed to read packet: %s", Utils::AVErrorToString(ret).c_str());
        }
        return false;
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto read_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    ProcessPacket(packet);
    UpdateStats(packet, read_time.count());
    
    return true;
}

bool XDemux::Seek(int64_t timestamp_us, int stream_index) {
    if (!opened_ || !format_ctx_) {
        LOG_ERROR("Demuxer not opened");
        return false;
    }
    
    // 选择流索引
    int target_stream = stream_index;
    if (target_stream < 0) {
        target_stream = video_stream_index_ >= 0 ? video_stream_index_ : audio_stream_index_;
    }
    
    if (target_stream < 0) {
        LOG_ERROR("No valid stream for seeking");
        return false;
    }
    
    // 转换时间戳
    AVRational time_base = format_ctx_->streams[target_stream]->time_base;
    int64_t timestamp = av_rescale_q(timestamp_us, {1, 1000000}, time_base);
    
    int ret = av_seek_frame(format_ctx_, target_stream, timestamp, AVSEEK_FLAG_BACKWARD);
    if (ret < 0) {
        LOG_ERROR("Failed to seek: %s", Utils::AVErrorToString(ret).c_str());
        return false;
    }
    
    LOG_INFO("Seeked to %.2fs", timestamp_us / 1000000.0);
    return true;
}

MediaInfo XDemux::GetMediaInfo() const {
    return media_info_;
}

DemuxStats XDemux::GetStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    DemuxStats stats = stats_;
    
    // 计算平均读取时间
    if (stats_.packets_read > 0) {
        stats.avg_read_time_ms = stats_.total_time_ms / (double)stats_.packets_read;
    }
    
    return stats;
}

bool XDemux::AnalyzeStreams() {
    if (!format_ctx_) {
        return false;
    }
    
    media_info_.filename = config_.filename;
    media_info_.format_name = format_ctx_->iformat->name;
    media_info_.duration_us = format_ctx_->duration;
    media_info_.bit_rate = format_ctx_->bit_rate;
    
    // 获取文件大小
    if (format_ctx_->pb) {
        media_info_.file_size = avio_size(format_ctx_->pb);
    }
    
    // 分析每个流
    for (unsigned int i = 0; i < format_ctx_->nb_streams; ++i) {
        AVStream* stream = format_ctx_->streams[i];
        StreamInfo stream_info;
        
        stream_info.index = i;
        stream_info.type = stream->codecpar->codec_type;
        stream_info.time_base = stream->time_base;
        stream_info.duration = stream->duration;
        stream_info.bit_rate = stream->codecpar->bit_rate;
        
        // 转换编码类型
        switch (stream->codecpar->codec_id) {
            case AV_CODEC_ID_H264:
                stream_info.codec_type = CodecType::H264;
                stream_info.codec_name = "H.264/AVC";
                break;
            case AV_CODEC_ID_HEVC:
                stream_info.codec_type = CodecType::H265;
                stream_info.codec_name = "H.265/HEVC";
                break;
            case AV_CODEC_ID_VP8:
                stream_info.codec_type = CodecType::VP8;
                stream_info.codec_name = "VP8";
                break;
            case AV_CODEC_ID_VP9:
                stream_info.codec_type = CodecType::VP9;
                stream_info.codec_name = "VP9";
                break;
            case AV_CODEC_ID_AV1:
                stream_info.codec_type = CodecType::AV1;
                stream_info.codec_name = "AV1";
                break;
            default:
                stream_info.codec_type = CodecType::UNKNOWN;
                stream_info.codec_name = avcodec_get_name(stream->codecpar->codec_id);
                break;
        }
        
        if (stream_info.type == AVMEDIA_TYPE_VIDEO) {
            stream_info.width = stream->codecpar->width;
            stream_info.height = stream->codecpar->height;
            stream_info.frame_rate = av_guess_frame_rate(format_ctx_, stream, nullptr);
            
            // 选择视频流
            if (config_.enable_video && video_stream_index_ < 0) {
                if (config_.video_stream_index < 0 || config_.video_stream_index == i) {
                    video_stream_index_ = i;
                }
            }
        } else if (stream_info.type == AVMEDIA_TYPE_AUDIO) {
            stream_info.sample_rate = stream->codecpar->sample_rate;
            stream_info.channels = stream->codecpar->ch_layout.nb_channels;
            
            // 选择音频流
            if (config_.enable_audio && audio_stream_index_ < 0) {
                if (config_.audio_stream_index < 0 || config_.audio_stream_index == i) {
                    audio_stream_index_ = i;
                }
            }
        }
        
        stream_info.is_valid = true;
        media_info_.streams.push_back(stream_info);
    }
    
    // 获取元数据
    AVDictionaryEntry* entry = nullptr;
    while ((entry = av_dict_get(format_ctx_->metadata, "", entry, AV_DICT_IGNORE_SUFFIX))) {
        media_info_.metadata[entry->key] = entry->value;
    }
    
    media_info_.is_valid = true;
    
    LOG_INFO("Analyzed %zu streams, video=%d, audio=%d",
             media_info_.streams.size(), video_stream_index_, audio_stream_index_);
    
    return true;
}

void XDemux::ProcessPacket(AVPacket* packet) {
    if (config_.packet_callback) {
        config_.packet_callback(packet, packet->stream_index);
    }
}

void XDemux::UpdateStats(const AVPacket* packet, int64_t read_time_us) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    stats_.packets_read++;
    stats_.bytes_read += packet->size;
    stats_.total_time_ms += read_time_us / 1000;
    
    if (packet->stream_index == video_stream_index_) {
        stats_.video_packets++;
    } else if (packet->stream_index == audio_stream_index_) {
        stats_.audio_packets++;
    }
}

// XMux实现
XMux::XMux()
    : format_ctx_(nullptr), opened_(false), header_written_(false),
      video_stream_index_(-1), audio_stream_index_(-1) {
}

XMux::~XMux() {
    Close();
}

bool XMux::Open(const MuxConfig& config) {
    if (opened_) {
        LOG_WARN("Muxer already opened");
        return true;
    }

    config_ = config;

    if (config_.filename.empty()) {
        LOG_ERROR("Filename is empty");
        return false;
    }

    // 分配输出格式上下文
    int ret = avformat_alloc_output_context2(&format_ctx_, nullptr,
                                           config_.format_name.empty() ? nullptr : config_.format_name.c_str(),
                                           config_.filename.c_str());
    if (ret < 0) {
        LOG_ERROR("Failed to allocate output context: %s", Utils::AVErrorToString(ret).c_str());
        return false;
    }

    if (!CreateStreams()) {
        LOG_ERROR("Failed to create streams");
        Close();
        return false;
    }

    // 打开输出文件
    if (!(format_ctx_->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&format_ctx_->pb, config_.filename.c_str(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            LOG_ERROR("Failed to open output file: %s", Utils::AVErrorToString(ret).c_str());
            Close();
            return false;
        }
    }

    // 写入文件头
    ret = avformat_write_header(format_ctx_, nullptr);
    if (ret < 0) {
        LOG_ERROR("Failed to write header: %s", Utils::AVErrorToString(ret).c_str());
        Close();
        return false;
    }

    opened_ = true;
    header_written_ = true;
    start_time_ = std::chrono::steady_clock::now();

    LOG_INFO("Muxer opened: %s, format=%s",
             config_.filename.c_str(), format_ctx_->oformat->name);

    return true;
}

void XMux::Close() {
    if (format_ctx_ && header_written_) {
        // 写入文件尾
        av_write_trailer(format_ctx_);
    }

    if (format_ctx_) {
        if (!(format_ctx_->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&format_ctx_->pb);
        }
        avformat_free_context(format_ctx_);
        format_ctx_ = nullptr;
    }

    opened_ = false;
    header_written_ = false;
    video_stream_index_ = -1;
    audio_stream_index_ = -1;

    LOG_INFO("Muxer closed");
}

bool XMux::WritePacket(AVPacket* packet, int stream_index) {
    if (!opened_ || !format_ctx_) {
        LOG_ERROR("Muxer not opened");
        return false;
    }

    if (!packet) {
        LOG_ERROR("Invalid packet pointer");
        return false;
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    // 设置流索引
    packet->stream_index = stream_index;

    // 重新计算时间戳
    AVStream* stream = format_ctx_->streams[stream_index];
    av_packet_rescale_ts(packet, {1, 1000000}, stream->time_base);

    int ret = av_interleaved_write_frame(format_ctx_, packet);
    if (ret < 0) {
        LOG_ERROR("Failed to write packet: %s", Utils::AVErrorToString(ret).c_str());
        return false;
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto write_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

    ProcessPacket(packet, stream_index);
    UpdateStats(packet, write_time.count());

    return true;
}

MuxStats XMux::GetStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    MuxStats stats = stats_;

    // 计算平均写入时间
    if (stats_.packets_written > 0) {
        stats.avg_write_time_ms = stats_.total_time_ms / (double)stats_.packets_written;
    }

    return stats;
}

bool XMux::CreateStreams() {
    if (!format_ctx_) {
        return false;
    }

    // 创建视频流
    if (config_.enable_video) {
        AVStream* video_stream = avformat_new_stream(format_ctx_, nullptr);
        if (!video_stream) {
            LOG_ERROR("Failed to create video stream");
            return false;
        }

        video_stream_index_ = video_stream->index;

        // 配置视频流参数
        AVCodecParameters* codecpar = video_stream->codecpar;
        codecpar->codec_type = AVMEDIA_TYPE_VIDEO;

        switch (config_.video_codec) {
            case CodecType::H264:
                codecpar->codec_id = AV_CODEC_ID_H264;
                break;
            case CodecType::H265:
                codecpar->codec_id = AV_CODEC_ID_HEVC;
                break;
            default:
                codecpar->codec_id = AV_CODEC_ID_H264;
                break;
        }

        codecpar->width = config_.video_width;
        codecpar->height = config_.video_height;
        codecpar->bit_rate = config_.video_bit_rate;

        video_stream->time_base = av_inv_q(config_.video_frame_rate);
        video_stream->r_frame_rate = config_.video_frame_rate;
        video_stream->avg_frame_rate = config_.video_frame_rate;

        LOG_INFO("Created video stream: %dx%d@%.2ffps, codec=%d",
                 config_.video_width, config_.video_height,
                 av_q2d(config_.video_frame_rate), static_cast<int>(config_.video_codec));
    }

    // 创建音频流
    if (config_.enable_audio && config_.audio_codec != CodecType::UNKNOWN) {
        AVStream* audio_stream = avformat_new_stream(format_ctx_, nullptr);
        if (!audio_stream) {
            LOG_ERROR("Failed to create audio stream");
            return false;
        }

        audio_stream_index_ = audio_stream->index;

        // 配置音频流参数
        AVCodecParameters* codecpar = audio_stream->codecpar;
        codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
        codecpar->sample_rate = config_.audio_sample_rate;
        codecpar->ch_layout.nb_channels = config_.audio_channels;
        codecpar->bit_rate = config_.audio_bit_rate;

        audio_stream->time_base = {1, config_.audio_sample_rate};

        LOG_INFO("Created audio stream: %dHz, %dch, codec=%d",
                 config_.audio_sample_rate, config_.audio_channels,
                 static_cast<int>(config_.audio_codec));
    }

    return true;
}

void XMux::ProcessPacket(AVPacket* packet, int stream_index) {
    // 子类可以重写此方法进行额外处理
}

void XMux::UpdateStats(const AVPacket* packet, int64_t write_time_us) {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    stats_.packets_written++;
    stats_.bytes_written += packet->size;
    stats_.total_time_ms += write_time_us / 1000;

    if (packet->stream_index == video_stream_index_) {
        stats_.video_packets++;
    } else if (packet->stream_index == audio_stream_index_) {
        stats_.audio_packets++;
    }
}
