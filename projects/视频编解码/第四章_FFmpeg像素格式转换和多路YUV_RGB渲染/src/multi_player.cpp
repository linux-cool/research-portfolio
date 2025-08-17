#include "multi_player.h"
#include <algorithm>

// VideoPlayer实现
VideoPlayer::VideoPlayer(int player_id) 
    : player_id_(player_id), state_(State::STOPPED),
      format_ctx_(nullptr), codec_ctx_(nullptr), video_stream_index_(-1),
      playback_speed_(1.0), start_time_(0), current_pts_(0) {
}

VideoPlayer::~VideoPlayer() {
    Close();
}

bool VideoPlayer::Open(const std::string& filename) {
    std::lock_guard<std::mutex> lock(player_mutex_);
    
    Close();
    
    // 打开文件
    format_ctx_ = avformat_alloc_context();
    if (avformat_open_input(&format_ctx_, filename.c_str(), nullptr, nullptr) < 0) {
        LOG_ERROR("Failed to open file: %s", filename.c_str());
        return false;
    }
    
    // 查找流信息
    if (avformat_find_stream_info(format_ctx_, nullptr) < 0) {
        LOG_ERROR("Failed to find stream info");
        CleanupDecoder();
        return false;
    }
    
    // 查找视频流
    video_stream_index_ = av_find_best_stream(format_ctx_, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (video_stream_index_ < 0) {
        LOG_ERROR("No video stream found");
        CleanupDecoder();
        return false;
    }
    
    // 初始化解码器
    if (!InitializeDecoder()) {
        CleanupDecoder();
        return false;
    }
    
    // 填充视频信息
    AVStream* video_stream = format_ctx_->streams[video_stream_index_];
    video_info_.filename = filename;
    video_info_.width = codec_ctx_->width;
    video_info_.height = codec_ctx_->height;
    video_info_.format = codec_ctx_->pix_fmt;
    video_info_.frame_rate = video_stream->r_frame_rate;
    video_info_.time_base = video_stream->time_base;
    video_info_.duration = format_ctx_->duration;
    video_info_.is_valid = true;
    
    state_ = State::STOPPED;
    
    LOG_INFO("Player %d opened: %s (%dx%d)", player_id_, filename.c_str(),
             video_info_.width, video_info_.height);
    
    return true;
}

void VideoPlayer::Close() {
    std::lock_guard<std::mutex> lock(player_mutex_);
    
    state_ = State::STOPPED;
    CleanupDecoder();
    video_info_ = VideoStreamInfo();
    current_pts_ = 0;
}

bool VideoPlayer::Play() {
    std::lock_guard<std::mutex> lock(player_mutex_);
    
    if (state_ == State::ERROR || !video_info_.is_valid) {
        return false;
    }
    
    state_ = State::PLAYING;
    start_time_ = Utils::GetCurrentTimeMs();
    
    LOG_INFO("Player %d started playing", player_id_);
    return true;
}

void VideoPlayer::Pause() {
    std::lock_guard<std::mutex> lock(player_mutex_);
    
    if (state_ == State::PLAYING) {
        state_ = State::PAUSED;
        LOG_INFO("Player %d paused", player_id_);
    }
}

void VideoPlayer::Stop() {
    std::lock_guard<std::mutex> lock(player_mutex_);
    
    state_ = State::STOPPED;
    current_pts_ = 0;
    
    // 重新定位到开始
    if (format_ctx_) {
        av_seek_frame(format_ctx_, video_stream_index_, 0, AVSEEK_FLAG_BACKWARD);
    }
    
    LOG_INFO("Player %d stopped", player_id_);
}

bool VideoPlayer::Seek(int64_t timestamp_us) {
    std::lock_guard<std::mutex> lock(player_mutex_);
    
    if (!format_ctx_ || video_stream_index_ < 0) {
        return false;
    }
    
    // 转换时间戳
    AVStream* stream = format_ctx_->streams[video_stream_index_];
    int64_t seek_target = av_rescale_q(timestamp_us, {1, 1000000}, stream->time_base);
    
    int ret = av_seek_frame(format_ctx_, video_stream_index_, seek_target, AVSEEK_FLAG_BACKWARD);
    if (ret < 0) {
        LOG_ERROR("Seek failed: %s", Utils::AVErrorToString(ret).c_str());
        return false;
    }
    
    current_pts_ = timestamp_us;
    LOG_INFO("Player %d seeked to %ld us", player_id_, timestamp_us);
    
    return true;
}

void VideoPlayer::SetPlaybackSpeed(double speed) {
    std::lock_guard<std::mutex> lock(player_mutex_);
    playback_speed_ = speed;
    LOG_INFO("Player %d speed set to %.2fx", player_id_, speed);
}

bool VideoPlayer::GetNextFrame(AVFrame* frame) {
    std::lock_guard<std::mutex> lock(player_mutex_);
    
    if (state_ != State::PLAYING || !frame) {
        return false;
    }
    
    return ReadPacketAndDecode(frame);
}

int64_t VideoPlayer::GetCurrentTime() const {
    std::lock_guard<std::mutex> lock(player_mutex_);
    return current_pts_;
}

bool VideoPlayer::InitializeDecoder() {
    AVStream* video_stream = format_ctx_->streams[video_stream_index_];
    
    // 查找解码器
    const AVCodec* codec = avcodec_find_decoder(video_stream->codecpar->codec_id);
    if (!codec) {
        LOG_ERROR("Decoder not found");
        return false;
    }
    
    // 创建解码器上下文
    codec_ctx_ = avcodec_alloc_context3(codec);
    if (!codec_ctx_) {
        LOG_ERROR("Failed to allocate codec context");
        return false;
    }
    
    // 复制参数
    if (avcodec_parameters_to_context(codec_ctx_, video_stream->codecpar) < 0) {
        LOG_ERROR("Failed to copy codec parameters");
        return false;
    }
    
    // 打开解码器
    if (avcodec_open2(codec_ctx_, codec, nullptr) < 0) {
        LOG_ERROR("Failed to open codec");
        return false;
    }
    
    return true;
}

void VideoPlayer::CleanupDecoder() {
    if (codec_ctx_) {
        avcodec_free_context(&codec_ctx_);
        codec_ctx_ = nullptr;
    }
    
    if (format_ctx_) {
        avformat_close_input(&format_ctx_);
        format_ctx_ = nullptr;
    }
    
    video_stream_index_ = -1;
}

bool VideoPlayer::ReadPacketAndDecode(AVFrame* frame) {
    AVPacket packet;
    av_init_packet(&packet);
    
    while (av_read_frame(format_ctx_, &packet) >= 0) {
        if (packet.stream_index == video_stream_index_) {
            // 发送包到解码器
            int ret = avcodec_send_packet(codec_ctx_, &packet);
            if (ret < 0) {
                av_packet_unref(&packet);
                continue;
            }
            
            // 接收解码后的帧
            ret = avcodec_receive_frame(codec_ctx_, frame);
            if (ret == 0) {
                // 更新当前时间戳
                if (frame->pts != AV_NOPTS_VALUE) {
                    AVStream* stream = format_ctx_->streams[video_stream_index_];
                    current_pts_ = av_rescale_q(frame->pts, stream->time_base, {1, 1000000});
                }
                
                av_packet_unref(&packet);
                return true;
            }
        }
        
        av_packet_unref(&packet);
    }
    
    // 到达文件末尾
    state_ = State::STOPPED;
    return false;
}

// MultiVideoPlayer实现
MultiVideoPlayer::MultiVideoPlayer(size_t max_players)
    : max_players_(max_players), render_width_(0), render_height_(0),
      rendering_(false), sync_enabled_(false), sync_start_time_(0) {

    frame_manager_ = std::make_shared<AVFrameManager>(max_players * 5);
    format_converter_ = std::make_unique<SwsConverter>();

    LOG_INFO("MultiVideoPlayer created with max %zu players", max_players);
}

MultiVideoPlayer::~MultiVideoPlayer() {
    StopRendering();
    StopAll();
    LOG_INFO("MultiVideoPlayer destroyed");
}

int MultiVideoPlayer::AddPlayer(const PlayerConfig& config) {
    if (players_.size() >= max_players_) {
        LOG_ERROR("Maximum number of players reached");
        return -1;
    }

    int player_id = config.player_id;
    if (player_id < 0) {
        // 自动分配ID
        player_id = 0;
        while (players_.find(player_id) != players_.end()) {
            player_id++;
        }
    }

    auto player = std::make_unique<VideoPlayer>(player_id);
    if (!player->Open(config.filename)) {
        LOG_ERROR("Failed to open file: %s", config.filename.c_str());
        return -1;
    }

    players_[player_id] = std::move(player);
    player_configs_[player_id] = config;
    player_configs_[player_id].player_id = player_id;

    // 初始化统计信息
    PlayerStats stats;
    stats.player_id = player_id;
    stats.state = VideoPlayer::State::STOPPED;
    stats.current_time_us = 0;
    stats.fps = 0.0;
    stats.frames_rendered = 0;
    stats.frames_dropped = 0;

    UpdatePlayerStats(player_id, stats);

    LOG_INFO("Player %d added: %s", player_id, config.filename.c_str());
    return player_id;
}

bool MultiVideoPlayer::RemovePlayer(int player_id) {
    auto it = players_.find(player_id);
    if (it == players_.end()) {
        return false;
    }

    it->second->Stop();
    players_.erase(it);
    player_configs_.erase(player_id);

    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        player_stats_.erase(player_id);
    }

    LOG_INFO("Player %d removed", player_id);
    return true;
}

bool MultiVideoPlayer::StartAll() {
    bool success = true;

    for (auto& [player_id, player] : players_) {
        if (!player->Play()) {
            LOG_ERROR("Failed to start player %d", player_id);
            success = false;
        }
    }

    if (sync_enabled_) {
        std::lock_guard<std::mutex> lock(sync_mutex_);
        sync_start_time_ = Utils::GetCurrentTimeMs() * 1000; // 转换为微秒
    }

    LOG_INFO("Started %zu players", players_.size());
    return success;
}

void MultiVideoPlayer::PauseAll() {
    for (auto& [player_id, player] : players_) {
        player->Pause();
    }
    LOG_INFO("Paused all players");
}

void MultiVideoPlayer::StopAll() {
    for (auto& [player_id, player] : players_) {
        player->Stop();
    }
    LOG_INFO("Stopped all players");
}

bool MultiVideoPlayer::SyncAll(int64_t timestamp_us) {
    bool success = true;

    for (auto& [player_id, player] : players_) {
        if (!player->Seek(timestamp_us)) {
            LOG_ERROR("Failed to sync player %d", player_id);
            success = false;
        }
    }

    if (success && sync_enabled_) {
        std::lock_guard<std::mutex> lock(sync_mutex_);
        sync_start_time_ = Utils::GetCurrentTimeMs() * 1000 - timestamp_us;
    }

    LOG_INFO("Synced all players to %ld us", timestamp_us);
    return success;
}

bool MultiVideoPlayer::SetRenderer(std::unique_ptr<XVideoView> renderer, int width, int height) {
    if (!renderer) {
        return false;
    }

    if (!renderer->Init(width, height, PixelFormat::RGB24)) {
        LOG_ERROR("Failed to initialize renderer");
        return false;
    }

    main_renderer_ = std::move(renderer);
    render_width_ = width;
    render_height_ = height;

    LOG_INFO("Renderer set: %dx%d", width, height);
    return true;
}

bool MultiVideoPlayer::StartRendering() {
    if (!main_renderer_ || rendering_) {
        return false;
    }

    rendering_ = true;
    render_thread_ = std::make_unique<std::thread>(&MultiVideoPlayer::RenderLoop, this);

    LOG_INFO("Rendering started");
    return true;
}

void MultiVideoPlayer::StopRendering() {
    if (!rendering_) {
        return;
    }

    rendering_ = false;

    if (render_thread_ && render_thread_->joinable()) {
        render_thread_->join();
    }

    render_thread_.reset();
    LOG_INFO("Rendering stopped");
}

std::vector<MultiVideoPlayer::PlayerStats> MultiVideoPlayer::GetPlayersStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    std::vector<PlayerStats> stats;
    for (const auto& [player_id, stat] : player_stats_) {
        stats.push_back(stat);
    }

    return stats;
}

void MultiVideoPlayer::RenderLoop() {
    LOG_INFO("Render loop started");

    while (rendering_) {
        if (!RenderFrame()) {
            Utils::SleepMs(16); // ~60fps
        }
    }

    LOG_INFO("Render loop stopped");
}

bool MultiVideoPlayer::RenderFrame() {
    // 获取所有播放器的当前帧
    std::map<int, AVFrame*> player_frames;

    for (auto& [player_id, player] : players_) {
        AVFrame* frame = frame_manager_->AllocFrame(
            player->GetVideoInfo().width,
            player->GetVideoInfo().height,
            player->GetVideoInfo().format
        );

        if (frame && player->GetNextFrame(frame)) {
            player_frames[player_id] = frame;
        } else if (frame) {
            frame_manager_->ReleaseFrame(frame);
        }
    }

    if (player_frames.empty()) {
        return false;
    }

    // 创建输出帧
    AVFrame* output_frame = frame_manager_->AllocFrame(render_width_, render_height_, AV_PIX_FMT_RGB24);
    if (!output_frame) {
        // 清理帧
        for (auto& [player_id, frame] : player_frames) {
            frame_manager_->ReleaseFrame(frame);
        }
        return false;
    }

    // 合成帧
    bool success = CompositeFrames(player_frames, output_frame);

    if (success) {
        // 渲染到屏幕
        main_renderer_->Render(output_frame);
    }

    // 清理帧
    for (auto& [player_id, frame] : player_frames) {
        frame_manager_->ReleaseFrame(frame);
    }
    frame_manager_->ReleaseFrame(output_frame);

    return success;
}

bool MultiVideoPlayer::CompositeFrames(const std::map<int, AVFrame*>& frames, AVFrame* output_frame) {
    // 清空输出帧
    memset(output_frame->data[0], 0, output_frame->linesize[0] * output_frame->height);

    for (const auto& [player_id, frame] : frames) {
        auto config_it = player_configs_.find(player_id);
        if (config_it == player_configs_.end()) {
            continue;
        }

        const PlayerConfig& config = config_it->second;

        // 转换为RGB格式
        AVFrame* rgb_frame = frame_manager_->AllocFrame(frame->width, frame->height, AV_PIX_FMT_RGB24);
        if (!rgb_frame) {
            continue;
        }

        SwsConverter::Config conv_config;
        conv_config.src_width = frame->width;
        conv_config.src_height = frame->height;
        conv_config.src_format = static_cast<AVPixelFormat>(frame->format);
        conv_config.dst_width = frame->width;
        conv_config.dst_height = frame->height;
        conv_config.dst_format = AV_PIX_FMT_RGB24;

        if (format_converter_->Init(conv_config) &&
            format_converter_->Convert(frame, rgb_frame)) {

            // 将RGB帧复制到输出帧的指定位置
            int dst_x = config.render_x;
            int dst_y = config.render_y;
            int dst_w = config.render_width > 0 ? config.render_width : frame->width;
            int dst_h = config.render_height > 0 ? config.render_height : frame->height;

            // 简单的像素复制（不做缩放）
            for (int y = 0; y < std::min(dst_h, frame->height); ++y) {
                if (dst_y + y >= output_frame->height) break;

                for (int x = 0; x < std::min(dst_w, frame->width); ++x) {
                    if (dst_x + x >= output_frame->width) break;

                    int src_offset = y * rgb_frame->linesize[0] + x * 3;
                    int dst_offset = (dst_y + y) * output_frame->linesize[0] + (dst_x + x) * 3;

                    memcpy(output_frame->data[0] + dst_offset,
                           rgb_frame->data[0] + src_offset, 3);
                }
            }
        }

        frame_manager_->ReleaseFrame(rgb_frame);
    }

    return true;
}

void MultiVideoPlayer::UpdatePlayerStats(int player_id, const PlayerStats& stats) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    player_stats_[player_id] = stats;
}

int64_t MultiVideoPlayer::GetSyncTime() const {
    if (!sync_enabled_) {
        return 0;
    }

    // 注意：这里不能加锁，因为是const方法
    int64_t current_time = Utils::GetCurrentTimeMs() * 1000; // 转换为微秒
    return current_time - sync_start_time_;
}

// MultiPlayerFactory实现
std::unique_ptr<MultiVideoPlayer> MultiPlayerFactory::CreateMultiPlayer(
    size_t max_players, XVideoViewFactory::RendererType renderer_type) {

    auto multi_player = std::make_unique<MultiVideoPlayer>(max_players);

    // 创建渲染器
    auto renderer = XVideoViewFactory::Create(renderer_type);
    if (!renderer) {
        LOG_ERROR("Failed to create renderer");
        return nullptr;
    }

    // 设置默认渲染尺寸
    if (!multi_player->SetRenderer(std::move(renderer), 1280, 720)) {
        LOG_ERROR("Failed to set renderer");
        return nullptr;
    }

    return multi_player;
}

std::unique_ptr<MultiVideoPlayer> MultiPlayerFactory::CreateWithLayout(
    const std::string& layout, const std::vector<std::string>& filenames,
    int total_width, int total_height) {

    auto multi_player = CreateMultiPlayer(filenames.size());
    if (!multi_player) {
        return nullptr;
    }

    // 根据布局计算每个播放器的位置和大小
    std::vector<MultiVideoPlayer::PlayerConfig> configs;

    if (layout == "2x2" && filenames.size() <= 4) {
        int w = total_width / 2;
        int h = total_height / 2;

        for (size_t i = 0; i < filenames.size(); ++i) {
            MultiVideoPlayer::PlayerConfig config;
            config.player_id = static_cast<int>(i);
            config.filename = filenames[i];
            config.render_x = (i % 2) * w;
            config.render_y = (i / 2) * h;
            config.render_width = w;
            config.render_height = h;
            configs.push_back(config);
        }
    } else if (layout == "1+3" && filenames.size() <= 4) {
        // 主窗口占左半边，其他3个占右半边
        int main_w = total_width / 2;
        int sub_w = total_width / 2;
        int sub_h = total_height / 3;

        for (size_t i = 0; i < filenames.size(); ++i) {
            MultiVideoPlayer::PlayerConfig config;
            config.player_id = static_cast<int>(i);
            config.filename = filenames[i];

            if (i == 0) {
                // 主窗口
                config.render_x = 0;
                config.render_y = 0;
                config.render_width = main_w;
                config.render_height = total_height;
            } else {
                // 子窗口
                config.render_x = main_w;
                config.render_y = (i - 1) * sub_h;
                config.render_width = sub_w;
                config.render_height = sub_h;
            }
            configs.push_back(config);
        }
    } else if (layout == "4x1" && filenames.size() <= 4) {
        int w = total_width / 4;

        for (size_t i = 0; i < filenames.size(); ++i) {
            MultiVideoPlayer::PlayerConfig config;
            config.player_id = static_cast<int>(i);
            config.filename = filenames[i];
            config.render_x = i * w;
            config.render_y = 0;
            config.render_width = w;
            config.render_height = total_height;
            configs.push_back(config);
        }
    } else {
        LOG_ERROR("Unsupported layout: %s", layout.c_str());
        return nullptr;
    }

    // 添加播放器
    for (const auto& config : configs) {
        if (multi_player->AddPlayer(config) < 0) {
            LOG_ERROR("Failed to add player for: %s", config.filename.c_str());
            return nullptr;
        }
    }

    LOG_INFO("Created multi-player with layout: %s", layout.c_str());
    return multi_player;
}
