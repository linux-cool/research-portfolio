#include "sws_converter.h"
#include <algorithm>

SwsConverter::SwsConverter() 
    : sws_context_(nullptr), initialized_(false) {
}

SwsConverter::~SwsConverter() {
    Reset();
}

bool SwsConverter::Init(const Config& config) {
    std::lock_guard<std::mutex> lock(convert_mutex_);
    
    if (config.src_width <= 0 || config.src_height <= 0 ||
        config.dst_width <= 0 || config.dst_height <= 0) {
        LOG_ERROR("Invalid dimensions: src=%dx%d, dst=%dx%d",
                 config.src_width, config.src_height,
                 config.dst_width, config.dst_height);
        return false;
    }
    
    if (config.src_format == AV_PIX_FMT_NONE || config.dst_format == AV_PIX_FMT_NONE) {
        LOG_ERROR("Invalid pixel formats: src=%d, dst=%d", 
                 config.src_format, config.dst_format);
        return false;
    }
    
    // 如果配置相同，不需要重新初始化
    if (initialized_ && 
        config.src_width == config_.src_width &&
        config.src_height == config_.src_height &&
        config.dst_width == config_.dst_width &&
        config.dst_height == config_.dst_height &&
        config.src_format == config_.src_format &&
        config.dst_format == config_.dst_format &&
        config.quality == config_.quality) {
        return true;
    }
    
    // 清理旧的上下文
    DestroyContext();
    
    config_ = config;
    
    if (!CreateContext()) {
        return false;
    }
    
    initialized_ = true;
    LOG_INFO("SwsConverter initialized: %dx%d(%d) -> %dx%d(%d), quality=%d",
             config_.src_width, config_.src_height, config_.src_format,
             config_.dst_width, config_.dst_height, config_.dst_format,
             static_cast<int>(config_.quality));
    
    return true;
}

bool SwsConverter::Convert(const AVFrame* src_frame, AVFrame* dst_frame) {
    if (!src_frame || !dst_frame) {
        LOG_ERROR("Invalid frame pointers");
        return false;
    }
    
    return Convert(src_frame->data, src_frame->linesize,
                  dst_frame->data, dst_frame->linesize);
}

bool SwsConverter::Convert(const uint8_t* const src_data[], const int src_linesize[],
                          uint8_t* const dst_data[], const int dst_linesize[]) {
    std::lock_guard<std::mutex> lock(convert_mutex_);
    
    if (!initialized_ || !sws_context_) {
        LOG_ERROR("SwsConverter not initialized");
        return false;
    }
    
    if (!src_data || !dst_data || !src_linesize || !dst_linesize) {
        LOG_ERROR("Invalid data pointers");
        return false;
    }
    
    int ret = sws_scale(sws_context_,
                       src_data, src_linesize, 0, config_.src_height,
                       dst_data, dst_linesize);
    
    if (ret != config_.dst_height) {
        LOG_ERROR("sws_scale failed: expected %d, got %d", config_.dst_height, ret);
        return false;
    }
    
    return true;
}

bool SwsConverter::ConvertAndScale(const AVFrame* src_frame, AVFrame* dst_frame,
                                  int dst_width, int dst_height) {
    if (!src_frame || !dst_frame) {
        return false;
    }
    
    // 创建临时配置
    Config temp_config = config_;
    temp_config.src_width = src_frame->width;
    temp_config.src_height = src_frame->height;
    temp_config.src_format = static_cast<AVPixelFormat>(src_frame->format);
    temp_config.dst_width = dst_width;
    temp_config.dst_height = dst_height;
    temp_config.dst_format = static_cast<AVPixelFormat>(dst_frame->format);
    
    // 如果需要，重新初始化
    if (!Init(temp_config)) {
        return false;
    }
    
    return Convert(src_frame, dst_frame);
}

void SwsConverter::Reset() {
    std::lock_guard<std::mutex> lock(convert_mutex_);
    DestroyContext();
    initialized_ = false;
}

SwsConverter::Config SwsConverter::GetConfig() const {
    std::lock_guard<std::mutex> lock(convert_mutex_);
    return config_;
}

std::vector<AVPixelFormat> SwsConverter::GetSupportedFormats() {
    return {
        AV_PIX_FMT_YUV420P,
        AV_PIX_FMT_YUV422P,
        AV_PIX_FMT_YUV444P,
        AV_PIX_FMT_RGB24,
        AV_PIX_FMT_BGR24,
        AV_PIX_FMT_RGBA,
        AV_PIX_FMT_BGRA,
        AV_PIX_FMT_ARGB,
        AV_PIX_FMT_ABGR,
        AV_PIX_FMT_NV12,
        AV_PIX_FMT_NV21,
        AV_PIX_FMT_GRAY8
    };
}

bool SwsConverter::IsConversionSupported(AVPixelFormat src_fmt, AVPixelFormat dst_fmt) {
    auto supported = GetSupportedFormats();
    return std::find(supported.begin(), supported.end(), src_fmt) != supported.end() &&
           std::find(supported.begin(), supported.end(), dst_fmt) != supported.end();
}

bool SwsConverter::CreateContext() {
    int flags = static_cast<int>(config_.quality);
    
    // CPU优化标志在新版本FFmpeg中已移除，使用默认优化
    
    sws_context_ = sws_getContext(
        config_.src_width, config_.src_height, config_.src_format,
        config_.dst_width, config_.dst_height, config_.dst_format,
        flags, nullptr, nullptr, nullptr
    );
    
    if (!sws_context_) {
        LOG_ERROR("Failed to create sws context");
        return false;
    }
    
    return true;
}

void SwsConverter::DestroyContext() {
    if (sws_context_) {
        sws_freeContext(sws_context_);
        sws_context_ = nullptr;
    }
}

// BatchConverter实现
BatchConverter::BatchConverter(size_t max_threads) 
    : should_stop_(false), next_task_id_(1),
      total_tasks_(0), completed_tasks_(0), failed_tasks_(0), total_convert_time_ms_(0.0) {
    
    // 创建工作线程
    for (size_t i = 0; i < max_threads; ++i) {
        worker_threads_.emplace_back(std::make_unique<std::thread>(&BatchConverter::WorkerThread, this));
    }
    
    LOG_INFO("BatchConverter created with %zu threads", max_threads);
}

BatchConverter::~BatchConverter() {
    CancelAll();
    
    // 等待所有线程结束
    for (auto& thread : worker_threads_) {
        if (thread && thread->joinable()) {
            thread->join();
        }
    }
    
    LOG_INFO("BatchConverter destroyed");
}

uint64_t BatchConverter::AddTask(const ConvertTask& task) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    
    if (should_stop_) {
        return 0;
    }
    
    TaskInfo task_info;
    task_info.id = next_task_id_++;
    task_info.task = task;
    task_info.start_time = std::chrono::steady_clock::now();
    
    task_queue_.push(task_info);
    
    {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        total_tasks_++;
    }
    
    queue_cv_.notify_one();
    return task_info.id;
}

void BatchConverter::WaitAll() {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    queue_cv_.wait(lock, [this] { return task_queue_.empty() && !should_stop_; });
}

void BatchConverter::CancelAll() {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        should_stop_ = true;
        
        // 清空队列
        while (!task_queue_.empty()) {
            task_queue_.pop();
        }
    }
    
    queue_cv_.notify_all();
}

BatchConverter::Stats BatchConverter::GetStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    Stats stats;
    stats.total_tasks = total_tasks_;
    stats.completed_tasks = completed_tasks_;
    stats.failed_tasks = failed_tasks_;
    stats.avg_convert_time_ms = completed_tasks_ > 0 ? 
        (total_convert_time_ms_ / completed_tasks_) : 0.0;
    
    return stats;
}

void BatchConverter::WorkerThread() {
    SwsConverter converter;
    
    while (!should_stop_) {
        TaskInfo task_info;
        
        // 获取任务
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this] { return !task_queue_.empty() || should_stop_; });
            
            if (should_stop_) {
                break;
            }
            
            if (task_queue_.empty()) {
                continue;
            }
            
            task_info = task_queue_.front();
            task_queue_.pop();
        }
        
        // 执行转换
        auto start_time = std::chrono::steady_clock::now();
        bool success = false;
        
        if (converter.Init(task_info.task.config)) {
            success = converter.Convert(task_info.task.src_frame, task_info.task.dst_frame);
        }
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        double convert_time_ms = duration.count() / 1000.0;
        
        // 更新统计信息
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            if (success) {
                completed_tasks_++;
                total_convert_time_ms_ += convert_time_ms;
            } else {
                failed_tasks_++;
            }
        }
        
        // 调用回调
        if (task_info.task.callback) {
            task_info.task.callback(success);
        }
    }
}

// FormatConverter实现
bool FormatConverter::YUV420P_to_RGB24(const AVFrame* yuv_frame, AVFrame* rgb_frame) {
    auto converter = CreateConverter(yuv_frame->width, yuv_frame->height,
                                   AV_PIX_FMT_YUV420P, AV_PIX_FMT_RGB24);
    return converter && converter->Convert(yuv_frame, rgb_frame);
}

bool FormatConverter::RGB24_to_YUV420P(const AVFrame* rgb_frame, AVFrame* yuv_frame) {
    auto converter = CreateConverter(rgb_frame->width, rgb_frame->height,
                                   AV_PIX_FMT_RGB24, AV_PIX_FMT_YUV420P);
    return converter && converter->Convert(rgb_frame, yuv_frame);
}

bool FormatConverter::YUV420P_to_RGBA(const AVFrame* yuv_frame, AVFrame* rgba_frame) {
    auto converter = CreateConverter(yuv_frame->width, yuv_frame->height,
                                   AV_PIX_FMT_YUV420P, AV_PIX_FMT_RGBA);
    return converter && converter->Convert(yuv_frame, rgba_frame);
}

bool FormatConverter::RGBA_to_YUV420P(const AVFrame* rgba_frame, AVFrame* yuv_frame) {
    auto converter = CreateConverter(rgba_frame->width, rgba_frame->height,
                                   AV_PIX_FMT_RGBA, AV_PIX_FMT_YUV420P);
    return converter && converter->Convert(rgba_frame, yuv_frame);
}

bool FormatConverter::NV12_to_RGB24(const AVFrame* nv12_frame, AVFrame* rgb_frame) {
    auto converter = CreateConverter(nv12_frame->width, nv12_frame->height,
                                   AV_PIX_FMT_NV12, AV_PIX_FMT_RGB24);
    return converter && converter->Convert(nv12_frame, rgb_frame);
}

bool FormatConverter::SaveRGBFrame(const AVFrame* rgb_frame, const std::string& filename,
                                  const std::string& format) {
    // 简化实现：保存为PPM格式
    if (format != "ppm" && format != "PPM") {
        LOG_WARN("Only PPM format supported for now, saving as PPM");
    }

    FILE* file = fopen(filename.c_str(), "wb");
    if (!file) {
        LOG_ERROR("Failed to open file for writing: %s", filename.c_str());
        return false;
    }

    // 写入PPM头部
    fprintf(file, "P6\n%d %d\n255\n", rgb_frame->width, rgb_frame->height);

    // 写入像素数据
    for (int y = 0; y < rgb_frame->height; ++y) {
        fwrite(rgb_frame->data[0] + y * rgb_frame->linesize[0],
               1, rgb_frame->width * 3, file);
    }

    fclose(file);
    LOG_INFO("RGB frame saved to: %s", filename.c_str());
    return true;
}

bool FormatConverter::LoadRGBFrame(const std::string& filename, AVFrame* rgb_frame) {
    FILE* file = fopen(filename.c_str(), "rb");
    if (!file) {
        LOG_ERROR("Failed to open file for reading: %s", filename.c_str());
        return false;
    }

    // 简化实现：只支持PPM格式
    char magic[3];
    int width, height, maxval;

    if (fscanf(file, "%2s\n%d %d\n%d\n", magic, &width, &height, &maxval) != 4) {
        LOG_ERROR("Invalid PPM file format");
        fclose(file);
        return false;
    }

    if (strcmp(magic, "P6") != 0 || maxval != 255) {
        LOG_ERROR("Unsupported PPM format");
        fclose(file);
        return false;
    }

    // 检查frame尺寸
    if (rgb_frame->width != width || rgb_frame->height != height) {
        LOG_ERROR("Frame size mismatch: expected %dx%d, got %dx%d",
                 rgb_frame->width, rgb_frame->height, width, height);
        fclose(file);
        return false;
    }

    // 读取像素数据
    for (int y = 0; y < height; ++y) {
        if (fread(rgb_frame->data[0] + y * rgb_frame->linesize[0],
                  1, width * 3, file) != static_cast<size_t>(width * 3)) {
            LOG_ERROR("Failed to read pixel data");
            fclose(file);
            return false;
        }
    }

    fclose(file);
    LOG_INFO("RGB frame loaded from: %s", filename.c_str());
    return true;
}

size_t FormatConverter::CalculateFrameSize(int width, int height, AVPixelFormat format) {
    return av_image_get_buffer_size(format, width, height, 1);
}

std::unique_ptr<SwsConverter> FormatConverter::CreateConverter(
    int width, int height, AVPixelFormat src_fmt, AVPixelFormat dst_fmt) {

    auto converter = std::make_unique<SwsConverter>();

    SwsConverter::Config config;
    config.src_width = width;
    config.src_height = height;
    config.src_format = src_fmt;
    config.dst_width = width;
    config.dst_height = height;
    config.dst_format = dst_fmt;
    config.quality = SwsConverter::Quality::BILINEAR;

    if (!converter->Init(config)) {
        return nullptr;
    }

    return converter;
}
