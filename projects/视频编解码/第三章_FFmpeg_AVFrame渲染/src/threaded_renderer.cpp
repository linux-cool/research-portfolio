#include "threaded_renderer.h"

ThreadedRenderer::ThreadedRenderer(const Config& config)
    : config_(config), frame_queue_(config.frame_queue_size),
      running_(false), paused_(false), should_stop_(false),
      total_frames_(0), dropped_frames_(0), rendered_frames_(0) {
    
    fps_controller_ = std::make_unique<FPSController>(config.target_fps);
    LOG_INFO("ThreadedRenderer created with queue size: %zu, target FPS: %.2f", 
             config.frame_queue_size, config.target_fps);
}

ThreadedRenderer::~ThreadedRenderer() {
    Stop();
    LOG_INFO("ThreadedRenderer destroyed");
}

bool ThreadedRenderer::Init(std::unique_ptr<XVideoView> renderer, 
                           std::shared_ptr<AVFrameManager> frame_manager) {
    if (!renderer || !frame_manager) {
        LOG_ERROR("Invalid renderer or frame manager");
        return false;
    }
    
    renderer_ = std::move(renderer);
    frame_manager_ = frame_manager;
    
    LOG_INFO("ThreadedRenderer initialized with %s renderer", 
             renderer_->GetType().c_str());
    return true;
}

bool ThreadedRenderer::Start() {
    if (running_) {
        LOG_WARN("ThreadedRenderer already running");
        return true;
    }
    
    if (!renderer_ || !frame_manager_) {
        LOG_ERROR("ThreadedRenderer not properly initialized");
        return false;
    }
    
    should_stop_ = false;
    running_ = true;
    paused_ = false;
    
    // 重启队列
    frame_queue_.Restart();
    
    // 重置统计信息
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        total_frames_ = 0;
        dropped_frames_ = 0;
        rendered_frames_ = 0;
    }
    
    fps_controller_->Reset();
    
    // 启动渲染线程
    render_thread_ = std::make_unique<std::thread>(&ThreadedRenderer::RenderThreadFunc, this);
    
    LOG_INFO("ThreadedRenderer started");
    return true;
}

void ThreadedRenderer::Stop() {
    if (!running_) {
        return;
    }
    
    LOG_INFO("Stopping ThreadedRenderer...");
    
    should_stop_ = true;
    running_ = false;
    
    // 停止队列，唤醒渲染线程
    frame_queue_.Stop();
    
    // 等待渲染线程结束
    if (render_thread_ && render_thread_->joinable()) {
        render_thread_->join();
    }
    
    render_thread_.reset();
    
    LOG_INFO("ThreadedRenderer stopped");
}

void ThreadedRenderer::Pause() {
    paused_ = true;
    LOG_INFO("ThreadedRenderer paused");
}

void ThreadedRenderer::Resume() {
    paused_ = false;
    LOG_INFO("ThreadedRenderer resumed");
}

bool ThreadedRenderer::SubmitFrame(AVFrame* frame, int timeout_ms) {
    if (!running_ || !frame) {
        return false;
    }
    
    // 检查是否需要丢帧
    if (config_.enable_frame_drop && ShouldDropFrame()) {
        UpdateStats(false, true);
        return true; // 丢帧也算成功
    }
    
    bool success = frame_queue_.Push(frame, timeout_ms);
    if (success) {
        UpdateStats(false, false);
    }
    
    return success;
}

void ThreadedRenderer::SetTargetFPS(double fps) {
    config_.target_fps = fps;
    if (fps_controller_) {
        fps_controller_->SetTargetFPS(fps);
    }
    if (renderer_) {
        renderer_->SetTargetFPS(fps);
    }
    LOG_INFO("Target FPS changed to: %.2f", fps);
}

ThreadedRenderer::RenderStats ThreadedRenderer::GetStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    RenderStats stats;
    stats.current_fps = fps_controller_ ? fps_controller_->GetCurrentFPS() : 0.0;
    stats.target_fps = config_.target_fps;
    stats.queue_size = frame_queue_.Size();
    stats.total_frames = total_frames_;
    stats.dropped_frames = dropped_frames_;
    stats.rendered_frames = rendered_frames_;
    stats.is_running = running_;
    stats.is_paused = paused_;
    
    return stats;
}

void ThreadedRenderer::ClearQueue() {
    frame_queue_.Clear();
    LOG_INFO("Frame queue cleared");
}

void ThreadedRenderer::RenderThreadFunc() {
    LOG_INFO("Render thread started");
    
    AVFrame* frame = nullptr;
    
    while (!should_stop_) {
        // 检查暂停状态
        if (paused_) {
            Utils::SleepMs(10);
            continue;
        }
        
        // 从队列获取帧
        if (!frame_queue_.Pop(frame, 100)) { // 100ms超时
            continue;
        }
        
        if (!frame) {
            continue;
        }
        
        // 处理帧
        bool success = ProcessFrame(frame);
        UpdateStats(success, false);
        
        // 帧率控制
        if (config_.enable_fps_control && fps_controller_) {
            fps_controller_->WaitForNextFrame();
        }
    }
    
    LOG_INFO("Render thread stopped");
}

bool ThreadedRenderer::ProcessFrame(AVFrame* frame) {
    if (!renderer_ || !frame) {
        return false;
    }
    
    // 渲染帧
    bool success = renderer_->Render(frame);
    
    if (!success) {
        LOG_WARN("Failed to render frame");
    }
    
    return success;
}

bool ThreadedRenderer::ShouldDropFrame() const {
    if (!config_.enable_frame_drop) {
        return false;
    }
    
    // 如果队列接近满，丢帧
    size_t queue_size = frame_queue_.Size();
    size_t threshold = config_.frame_queue_size * 0.8; // 80%阈值
    
    return queue_size >= threshold;
}

void ThreadedRenderer::UpdateStats(bool frame_rendered, bool frame_dropped) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    total_frames_++;
    
    if (frame_rendered) {
        rendered_frames_++;
    }
    
    if (frame_dropped) {
        dropped_frames_++;
    }
}

// AVFrameRendererFactory实现
std::unique_ptr<ThreadedRenderer> AVFrameRendererFactory::CreateThreadedRenderer(
    const ThreadedRenderer::Config& config,
    XVideoViewFactory::RendererType renderer_type) {
    
    // 创建渲染器
    auto renderer = XVideoViewFactory::Create(renderer_type);
    if (!renderer) {
        LOG_ERROR("Failed to create video renderer");
        return nullptr;
    }
    
    // 创建帧管理器
    auto frame_manager = CreateFrameManager();
    if (!frame_manager) {
        LOG_ERROR("Failed to create frame manager");
        return nullptr;
    }
    
    // 创建多线程渲染器
    auto threaded_renderer = std::make_unique<ThreadedRenderer>(config);
    if (!threaded_renderer->Init(std::move(renderer), frame_manager)) {
        LOG_ERROR("Failed to initialize threaded renderer");
        return nullptr;
    }
    
    LOG_INFO("Created threaded renderer successfully");
    return threaded_renderer;
}

std::shared_ptr<AVFrameManager> AVFrameRendererFactory::CreateFrameManager(size_t pool_size) {
    return std::make_shared<AVFrameManager>(pool_size);
}
