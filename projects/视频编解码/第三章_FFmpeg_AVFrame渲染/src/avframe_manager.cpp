#include "avframe_manager.h"
#include <algorithm>
#include <chrono>

AVFrameManager::AVFrameManager(size_t pool_size) 
    : max_pool_size_(pool_size), peak_usage_(0) {
    frame_pool_.reserve(pool_size);
    LOG_INFO("AVFrameManager created with pool size: %zu", pool_size);
}

AVFrameManager::~AVFrameManager() {
    Clear();
    LOG_INFO("AVFrameManager destroyed");
}

AVFrame* AVFrameManager::AllocFrame(int width, int height, AVPixelFormat format) {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    // 首先尝试从池中找到可用的frame
    FrameInfo* frame_info = FindAvailableFrame(width, height, format);
    
    if (frame_info) {
        frame_info->in_use = true;
        frame_info->last_used_time = Utils::GetCurrentTimeMs();
        return frame_info->frame;
    }
    
    // 如果池中没有合适的frame，创建新的
    if (frame_pool_.size() < max_pool_size_) {
        AVFrame* new_frame = CreateNewFrame(width, height, format);
        if (new_frame) {
            auto info = std::make_unique<FrameInfo>();
            info->frame = new_frame;
            info->width = width;
            info->height = height;
            info->format = format;
            info->in_use = true;
            info->last_used_time = Utils::GetCurrentTimeMs();
            
            frame_pool_.push_back(std::move(info));
            
            // 更新峰值使用量
            size_t current_usage = frame_pool_.size();
            if (current_usage > peak_usage_) {
                peak_usage_ = current_usage;
            }
            
            return new_frame;
        }
    }
    
    // 如果池已满，清理旧的frame后重试
    CleanupOldFrames();
    frame_info = FindAvailableFrame(width, height, format);
    if (frame_info) {
        frame_info->in_use = true;
        frame_info->last_used_time = Utils::GetCurrentTimeMs();
        return frame_info->frame;
    }
    
    LOG_ERROR("Failed to allocate AVFrame: %dx%d, format=%d", width, height, format);
    return nullptr;
}

void AVFrameManager::ReleaseFrame(AVFrame* frame) {
    if (!frame) return;
    
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    for (auto& info : frame_pool_) {
        if (info->frame == frame) {
            info->in_use = false;
            info->last_used_time = Utils::GetCurrentTimeMs();
            
            // 清理frame数据但保留缓冲区
            av_frame_unref(frame);
            return;
        }
    }
    
    LOG_WARN("Attempted to release unknown AVFrame");
}

AVFrame* AVFrameManager::CloneFrame(const AVFrame* src) {
    if (!src) return nullptr;
    
    AVFrame* dst = AllocFrame(src->width, src->height, 
                             static_cast<AVPixelFormat>(src->format));
    if (!dst) {
        return nullptr;
    }
    
    int ret = av_frame_copy(dst, src);
    if (ret < 0) {
        LOG_ERROR("Failed to copy AVFrame: %s", Utils::AVErrorToString(ret).c_str());
        ReleaseFrame(dst);
        return nullptr;
    }
    
    // 复制时间戳和其他元数据
    dst->pts = src->pts;
    dst->pkt_dts = src->pkt_dts;
    dst->time_base = src->time_base;
    dst->pict_type = src->pict_type;
    // key_frame字段在新版本FFmpeg中已移除
    
    return dst;
}

AVFrameManager::PoolStats AVFrameManager::GetStats() const {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    PoolStats stats;
    stats.total_frames = frame_pool_.size();
    stats.available_frames = 0;
    stats.allocated_frames = 0;
    stats.peak_usage = peak_usage_;
    
    for (const auto& info : frame_pool_) {
        if (info->in_use) {
            stats.allocated_frames++;
        } else {
            stats.available_frames++;
        }
    }
    
    return stats;
}

void AVFrameManager::Clear() {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    for (auto& info : frame_pool_) {
        if (info->frame) {
            av_frame_free(&info->frame);
        }
    }
    
    frame_pool_.clear();
    peak_usage_ = 0;
    
    LOG_INFO("AVFrame pool cleared");
}

AVFrame* AVFrameManager::CreateNewFrame(int width, int height, AVPixelFormat format) {
    AVFrame* frame = av_frame_alloc();
    if (!frame) {
        LOG_ERROR("Failed to allocate AVFrame");
        return nullptr;
    }
    
    frame->width = width;
    frame->height = height;
    frame->format = format;
    
    int ret = av_frame_get_buffer(frame, 32); // 32字节对齐
    if (ret < 0) {
        LOG_ERROR("Failed to allocate AVFrame buffer: %s", 
                 Utils::AVErrorToString(ret).c_str());
        av_frame_free(&frame);
        return nullptr;
    }
    
    return frame;
}

AVFrameManager::FrameInfo* AVFrameManager::FindAvailableFrame(int width, int height, 
                                                             AVPixelFormat format) {
    for (auto& info : frame_pool_) {
        if (!info->in_use && 
            info->width == width && 
            info->height == height && 
            info->format == format) {
            return info.get();
        }
    }
    return nullptr;
}

void AVFrameManager::CleanupOldFrames() {
    const int64_t CLEANUP_THRESHOLD_MS = 5000; // 5秒
    int64_t current_time = Utils::GetCurrentTimeMs();
    
    auto it = std::remove_if(frame_pool_.begin(), frame_pool_.end(),
        [current_time, CLEANUP_THRESHOLD_MS](const std::unique_ptr<FrameInfo>& info) {
            if (!info->in_use && 
                (current_time - info->last_used_time) > CLEANUP_THRESHOLD_MS) {
                av_frame_free(&info->frame);
                return true;
            }
            return false;
        });
    
    size_t removed = std::distance(it, frame_pool_.end());
    frame_pool_.erase(it, frame_pool_.end());
    
    if (removed > 0) {
        LOG_INFO("Cleaned up %zu old AVFrames", removed);
    }
}

// AVFrameWrapper实现
AVFrameWrapper::AVFrameWrapper(AVFrameManager* manager, AVFrame* frame)
    : manager_(manager), frame_(frame) {
}

AVFrameWrapper::~AVFrameWrapper() {
    if (manager_ && frame_) {
        manager_->ReleaseFrame(frame_);
    }
}

AVFrameWrapper::AVFrameWrapper(AVFrameWrapper&& other) noexcept
    : manager_(other.manager_), frame_(other.frame_) {
    other.manager_ = nullptr;
    other.frame_ = nullptr;
}

AVFrameWrapper& AVFrameWrapper::operator=(AVFrameWrapper&& other) noexcept {
    if (this != &other) {
        if (manager_ && frame_) {
            manager_->ReleaseFrame(frame_);
        }
        
        manager_ = other.manager_;
        frame_ = other.frame_;
        
        other.manager_ = nullptr;
        other.frame_ = nullptr;
    }
    return *this;
}

AVFrame* AVFrameWrapper::release() {
    AVFrame* temp = frame_;
    frame_ = nullptr;
    manager_ = nullptr;
    return temp;
}

// YUVConverter实现
YUVConverter::YUVConverter()
    : sws_context_(nullptr), src_width_(0), src_height_(0),
      dst_width_(0), dst_height_(0),
      src_format_(AV_PIX_FMT_NONE), dst_format_(AV_PIX_FMT_NONE),
      initialized_(false) {
}

YUVConverter::~YUVConverter() {
    Reset();
}

bool YUVConverter::Init(int src_width, int src_height, AVPixelFormat src_format,
                       int dst_width, int dst_height, AVPixelFormat dst_format) {
    std::lock_guard<std::mutex> lock(convert_mutex_);

    // 如果参数相同，不需要重新初始化
    if (initialized_ &&
        src_width_ == src_width && src_height_ == src_height && src_format_ == src_format &&
        dst_width_ == dst_width && dst_height_ == dst_height && dst_format_ == dst_format) {
        return true;
    }

    // 清理旧的上下文
    if (sws_context_) {
        sws_freeContext(sws_context_);
        sws_context_ = nullptr;
    }

    // 创建新的转换上下文
    sws_context_ = sws_getContext(
        src_width, src_height, src_format,
        dst_width, dst_height, dst_format,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );

    if (!sws_context_) {
        LOG_ERROR("Failed to create sws context: %dx%d(%d) -> %dx%d(%d)",
                 src_width, src_height, src_format,
                 dst_width, dst_height, dst_format);
        return false;
    }

    src_width_ = src_width;
    src_height_ = src_height;
    src_format_ = src_format;
    dst_width_ = dst_width;
    dst_height_ = dst_height;
    dst_format_ = dst_format;
    initialized_ = true;

    LOG_INFO("YUVConverter initialized: %dx%d(%d) -> %dx%d(%d)",
             src_width, src_height, src_format,
             dst_width, dst_height, dst_format);

    return true;
}

bool YUVConverter::Convert(const AVFrame* src_frame, AVFrame* dst_frame) {
    if (!src_frame || !dst_frame) {
        LOG_ERROR("Invalid frame pointers");
        return false;
    }

    return Convert(src_frame->data, src_frame->linesize,
                  dst_frame->data, dst_frame->linesize);
}

bool YUVConverter::Convert(const uint8_t* const src_data[], const int src_linesize[],
                          uint8_t* const dst_data[], const int dst_linesize[]) {
    std::lock_guard<std::mutex> lock(convert_mutex_);

    if (!initialized_ || !sws_context_) {
        LOG_ERROR("YUVConverter not initialized");
        return false;
    }

    int ret = sws_scale(sws_context_,
                       src_data, src_linesize, 0, src_height_,
                       dst_data, dst_linesize);

    if (ret != dst_height_) {
        LOG_ERROR("sws_scale failed: expected %d, got %d", dst_height_, ret);
        return false;
    }

    return true;
}

void YUVConverter::Reset() {
    std::lock_guard<std::mutex> lock(convert_mutex_);

    if (sws_context_) {
        sws_freeContext(sws_context_);
        sws_context_ = nullptr;
    }

    initialized_ = false;
    LOG_INFO("YUVConverter reset");
}

YUVConverter::ConvertInfo YUVConverter::GetInfo() const {
    std::lock_guard<std::mutex> lock(convert_mutex_);

    ConvertInfo info;
    info.src_width = src_width_;
    info.src_height = src_height_;
    info.dst_width = dst_width_;
    info.dst_height = dst_height_;
    info.src_format = src_format_;
    info.dst_format = dst_format_;
    info.initialized = initialized_;

    return info;
}

// FPSController实现
FPSController::FPSController(double target_fps)
    : target_fps_(target_fps), last_frame_time_(0), frame_count_(0),
      dropped_frames_(0), start_time_(0) {

    target_frame_time_us_ = static_cast<int64_t>(1000000.0 / target_fps);
    start_time_ = GetCurrentTimeUs();
    last_frame_time_ = start_time_;

    LOG_INFO("FPSController created with target FPS: %.2f", target_fps);
}

void FPSController::SetTargetFPS(double fps) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    target_fps_ = fps;
    target_frame_time_us_ = static_cast<int64_t>(1000000.0 / fps);
    LOG_INFO("Target FPS changed to: %.2f", fps);
}

int64_t FPSController::WaitForNextFrame() {
    int64_t current_time = GetCurrentTimeUs();
    int64_t elapsed = current_time - last_frame_time_;
    int64_t wait_time = target_frame_time_us_ - elapsed;

    if (wait_time > 0) {
        // 需要等待
        std::this_thread::sleep_for(std::chrono::microseconds(wait_time));
        current_time = GetCurrentTimeUs();
    } else if (wait_time < -target_frame_time_us_) {
        // 严重延迟，记录丢帧
        std::lock_guard<std::mutex> lock(stats_mutex_);
        dropped_frames_++;
    }

    last_frame_time_ = current_time;
    UpdateStats();

    return wait_time > 0 ? wait_time / 1000 : 0; // 返回毫秒
}

double FPSController::GetCurrentFPS() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    if (frame_count_ == 0) return 0.0;

    int64_t elapsed = GetCurrentTimeUs() - start_time_;
    if (elapsed <= 0) return 0.0;

    return (frame_count_ * 1000000.0) / elapsed;
}

void FPSController::Reset() {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    start_time_ = GetCurrentTimeUs();
    last_frame_time_ = start_time_;
    frame_count_ = 0;
    dropped_frames_ = 0;

    // 清空帧时间历史
    while (!frame_times_.empty()) {
        frame_times_.pop();
    }

    LOG_INFO("FPSController reset");
}

FPSController::FPSStats FPSController::GetStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    FPSStats stats;
    stats.current_fps = GetCurrentFPS();
    stats.target_fps = target_fps_;
    stats.total_frames = frame_count_;
    stats.dropped_frames = dropped_frames_;

    // 计算平均帧时间和方差
    if (!frame_times_.empty()) {
        int64_t sum = 0;
        std::queue<int64_t> temp_queue = frame_times_;

        while (!temp_queue.empty()) {
            sum += temp_queue.front();
            temp_queue.pop();
        }

        stats.avg_frame_time = static_cast<double>(sum) / frame_times_.size() / 1000.0; // 毫秒

        // 计算方差
        double variance_sum = 0.0;
        temp_queue = frame_times_;
        while (!temp_queue.empty()) {
            double diff = (temp_queue.front() / 1000.0) - stats.avg_frame_time;
            variance_sum += diff * diff;
            temp_queue.pop();
        }
        stats.frame_time_variance = variance_sum / frame_times_.size();
    } else {
        stats.avg_frame_time = 0.0;
        stats.frame_time_variance = 0.0;
    }

    return stats;
}

void FPSController::UpdateStats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    frame_count_++;

    // 记录帧时间
    int64_t current_time = GetCurrentTimeUs();
    if (frame_count_ > 1) {
        int64_t frame_time = current_time - last_frame_time_;
        frame_times_.push(frame_time);

        // 限制历史记录大小
        if (frame_times_.size() > MAX_FRAME_HISTORY) {
            frame_times_.pop();
        }
    }
}

int64_t FPSController::GetCurrentTimeUs() const {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
}

// PTSCalculator实现
PTSCalculator::PTSCalculator(AVRational time_base)
    : time_base_(time_base), current_pts_(0), start_pts_(0) {
    LOG_INFO("PTSCalculator created with time_base: %d/%d",
             time_base.num, time_base.den);
}

void PTSCalculator::SetTimeBase(AVRational time_base) {
    std::lock_guard<std::mutex> lock(pts_mutex_);
    time_base_ = time_base;
    LOG_INFO("Time base changed to: %d/%d", time_base.num, time_base.den);
}

int64_t PTSCalculator::GetNextPTS() {
    std::lock_guard<std::mutex> lock(pts_mutex_);
    return current_pts_++;
}

void PTSCalculator::Reset() {
    std::lock_guard<std::mutex> lock(pts_mutex_);
    current_pts_ = start_pts_;
    LOG_INFO("PTS calculator reset to: %ld", start_pts_);
}

void PTSCalculator::SetStartPTS(int64_t start_pts) {
    std::lock_guard<std::mutex> lock(pts_mutex_);
    start_pts_ = start_pts;
    current_pts_ = start_pts;
    LOG_INFO("Start PTS set to: %ld", start_pts);
}

double PTSCalculator::PTSToSeconds(int64_t pts) const {
    return av_q2d(time_base_) * pts;
}

int64_t PTSCalculator::SecondsToPTS(double seconds) const {
    return static_cast<int64_t>(seconds / av_q2d(time_base_));
}
