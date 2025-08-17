#pragma once

#include "common.h"
#include <memory>

/**
 * @brief 高级sws_scale封装器
 * 
 * 提供更易用的像素格式转换接口，支持多种转换模式和优化选项
 */
class SwsConverter {
public:
    /**
     * @brief 转换质量枚举
     */
    enum class Quality {
        FAST_BILINEAR = SWS_FAST_BILINEAR,
        BILINEAR = SWS_BILINEAR,
        BICUBIC = SWS_BICUBIC,
        X = SWS_X,
        POINT = SWS_POINT,
        AREA = SWS_AREA,
        BICUBLIN = SWS_BICUBLIN,
        GAUSS = SWS_GAUSS,
        SINC = SWS_SINC,
        LANCZOS = SWS_LANCZOS,
        SPLINE = SWS_SPLINE
    };
    
    /**
     * @brief 转换配置
     */
    struct Config {
        int src_width, src_height;
        int dst_width, dst_height;
        AVPixelFormat src_format, dst_format;
        Quality quality;
        bool enable_cpu_flags;
        
        Config() : src_width(0), src_height(0), dst_width(0), dst_height(0),
                  src_format(AV_PIX_FMT_NONE), dst_format(AV_PIX_FMT_NONE),
                  quality(Quality::BILINEAR), enable_cpu_flags(true) {}
    };
    
    SwsConverter();
    ~SwsConverter();
    
    /**
     * @brief 初始化转换器
     * @param config 转换配置
     * @return 成功返回true
     */
    bool Init(const Config& config);
    
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
     * @brief 转换并缩放
     * @param src_frame 源frame
     * @param dst_frame 目标frame
     * @param dst_width 目标宽度
     * @param dst_height 目标高度
     * @return 成功返回true
     */
    bool ConvertAndScale(const AVFrame* src_frame, AVFrame* dst_frame,
                        int dst_width, int dst_height);
    
    /**
     * @brief 重置转换器
     */
    void Reset();
    
    /**
     * @brief 获取转换器信息
     */
    Config GetConfig() const;
    
    /**
     * @brief 检查是否已初始化
     */
    bool IsInitialized() const { return initialized_; }
    
    /**
     * @brief 获取支持的像素格式列表
     */
    static std::vector<AVPixelFormat> GetSupportedFormats();
    
    /**
     * @brief 检查格式转换是否支持
     */
    static bool IsConversionSupported(AVPixelFormat src_fmt, AVPixelFormat dst_fmt);

private:
    SwsContext* sws_context_;
    Config config_;
    bool initialized_;
    mutable std::mutex convert_mutex_;
    
    bool CreateContext();
    void DestroyContext();
};

/**
 * @brief 批量格式转换器
 */
class BatchConverter {
public:
    struct ConvertTask {
        const AVFrame* src_frame;
        AVFrame* dst_frame;
        SwsConverter::Config config;
        std::function<void(bool success)> callback;
    };
    
    explicit BatchConverter(size_t max_threads = 4);
    ~BatchConverter();
    
    /**
     * @brief 添加转换任务
     * @param task 转换任务
     * @return 任务ID
     */
    uint64_t AddTask(const ConvertTask& task);
    
    /**
     * @brief 等待所有任务完成
     */
    void WaitAll();
    
    /**
     * @brief 取消所有任务
     */
    void CancelAll();
    
    /**
     * @brief 获取统计信息
     */
    struct Stats {
        uint64_t total_tasks;
        uint64_t completed_tasks;
        uint64_t failed_tasks;
        double avg_convert_time_ms;
    };
    
    Stats GetStats() const;

private:
    struct TaskInfo {
        uint64_t id;
        ConvertTask task;
        std::chrono::steady_clock::time_point start_time;
    };
    
    std::vector<std::unique_ptr<std::thread>> worker_threads_;
    std::queue<TaskInfo> task_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::atomic<bool> should_stop_;
    std::atomic<uint64_t> next_task_id_;
    
    // 统计信息
    mutable std::mutex stats_mutex_;
    uint64_t total_tasks_;
    uint64_t completed_tasks_;
    uint64_t failed_tasks_;
    double total_convert_time_ms_;
    
    void WorkerThread();
};

/**
 * @brief 常用格式转换工具类
 */
class FormatConverter {
public:
    /**
     * @brief YUV420P转RGB24
     */
    static bool YUV420P_to_RGB24(const AVFrame* yuv_frame, AVFrame* rgb_frame);
    
    /**
     * @brief RGB24转YUV420P
     */
    static bool RGB24_to_YUV420P(const AVFrame* rgb_frame, AVFrame* yuv_frame);
    
    /**
     * @brief YUV420P转RGBA
     */
    static bool YUV420P_to_RGBA(const AVFrame* yuv_frame, AVFrame* rgba_frame);
    
    /**
     * @brief RGBA转YUV420P
     */
    static bool RGBA_to_YUV420P(const AVFrame* rgba_frame, AVFrame* yuv_frame);
    
    /**
     * @brief NV12转RGB24
     */
    static bool NV12_to_RGB24(const AVFrame* nv12_frame, AVFrame* rgb_frame);
    
    /**
     * @brief 保存RGB帧为图片文件
     * @param rgb_frame RGB帧
     * @param filename 文件名
     * @param format 图片格式 ("png", "jpg", "bmp")
     * @return 成功返回true
     */
    static bool SaveRGBFrame(const AVFrame* rgb_frame, const std::string& filename, 
                           const std::string& format = "png");
    
    /**
     * @brief 从图片文件加载RGB帧
     * @param filename 文件名
     * @param rgb_frame 输出RGB帧
     * @return 成功返回true
     */
    static bool LoadRGBFrame(const std::string& filename, AVFrame* rgb_frame);
    
    /**
     * @brief 计算格式转换的内存需求
     * @param width 宽度
     * @param height 高度
     * @param format 像素格式
     * @return 所需字节数
     */
    static size_t CalculateFrameSize(int width, int height, AVPixelFormat format);

private:
    static std::unique_ptr<SwsConverter> CreateConverter(
        int width, int height, AVPixelFormat src_fmt, AVPixelFormat dst_fmt);
};
