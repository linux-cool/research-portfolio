#pragma once

#include "common.h"
#include <queue>
#include <memory>

/**
 * @brief AVFrame内存管理器
 * 
 * 负责AVFrame的分配、释放和内存池管理
 */
class AVFrameManager {
public:
    AVFrameManager(size_t pool_size = 10);
    ~AVFrameManager();
    
    /**
     * @brief 分配一个AVFrame
     * @param width 宽度
     * @param height 高度
     * @param format 像素格式
     * @return AVFrame指针，失败返回nullptr
     */
    AVFrame* AllocFrame(int width, int height, AVPixelFormat format);
    
    /**
     * @brief 释放AVFrame到池中
     * @param frame 要释放的frame
     */
    void ReleaseFrame(AVFrame* frame);
    
    /**
     * @brief 克隆AVFrame
     * @param src 源frame
     * @return 克隆的frame，失败返回nullptr
     */
    AVFrame* CloneFrame(const AVFrame* src);
    
    /**
     * @brief 获取池统计信息
     */
    struct PoolStats {
        size_t total_frames;
        size_t available_frames;
        size_t allocated_frames;
        size_t peak_usage;
    };
    
    PoolStats GetStats() const;
    
    /**
     * @brief 清空内存池
     */
    void Clear();

private:
    struct FrameInfo {
        AVFrame* frame;
        int width;
        int height;
        AVPixelFormat format;
        bool in_use;
        int64_t last_used_time;
    };
    
    std::vector<std::unique_ptr<FrameInfo>> frame_pool_;
    mutable std::mutex pool_mutex_;
    size_t max_pool_size_;
    size_t peak_usage_;
    
    AVFrame* CreateNewFrame(int width, int height, AVPixelFormat format);
    FrameInfo* FindAvailableFrame(int width, int height, AVPixelFormat format);
    void CleanupOldFrames();
};

/**
 * @brief AVFrame RAII包装器
 */
class AVFrameWrapper {
public:
    AVFrameWrapper(AVFrameManager* manager, AVFrame* frame);
    ~AVFrameWrapper();
    
    // 禁止拷贝
    AVFrameWrapper(const AVFrameWrapper&) = delete;
    AVFrameWrapper& operator=(const AVFrameWrapper&) = delete;
    
    // 支持移动
    AVFrameWrapper(AVFrameWrapper&& other) noexcept;
    AVFrameWrapper& operator=(AVFrameWrapper&& other) noexcept;
    
    AVFrame* get() const { return frame_; }
    AVFrame* operator->() const { return frame_; }
    AVFrame& operator*() const { return *frame_; }
    
    explicit operator bool() const { return frame_ != nullptr; }
    
    /**
     * @brief 释放frame的所有权
     * @return frame指针
     */
    AVFrame* release();

private:
    AVFrameManager* manager_;
    AVFrame* frame_;
};

/**
 * @brief YUV数据转换器
 */
class YUVConverter {
public:
    YUVConverter();
    ~YUVConverter();
    
    /**
     * @brief 初始化转换器
     * @param src_width 源宽度
     * @param src_height 源高度
     * @param src_format 源格式
     * @param dst_width 目标宽度
     * @param dst_height 目标高度
     * @param dst_format 目标格式
     * @return 成功返回true
     */
    bool Init(int src_width, int src_height, AVPixelFormat src_format,
              int dst_width, int dst_height, AVPixelFormat dst_format);
    
    /**
     * @brief 转换AVFrame
     * @param src_frame 源frame
     * @param dst_frame 目标frame
     * @return 成功返回true
     */
    bool Convert(const AVFrame* src_frame, AVFrame* dst_frame);
    
    /**
     * @brief 转换原始数据
     * @param src_data 源数据
     * @param src_linesize 源行步长
     * @param dst_data 目标数据
     * @param dst_linesize 目标行步长
     * @return 成功返回true
     */
    bool Convert(const uint8_t* const src_data[], const int src_linesize[],
                uint8_t* const dst_data[], const int dst_linesize[]);
    
    /**
     * @brief 重置转换器
     */
    void Reset();
    
    /**
     * @brief 获取转换器信息
     */
    struct ConvertInfo {
        int src_width, src_height;
        int dst_width, dst_height;
        AVPixelFormat src_format, dst_format;
        bool initialized;
    };
    
    ConvertInfo GetInfo() const;

private:
    SwsContext* sws_context_;
    int src_width_, src_height_;
    int dst_width_, dst_height_;
    AVPixelFormat src_format_, dst_format_;
    bool initialized_;
    mutable std::mutex convert_mutex_;
};

/**
 * @brief 帧率控制器
 */
class FPSController {
public:
    explicit FPSController(double target_fps = 25.0);
    ~FPSController() = default;
    
    /**
     * @brief 设置目标帧率
     * @param fps 目标帧率
     */
    void SetTargetFPS(double fps);
    
    /**
     * @brief 获取目标帧率
     * @return 目标帧率
     */
    double GetTargetFPS() const { return target_fps_; }
    
    /**
     * @brief 等待下一帧时间
     * @return 实际等待时间(毫秒)
     */
    int64_t WaitForNextFrame();
    
    /**
     * @brief 获取当前帧率
     * @return 当前帧率
     */
    double GetCurrentFPS() const;
    
    /**
     * @brief 重置帧率统计
     */
    void Reset();
    
    /**
     * @brief 获取帧率统计信息
     */
    struct FPSStats {
        double current_fps;
        double target_fps;
        int64_t total_frames;
        int64_t dropped_frames;
        double avg_frame_time;
        double frame_time_variance;
    };
    
    FPSStats GetStats() const;

private:
    double target_fps_;
    int64_t target_frame_time_us_; // 微秒
    
    mutable std::mutex stats_mutex_;
    int64_t last_frame_time_;
    int64_t frame_count_;
    int64_t dropped_frames_;
    int64_t start_time_;
    
    // 帧时间历史记录（用于计算方差）
    std::queue<int64_t> frame_times_;
    static const size_t MAX_FRAME_HISTORY = 100;
    
    void UpdateStats();
    int64_t GetCurrentTimeUs() const;
};

/**
 * @brief 时间戳计算器
 */
class PTSCalculator {
public:
    explicit PTSCalculator(AVRational time_base = {1, 1000000});
    ~PTSCalculator() = default;
    
    /**
     * @brief 设置时间基
     * @param time_base 时间基
     */
    void SetTimeBase(AVRational time_base);
    
    /**
     * @brief 获取下一个PTS
     * @return PTS值
     */
    int64_t GetNextPTS();
    
    /**
     * @brief 重置PTS计算器
     */
    void Reset();
    
    /**
     * @brief 设置起始PTS
     * @param start_pts 起始PTS
     */
    void SetStartPTS(int64_t start_pts);
    
    /**
     * @brief PTS转换为秒
     * @param pts PTS值
     * @return 秒数
     */
    double PTSToSeconds(int64_t pts) const;
    
    /**
     * @brief 秒转换为PTS
     * @param seconds 秒数
     * @return PTS值
     */
    int64_t SecondsToPTS(double seconds) const;

private:
    AVRational time_base_;
    int64_t current_pts_;
    int64_t start_pts_;
    std::mutex pts_mutex_;
};
