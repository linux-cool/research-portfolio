#pragma once

#include "common.h"
#include "avframe_manager.h"
#include "xvideo_view.h"
#include <thread>
#include <atomic>
#include <condition_variable>

/**
 * @brief 线程安全的AVFrame队列
 */
template<typename T>
class ThreadSafeQueue {
public:
    explicit ThreadSafeQueue(size_t max_size = 100) : max_size_(max_size), stopped_(false) {}
    
    ~ThreadSafeQueue() {
        Stop();
    }
    
    /**
     * @brief 添加元素到队列
     * @param item 要添加的元素
     * @param timeout_ms 超时时间(毫秒)，-1表示无限等待
     * @return 成功返回true，超时或队列已停止返回false
     */
    bool Push(T item, int timeout_ms = -1) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        if (timeout_ms >= 0) {
            if (!not_full_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                                   [this] { return queue_.size() < max_size_ || stopped_; })) {
                return false; // 超时
            }
        } else {
            not_full_.wait(lock, [this] { return queue_.size() < max_size_ || stopped_; });
        }
        
        if (stopped_) return false;
        
        queue_.push(std::move(item));
        not_empty_.notify_one();
        return true;
    }
    
    /**
     * @brief 从队列取出元素
     * @param item 输出参数，取出的元素
     * @param timeout_ms 超时时间(毫秒)，-1表示无限等待
     * @return 成功返回true，超时或队列已停止返回false
     */
    bool Pop(T& item, int timeout_ms = -1) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        if (timeout_ms >= 0) {
            if (!not_empty_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                                    [this] { return !queue_.empty() || stopped_; })) {
                return false; // 超时
            }
        } else {
            not_empty_.wait(lock, [this] { return !queue_.empty() || stopped_; });
        }
        
        if (stopped_ && queue_.empty()) return false;
        
        if (!queue_.empty()) {
            item = std::move(queue_.front());
            queue_.pop();
            not_full_.notify_one();
            return true;
        }
        
        return false;
    }
    
    /**
     * @brief 获取队列大小
     */
    size_t Size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }
    
    /**
     * @brief 检查队列是否为空
     */
    bool Empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }
    
    /**
     * @brief 检查队列是否已满
     */
    bool Full() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size() >= max_size_;
    }
    
    /**
     * @brief 清空队列
     */
    void Clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        while (!queue_.empty()) {
            queue_.pop();
        }
        not_full_.notify_all();
    }
    
    /**
     * @brief 停止队列
     */
    void Stop() {
        std::lock_guard<std::mutex> lock(mutex_);
        stopped_ = true;
        not_empty_.notify_all();
        not_full_.notify_all();
    }
    
    /**
     * @brief 重启队列
     */
    void Restart() {
        std::lock_guard<std::mutex> lock(mutex_);
        stopped_ = false;
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
    std::queue<T> queue_;
    size_t max_size_;
    std::atomic<bool> stopped_;
};

/**
 * @brief 多线程AVFrame渲染器
 */
class ThreadedRenderer {
public:
    struct Config {
        size_t frame_queue_size;      // 帧队列大小
        double target_fps;            // 目标帧率
        bool enable_fps_control;      // 是否启用帧率控制
        bool enable_frame_drop;       // 是否启用丢帧
        int render_thread_priority;   // 渲染线程优先级

        Config() : frame_queue_size(10), target_fps(25.0),
                  enable_fps_control(true), enable_frame_drop(true),
                  render_thread_priority(0) {}
    };
    
    explicit ThreadedRenderer(const Config& config = Config());
    ~ThreadedRenderer();
    
    /**
     * @brief 初始化渲染器
     * @param renderer 视频渲染器
     * @param frame_manager AVFrame管理器
     * @return 成功返回true
     */
    bool Init(std::unique_ptr<XVideoView> renderer, 
              std::shared_ptr<AVFrameManager> frame_manager);
    
    /**
     * @brief 启动渲染线程
     * @return 成功返回true
     */
    bool Start();
    
    /**
     * @brief 停止渲染线程
     */
    void Stop();
    
    /**
     * @brief 暂停渲染
     */
    void Pause();
    
    /**
     * @brief 恢复渲染
     */
    void Resume();
    
    /**
     * @brief 提交AVFrame进行渲染
     * @param frame 要渲染的frame
     * @param timeout_ms 超时时间
     * @return 成功返回true
     */
    bool SubmitFrame(AVFrame* frame, int timeout_ms = 100);
    
    /**
     * @brief 设置目标帧率
     * @param fps 目标帧率
     */
    void SetTargetFPS(double fps);
    
    /**
     * @brief 获取渲染统计信息
     */
    struct RenderStats {
        double current_fps;
        double target_fps;
        size_t queue_size;
        size_t total_frames;
        size_t dropped_frames;
        size_t rendered_frames;
        bool is_running;
        bool is_paused;
    };
    
    RenderStats GetStats() const;
    
    /**
     * @brief 清空帧队列
     */
    void ClearQueue();

private:
    Config config_;
    std::unique_ptr<XVideoView> renderer_;
    std::shared_ptr<AVFrameManager> frame_manager_;
    std::unique_ptr<FPSController> fps_controller_;
    
    // 线程相关
    std::unique_ptr<std::thread> render_thread_;
    std::atomic<bool> running_;
    std::atomic<bool> paused_;
    std::atomic<bool> should_stop_;
    
    // 帧队列
    ThreadSafeQueue<AVFrame*> frame_queue_;
    
    // 统计信息
    mutable std::mutex stats_mutex_;
    size_t total_frames_;
    size_t dropped_frames_;
    size_t rendered_frames_;
    
    /**
     * @brief 渲染线程主函数
     */
    void RenderThreadFunc();
    
    /**
     * @brief 处理单个帧
     * @param frame 要处理的frame
     * @return 成功返回true
     */
    bool ProcessFrame(AVFrame* frame);
    
    /**
     * @brief 检查是否需要丢帧
     * @return 需要丢帧返回true
     */
    bool ShouldDropFrame() const;
    
    /**
     * @brief 更新统计信息
     */
    void UpdateStats(bool frame_rendered, bool frame_dropped);
};

/**
 * @brief AVFrame渲染器工厂
 */
class AVFrameRendererFactory {
public:
    /**
     * @brief 创建多线程渲染器
     * @param config 配置参数
     * @param renderer_type 渲染器类型
     * @return 渲染器实例
     */
    static std::unique_ptr<ThreadedRenderer> CreateThreadedRenderer(
        const ThreadedRenderer::Config& config = ThreadedRenderer::Config(),
        XVideoViewFactory::RendererType renderer_type = XVideoViewFactory::RendererType::AUTO);
    
    /**
     * @brief 创建AVFrame管理器
     * @param pool_size 池大小
     * @return 管理器实例
     */
    static std::shared_ptr<AVFrameManager> CreateFrameManager(size_t pool_size = 20);
};
