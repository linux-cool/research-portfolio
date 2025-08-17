#include "xrtsp.h"
#include <algorithm>

XRTSPClient::XRTSPClient() 
    : format_ctx_(nullptr), state_(RTSPState::DISCONNECTED),
      running_(false), should_reconnect_(false), max_queue_size_(100) {
}

XRTSPClient::~XRTSPClient() {
    Disconnect();
}

bool XRTSPClient::Connect(const RTSPConfig& config) {
    if (state_ != RTSPState::DISCONNECTED) {
        LOG_WARN("RTSP client already connected or connecting");
        return false;
    }
    
    config_ = config;
    
    if (!RTSPUtils::ValidateURL(config_.url)) {
        LOG_ERROR("Invalid RTSP URL: %s", config_.url.c_str());
        return false;
    }
    
    SetState(RTSPState::CONNECTING);
    
    // 分配格式上下文
    format_ctx_ = avformat_alloc_context();
    if (!format_ctx_) {
        LOG_ERROR("Failed to allocate format context");
        SetState(RTSPState::ERROR);
        return false;
    }
    
    // 设置RTSP选项
    AVDictionary* options = nullptr;
    
    // 设置传输协议
    if (config_.enable_tcp) {
        av_dict_set(&options, "rtsp_transport", "tcp", 0);
    } else {
        av_dict_set(&options, "rtsp_transport", "udp", 0);
    }
    
    // 设置超时
    av_dict_set(&options, "timeout", std::to_string(config_.timeout_ms * 1000).c_str(), 0);
    
    // 设置缓冲区大小
    av_dict_set(&options, "buffer_size", std::to_string(config_.buffer_size).c_str(), 0);
    
    // 设置用户认证
    if (!config_.username.empty()) {
        std::string auth = config_.username + ":" + config_.password;
        av_dict_set(&options, "user_agent", "XRTSPClient/1.0", 0);
    }
    
    // 打开RTSP流
    int ret = avformat_open_input(&format_ctx_, config_.url.c_str(), nullptr, &options);
    av_dict_free(&options);
    
    if (ret < 0) {
        LOG_ERROR("Failed to open RTSP stream: %s", Utils::AVErrorToString(ret).c_str());
        avformat_free_context(format_ctx_);
        format_ctx_ = nullptr;
        SetState(RTSPState::ERROR);
        return false;
    }
    
    // 查找流信息
    ret = avformat_find_stream_info(format_ctx_, nullptr);
    if (ret < 0) {
        LOG_ERROR("Failed to find stream info: %s", Utils::AVErrorToString(ret).c_str());
        Disconnect();
        return false;
    }
    
    // 分析媒体信息
    media_info_.filename = config_.url;
    media_info_.format_name = format_ctx_->iformat->name;
    media_info_.duration_us = format_ctx_->duration;
    media_info_.bit_rate = format_ctx_->bit_rate;
    
    // 分析流
    for (unsigned int i = 0; i < format_ctx_->nb_streams; ++i) {
        AVStream* stream = format_ctx_->streams[i];
        StreamInfo stream_info;
        
        stream_info.index = i;
        stream_info.type = stream->codecpar->codec_type;
        stream_info.time_base = stream->time_base;
        stream_info.bit_rate = stream->codecpar->bit_rate;
        
        // 设置编码类型
        switch (stream->codecpar->codec_id) {
            case AV_CODEC_ID_H264:
                stream_info.codec_type = CodecType::H264;
                stream_info.codec_name = "H.264/AVC";
                break;
            case AV_CODEC_ID_HEVC:
                stream_info.codec_type = CodecType::H265;
                stream_info.codec_name = "H.265/HEVC";
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
        } else if (stream_info.type == AVMEDIA_TYPE_AUDIO) {
            stream_info.sample_rate = stream->codecpar->sample_rate;
            stream_info.channels = stream->codecpar->ch_layout.nb_channels;
        }
        
        stream_info.is_valid = true;
        media_info_.streams.push_back(stream_info);
    }
    
    media_info_.is_valid = true;
    
    SetState(RTSPState::CONNECTED);
    start_time_ = std::chrono::steady_clock::now();
    
    LOG_INFO("RTSP connected: %s, streams=%zu", 
             config_.url.c_str(), media_info_.streams.size());
    
    return true;
}

void XRTSPClient::Disconnect() {
    running_ = false;
    should_reconnect_ = false;
    
    // 等待线程结束
    if (receive_thread_.joinable()) {
        receive_thread_.join();
    }
    
    if (reconnect_thread_.joinable()) {
        reconnect_thread_.join();
    }
    
    // 清理包队列
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        while (!packet_queue_.empty()) {
            AVPacket* packet = packet_queue_.front();
            packet_queue_.pop();
            av_packet_free(&packet);
        }
    }
    
    // 关闭格式上下文
    if (format_ctx_) {
        avformat_close_input(&format_ctx_);
        format_ctx_ = nullptr;
    }
    
    SetState(RTSPState::DISCONNECTED);
    
    LOG_INFO("RTSP disconnected");
}

bool XRTSPClient::Play() {
    if (state_ != RTSPState::CONNECTED && state_ != RTSPState::PAUSED) {
        LOG_ERROR("Cannot play - not connected or invalid state");
        return false;
    }
    
    running_ = true;
    
    // 启动接收线程
    if (!receive_thread_.joinable()) {
        receive_thread_ = std::thread(&XRTSPClient::ReceiveThread, this);
    }
    
    // 启动重连线程（如果启用自动重连）
    if (config_.auto_reconnect && !reconnect_thread_.joinable()) {
        reconnect_thread_ = std::thread(&XRTSPClient::ReconnectThread, this);
    }
    
    SetState(RTSPState::PLAYING);
    
    LOG_INFO("RTSP playback started");
    return true;
}

bool XRTSPClient::Pause() {
    if (state_ != RTSPState::PLAYING) {
        LOG_ERROR("Cannot pause - not playing");
        return false;
    }
    
    SetState(RTSPState::PAUSED);
    
    LOG_INFO("RTSP playback paused");
    return true;
}

bool XRTSPClient::Stop() {
    if (state_ != RTSPState::PLAYING && state_ != RTSPState::PAUSED) {
        LOG_ERROR("Cannot stop - not playing or paused");
        return false;
    }
    
    running_ = false;
    SetState(RTSPState::CONNECTED);
    
    LOG_INFO("RTSP playback stopped");
    return true;
}

RTSPStats XRTSPClient::GetStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    RTSPStats stats = stats_;
    stats.current_state = state_;
    
    // 计算连接时间
    if (state_ != RTSPState::DISCONNECTED) {
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_);
        stats.connection_time_ms = duration.count();
    }
    
    // 计算平均码率
    if (stats.connection_time_ms > 0) {
        stats.avg_bitrate_kbps = (stats.bytes_received * 8.0) / stats.connection_time_ms;
    }
    
    return stats;
}

MediaInfo XRTSPClient::GetMediaInfo() const {
    return media_info_;
}

void XRTSPClient::ReceiveThread() {
    LOG_INFO("RTSP receive thread started");
    
    AVPacket* packet = av_packet_alloc();
    if (!packet) {
        LOG_ERROR("Failed to allocate packet");
        return;
    }
    
    while (running_ && format_ctx_) {
        if (state_ == RTSPState::PAUSED) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        
        int ret = av_read_frame(format_ctx_, packet);
        if (ret < 0) {
            if (ret == AVERROR_EOF) {
                LOG_INFO("RTSP stream ended");
                break;
            } else if (ret == AVERROR(EAGAIN)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            } else {
                LOG_ERROR("Failed to read RTSP frame: %s", Utils::AVErrorToString(ret).c_str());
                should_reconnect_ = config_.auto_reconnect;
                break;
            }
        }
        
        ProcessPacket(packet);
        UpdateStats(packet);
        
        av_packet_unref(packet);
    }
    
    av_packet_free(&packet);
    
    if (should_reconnect_) {
        SetState(RTSPState::ERROR);
    }
    
    LOG_INFO("RTSP receive thread ended");
}

void XRTSPClient::ReconnectThread() {
    LOG_INFO("RTSP reconnect thread started");
    
    while (running_) {
        if (should_reconnect_ && state_ == RTSPState::ERROR) {
            LOG_INFO("Attempting to reconnect...");
            
            if (TryReconnect()) {
                should_reconnect_ = false;
                LOG_INFO("Reconnection successful");
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(config_.reconnect_interval_ms));
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }
    
    LOG_INFO("RTSP reconnect thread ended");
}

void XRTSPClient::ProcessPacket(AVPacket* packet) {
    // 检查队列大小
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (packet_queue_.size() >= max_queue_size_) {
            // 丢弃最旧的包
            AVPacket* old_packet = packet_queue_.front();
            packet_queue_.pop();
            av_packet_free(&old_packet);
            
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            stats_.dropped_packets++;
        }
    }
    
    // 复制包到队列
    AVPacket* packet_copy = av_packet_alloc();
    if (packet_copy && av_packet_ref(packet_copy, packet) >= 0) {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        packet_queue_.push(packet_copy);
        queue_cv_.notify_one();
    }
    
    // 调用回调函数
    if (config_.packet_callback) {
        config_.packet_callback(packet, packet->stream_index);
    }
}

void XRTSPClient::UpdateStats(const AVPacket* packet) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    stats_.packets_received++;
    stats_.bytes_received += packet->size;
    
    // 根据流类型更新统计
    if (packet->stream_index < media_info_.streams.size()) {
        const StreamInfo& stream = media_info_.streams[packet->stream_index];
        if (stream.type == AVMEDIA_TYPE_VIDEO) {
            stats_.video_packets++;
        } else if (stream.type == AVMEDIA_TYPE_AUDIO) {
            stats_.audio_packets++;
        }
    }
}

void XRTSPClient::SetState(RTSPState state) {
    RTSPState old_state = state_;
    state_ = state;
    
    if (old_state != state && config_.state_callback) {
        config_.state_callback(state);
    }
}

bool XRTSPClient::TryReconnect() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    if (stats_.reconnect_count >= config_.max_reconnect_attempts) {
        LOG_ERROR("Max reconnection attempts reached");
        return false;
    }
    
    stats_.reconnect_count++;
    
    // 关闭当前连接
    if (format_ctx_) {
        avformat_close_input(&format_ctx_);
        format_ctx_ = nullptr;
    }
    
    // 尝试重新连接
    RTSPConfig temp_config = config_;
    return Connect(temp_config);
}

// XRTSPRecorder实现
XRTSPRecorder::XRTSPRecorder()
    : recording_(false), paused_(false), current_duration_ms_(0),
      current_file_size_(0), file_sequence_(0) {
}

XRTSPRecorder::~XRTSPRecorder() {
    StopRecord();
}

bool XRTSPRecorder::StartRecord(const RTSPRecordConfig& config) {
    if (recording_) {
        LOG_WARN("Already recording");
        return false;
    }

    config_ = config;

    // 创建RTSP客户端
    rtsp_client_ = std::make_unique<XRTSPClient>();
    if (!rtsp_client_) {
        LOG_ERROR("Failed to create RTSP client");
        return false;
    }

    // 设置包回调
    config_.rtsp_config.packet_callback = [this](AVPacket* packet, int stream_index) {
        if (recording_ && !paused_) {
            ProcessRecordPacket(packet, stream_index);
        }
    };

    // 连接RTSP流
    if (!rtsp_client_->Connect(config_.rtsp_config)) {
        LOG_ERROR("Failed to connect to RTSP stream");
        return false;
    }

    // 创建第一个录制文件
    if (!CreateNewFile()) {
        LOG_ERROR("Failed to create record file");
        return false;
    }

    // 开始播放
    if (!rtsp_client_->Play()) {
        LOG_ERROR("Failed to start RTSP playback");
        return false;
    }

    recording_ = true;
    paused_ = false;

    // 启动录制线程
    record_thread_ = std::thread(&XRTSPRecorder::RecordThread, this);

    LOG_INFO("RTSP recording started: %s -> %s",
             config_.rtsp_config.url.c_str(), current_file_.c_str());

    return true;
}

void XRTSPRecorder::StopRecord() {
    recording_ = false;

    if (record_thread_.joinable()) {
        record_thread_.join();
    }

    if (rtsp_client_) {
        rtsp_client_->Disconnect();
        rtsp_client_.reset();
    }

    if (muxer_) {
        muxer_->Close();
        muxer_.reset();
    }

    // 调用文件完成回调
    if (!current_file_.empty() && config_.file_completed_callback) {
        config_.file_completed_callback(current_file_);
    }

    LOG_INFO("RTSP recording stopped");
}

bool XRTSPRecorder::PauseRecord() {
    if (!recording_) {
        LOG_ERROR("Not recording");
        return false;
    }

    paused_ = true;
    LOG_INFO("RTSP recording paused");
    return true;
}

bool XRTSPRecorder::ResumeRecord() {
    if (!recording_ || !paused_) {
        LOG_ERROR("Not paused");
        return false;
    }

    paused_ = false;
    LOG_INFO("RTSP recording resumed");
    return true;
}

RTSPStats XRTSPRecorder::GetRecordStats() const {
    if (rtsp_client_) {
        return rtsp_client_->GetStats();
    }
    return RTSPStats();
}

void XRTSPRecorder::RecordThread() {
    LOG_INFO("RTSP record thread started");

    while (recording_) {
        // 检查是否需要分割文件
        if (ShouldSplitFile()) {
            // 完成当前文件
            if (muxer_) {
                muxer_->Close();

                if (config_.file_completed_callback) {
                    config_.file_completed_callback(current_file_);
                }
            }

            // 创建新文件
            if (!CreateNewFile()) {
                LOG_ERROR("Failed to create new record file");
                break;
            }
        }

        // 更新进度
        if (config_.progress_callback) {
            config_.progress_callback(current_duration_ms_, current_file_size_);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    LOG_INFO("RTSP record thread ended");
}

void XRTSPRecorder::ProcessRecordPacket(AVPacket* packet, int stream_index) {
    std::lock_guard<std::mutex> lock(record_mutex_);

    if (!muxer_ || !muxer_->IsOpened()) {
        return;
    }

    // 写入包
    if (muxer_->WritePacket(packet, stream_index)) {
        current_file_size_ += packet->size;

        // 估算时长（简化处理）
        if (stream_index == muxer_->GetVideoStreamIndex()) {
            current_duration_ms_ += 40; // 假设25fps，每帧40ms
        }
    }
}

bool XRTSPRecorder::ShouldSplitFile() {
    // 检查最大时长
    if (config_.max_duration_ms > 0 && current_duration_ms_ >= config_.max_duration_ms) {
        return true;
    }

    // 检查最大文件大小
    if (config_.max_file_size > 0 && current_file_size_ >= config_.max_file_size) {
        return true;
    }

    return false;
}

bool XRTSPRecorder::CreateNewFile() {
    // 生成文件名
    current_file_ = RTSPUtils::GenerateFileName(config_.output_file, file_sequence_++);

    // 创建封装器
    std::string format = config_.output_format;
    if (format.empty()) {
        // 简单的格式检测
        if (current_file_.find(".mp4") != std::string::npos) {
            format = "mp4";
        } else if (current_file_.find(".avi") != std::string::npos) {
            format = "avi";
        } else if (current_file_.find(".mkv") != std::string::npos) {
            format = "matroska";
        } else {
            format = "mp4"; // 默认格式
        }
    }

    muxer_ = std::make_unique<XMux>();
    if (!muxer_) {
        LOG_ERROR("Failed to create muxer for format: %s", format.c_str());
        return false;
    }

    // 配置封装器
    MuxConfig mux_config;
    mux_config.filename = current_file_;
    mux_config.format_name = format;

    // 从RTSP流获取媒体信息
    MediaInfo media_info = rtsp_client_->GetMediaInfo();

    // 配置视频流
    for (const auto& stream : media_info.streams) {
        if (stream.type == AVMEDIA_TYPE_VIDEO && config_.rtsp_config.enable_video) {
            mux_config.enable_video = true;
            mux_config.video_codec = stream.codec_type;
            mux_config.video_width = stream.width;
            mux_config.video_height = stream.height;
            mux_config.video_frame_rate = stream.frame_rate;
            mux_config.video_bit_rate = stream.bit_rate;
            break;
        }
    }

    // 配置音频流
    for (const auto& stream : media_info.streams) {
        if (stream.type == AVMEDIA_TYPE_AUDIO && config_.rtsp_config.enable_audio) {
            mux_config.enable_audio = true;
            mux_config.audio_codec = stream.codec_type;
            mux_config.audio_sample_rate = stream.sample_rate;
            mux_config.audio_channels = stream.channels;
            mux_config.audio_bit_rate = stream.bit_rate;
            break;
        }
    }

    if (!muxer_->Open(mux_config)) {
        LOG_ERROR("Failed to open muxer: %s", current_file_.c_str());
        return false;
    }

    // 重置计数器
    current_duration_ms_ = 0;
    current_file_size_ = 0;

    LOG_INFO("Created new record file: %s", current_file_.c_str());
    return true;
}

// XRTSPMultiRecorder实现
XRTSPMultiRecorder::XRTSPMultiRecorder() {
}

XRTSPMultiRecorder::~XRTSPMultiRecorder() {
    StopAllRecords();
}

bool XRTSPMultiRecorder::AddRecordTask(const std::string& id, const RTSPRecordConfig& config) {
    std::lock_guard<std::mutex> lock(recorders_mutex_);

    if (recorders_.find(id) != recorders_.end()) {
        LOG_WARN("Record task already exists: %s", id.c_str());
        return false;
    }

    auto recorder = std::make_unique<XRTSPRecorder>();
    if (!recorder) {
        LOG_ERROR("Failed to create recorder for task: %s", id.c_str());
        return false;
    }

    recorders_[id] = std::move(recorder);

    LOG_INFO("Added record task: %s", id.c_str());
    return true;
}

bool XRTSPMultiRecorder::RemoveRecordTask(const std::string& id) {
    std::lock_guard<std::mutex> lock(recorders_mutex_);

    auto it = recorders_.find(id);
    if (it == recorders_.end()) {
        LOG_WARN("Record task not found: %s", id.c_str());
        return false;
    }

    // 停止录制
    it->second->StopRecord();

    // 移除任务
    recorders_.erase(it);

    LOG_INFO("Removed record task: %s", id.c_str());
    return true;
}

bool XRTSPMultiRecorder::StartAllRecords() {
    std::lock_guard<std::mutex> lock(recorders_mutex_);

    bool all_success = true;

    for (auto& [id, recorder] : recorders_) {
        // 这里需要为每个录制器配置不同的参数
        // 实际使用时应该在AddRecordTask时保存配置
        LOG_WARN("StartAllRecords needs individual configs for each task");
        // if (!recorder->StartRecord(config)) {
        //     LOG_ERROR("Failed to start record task: %s", id.c_str());
        //     all_success = false;
        // }
    }

    return all_success;
}

void XRTSPMultiRecorder::StopAllRecords() {
    std::lock_guard<std::mutex> lock(recorders_mutex_);

    for (auto& [id, recorder] : recorders_) {
        recorder->StopRecord();
        LOG_INFO("Stopped record task: %s", id.c_str());
    }
}

std::vector<std::string> XRTSPMultiRecorder::GetTaskIds() const {
    std::lock_guard<std::mutex> lock(recorders_mutex_);

    std::vector<std::string> ids;
    for (const auto& [id, recorder] : recorders_) {
        ids.push_back(id);
    }

    return ids;
}

RTSPStats XRTSPMultiRecorder::GetTaskStats(const std::string& id) const {
    std::lock_guard<std::mutex> lock(recorders_mutex_);

    auto it = recorders_.find(id);
    if (it != recorders_.end()) {
        return it->second->GetRecordStats();
    }

    return RTSPStats();
}

// RTSPUtils实现
bool RTSPUtils::ParseURL(const std::string& url, std::string& host, int& port, std::string& path) {
    // 简化的URL解析
    if (url.substr(0, 7) != "rtsp://") {
        return false;
    }

    std::string remaining = url.substr(7); // 去掉 "rtsp://"

    // 查找路径分隔符
    size_t path_pos = remaining.find('/');
    std::string host_port;

    if (path_pos != std::string::npos) {
        host_port = remaining.substr(0, path_pos);
        path = remaining.substr(path_pos);
    } else {
        host_port = remaining;
        path = "/";
    }

    // 解析主机和端口
    size_t port_pos = host_port.find(':');
    if (port_pos != std::string::npos) {
        host = host_port.substr(0, port_pos);
        try {
            port = std::stoi(host_port.substr(port_pos + 1));
        } catch (...) {
            return false;
        }
    } else {
        host = host_port;
        port = 554; // RTSP默认端口
    }

    return !host.empty();
}

bool RTSPUtils::ValidateURL(const std::string& url) {
    std::string host, path;
    int port;
    return ParseURL(url, host, port, path);
}

bool RTSPUtils::TestConnection(const std::string& url, int timeout_ms) {
    AVFormatContext* format_ctx = avformat_alloc_context();
    if (!format_ctx) {
        return false;
    }

    AVDictionary* options = nullptr;
    av_dict_set(&options, "timeout", std::to_string(timeout_ms * 1000).c_str(), 0);
    av_dict_set(&options, "rtsp_transport", "tcp", 0);

    int ret = avformat_open_input(&format_ctx, url.c_str(), nullptr, &options);
    av_dict_free(&options);

    if (ret >= 0) {
        avformat_close_input(&format_ctx);
        return true;
    } else {
        avformat_free_context(format_ctx);
        return false;
    }
}

MediaInfo RTSPUtils::GetStreamInfo(const std::string& url, int timeout_ms) {
    MediaInfo info;

    AVFormatContext* format_ctx = avformat_alloc_context();
    if (!format_ctx) {
        return info;
    }

    AVDictionary* options = nullptr;
    av_dict_set(&options, "timeout", std::to_string(timeout_ms * 1000).c_str(), 0);
    av_dict_set(&options, "rtsp_transport", "tcp", 0);

    int ret = avformat_open_input(&format_ctx, url.c_str(), nullptr, &options);
    av_dict_free(&options);

    if (ret < 0) {
        avformat_free_context(format_ctx);
        return info;
    }

    ret = avformat_find_stream_info(format_ctx, nullptr);
    if (ret < 0) {
        avformat_close_input(&format_ctx);
        return info;
    }

    // 填充媒体信息
    info.filename = url;
    info.format_name = format_ctx->iformat->name;
    info.duration_us = format_ctx->duration;
    info.bit_rate = format_ctx->bit_rate;

    for (unsigned int i = 0; i < format_ctx->nb_streams; ++i) {
        AVStream* stream = format_ctx->streams[i];
        StreamInfo stream_info;

        stream_info.index = i;
        stream_info.type = stream->codecpar->codec_type;
        stream_info.time_base = stream->time_base;
        stream_info.bit_rate = stream->codecpar->bit_rate;
        stream_info.codec_name = avcodec_get_name(stream->codecpar->codec_id);

        if (stream_info.type == AVMEDIA_TYPE_VIDEO) {
            stream_info.width = stream->codecpar->width;
            stream_info.height = stream->codecpar->height;
            stream_info.frame_rate = av_guess_frame_rate(format_ctx, stream, nullptr);
        } else if (stream_info.type == AVMEDIA_TYPE_AUDIO) {
            stream_info.sample_rate = stream->codecpar->sample_rate;
            stream_info.channels = stream->codecpar->ch_layout.nb_channels;
        }

        stream_info.is_valid = true;
        info.streams.push_back(stream_info);
    }

    info.is_valid = true;

    avformat_close_input(&format_ctx);
    return info;
}

std::string RTSPUtils::GenerateFileName(const std::string& base_name, int sequence, int64_t timestamp) {
    std::string result = base_name;

    // 如果没有扩展名，添加默认扩展名
    if (result.find('.') == std::string::npos) {
        result += ".mp4";
    }

    // 插入序列号和时间戳
    size_t dot_pos = result.find_last_of('.');
    std::string name_part = result.substr(0, dot_pos);
    std::string ext_part = result.substr(dot_pos);

    if (sequence > 0) {
        name_part += "_" + std::to_string(sequence);
    }

    if (timestamp > 0) {
        name_part += "_" + std::to_string(timestamp);
    }

    return name_part + ext_part;
}
