#pragma once

#include "common.h"
#include <memory>
#include <functional>

/**
 * @brief 解码器配置
 */
struct DecodeConfig {
    // 基本参数
    CodecType codec_type;       // 解码器类型
    int width, height;          // 视频尺寸（可选，用于验证）
    AVPixelFormat pixel_format; // 输出像素格式
    
    // 硬件加速
    bool enable_hw_accel;       // 是否启用硬件加速
    std::string hw_device;      // 硬件设备类型 ("cuda", "vaapi", "qsv")
    
    // 性能参数
    int thread_count;           // 解码线程数
    bool enable_multithreading; // 是否启用多线程解码
    
    // 回调函数
    std::function<void(AVFrame*)> frame_callback;    // 解码帧回调
    std::function<void(const std::string&)> error_callback; // 错误回调
    
    DecodeConfig() {
        codec_type = CodecType::H264;
        width = 0;
        height = 0;
        pixel_format = AV_PIX_FMT_YUV420P;
        enable_hw_accel = false;
        hw_device = "";
        thread_count = 0;  // 0表示自动检测
        enable_multithreading = true;
    }
};

/**
 * @brief 解码统计信息
 */
struct DecodeStats {
    uint64_t frames_decoded;    // 已解码帧数
    uint64_t bytes_decoded;     // 已解码字节数
    double avg_fps;             // 平均解码FPS
    double avg_decode_time_ms;  // 平均解码时间(毫秒)
    int64_t total_time_ms;      // 总解码时间(毫秒)
    uint64_t errors_count;      // 错误计数
    
    DecodeStats() : frames_decoded(0), bytes_decoded(0), avg_fps(0.0),
                   avg_decode_time_ms(0.0), total_time_ms(0), errors_count(0) {}
};

/**
 * @brief 视频解码器基类
 */
class XDecode {
public:
    XDecode();
    virtual ~XDecode();
    
    /**
     * @brief 初始化解码器
     * @param config 解码配置
     * @return 成功返回true
     */
    virtual bool Init(const DecodeConfig& config);
    
    /**
     * @brief 解码一个包
     * @param packet 输入包，nullptr表示flush
     * @return 成功返回true
     */
    virtual bool Decode(const AVPacket* packet);
    
    /**
     * @brief 刷新解码器缓冲区
     * @return 成功返回true
     */
    virtual bool Flush();
    
    /**
     * @brief 关闭解码器
     */
    virtual void Close();
    
    /**
     * @brief 获取解码统计信息
     */
    virtual DecodeStats GetStats() const;
    
    /**
     * @brief 设置解码参数
     * @param key 参数名
     * @param value 参数值
     * @return 成功返回true
     */
    virtual bool SetParameter(const std::string& key, const std::string& value);
    
    /**
     * @brief 获取解码器信息
     */
    virtual std::string GetDecoderInfo() const;
    
    /**
     * @brief 检查是否已初始化
     */
    bool IsInitialized() const { return initialized_; }
    
    /**
     * @brief 获取配置
     */
    DecodeConfig GetConfig() const { return config_; }

protected:
    /**
     * @brief 创建解码器上下文
     * @return 成功返回true
     */
    virtual bool CreateDecoder();
    
    /**
     * @brief 配置解码器参数
     * @return 成功返回true
     */
    virtual bool ConfigureDecoder();
    
    /**
     * @brief 处理解码后的帧
     * @param frame 解码帧
     */
    virtual void ProcessFrame(AVFrame* frame);
    
    /**
     * @brief 更新统计信息
     * @param frame 解码帧
     * @param decode_time_us 解码时间(微秒)
     */
    virtual void UpdateStats(const AVFrame* frame, int64_t decode_time_us);
    
    /**
     * @brief 设置硬件加速
     * @return 成功返回true
     */
    virtual bool SetupHardwareAccel();

protected:
    AVCodecContext* codec_ctx_;
    const AVCodec* codec_;
    DecodeConfig config_;
    bool initialized_;
    
    // 统计信息
    mutable std::mutex stats_mutex_;
    DecodeStats stats_;
    std::chrono::steady_clock::time_point start_time_;
    
    // 硬件加速
    AVBufferRef* hw_device_ctx_;
    AVPixelFormat hw_pixel_format_;
};

/**
 * @brief H.264解码器
 */
class H264Decoder : public XDecode {
public:
    H264Decoder();
    ~H264Decoder() override;
    
    bool Init(const DecodeConfig& config) override;
    std::string GetDecoderInfo() const override;

protected:
    bool CreateDecoder() override;
    bool ConfigureDecoder() override;
    
private:
    void SetH264SpecificOptions();
};

/**
 * @brief H.265解码器
 */
class H265Decoder : public XDecode {
public:
    H265Decoder();
    ~H265Decoder() override;
    
    bool Init(const DecodeConfig& config) override;
    std::string GetDecoderInfo() const override;

protected:
    bool CreateDecoder() override;
    bool ConfigureDecoder() override;
    
private:
    void SetH265SpecificOptions();
};

/**
 * @brief 解码器工厂
 */
class XDecodeFactory {
public:
    /**
     * @brief 创建解码器
     * @param codec_type 解码器类型
     * @return 解码器实例
     */
    static std::unique_ptr<XDecode> Create(CodecType codec_type);
    
    /**
     * @brief 获取支持的解码器列表
     */
    static std::vector<CodecType> GetSupportedCodecs();
    
    /**
     * @brief 检查解码器是否支持
     * @param codec_type 解码器类型
     * @return 支持返回true
     */
    static bool IsCodecSupported(CodecType codec_type);
    
    /**
     * @brief 获取解码器名称
     * @param codec_type 解码器类型
     * @return 解码器名称
     */
    static std::string GetCodecName(CodecType codec_type);
};

/**
 * @brief 解码器辅助工具
 */
class DecodeUtils {
public:
    /**
     * @brief 从文件头检测编码格式
     * @param data 数据缓冲区
     * @param size 数据大小
     * @return 检测到的编码格式
     */
    static CodecType DetectCodecType(const uint8_t* data, size_t size);
    
    /**
     * @brief 验证解码配置
     * @param config 解码配置
     * @return 有效返回true
     */
    static bool ValidateConfig(const DecodeConfig& config);
    
    /**
     * @brief 获取硬件解码设备列表
     * @return 设备列表
     */
    static std::vector<std::string> GetHardwareDevices();
    
    /**
     * @brief 检查硬件解码是否可用
     * @param device 设备类型
     * @param codec_type 解码器类型
     * @return 可用返回true
     */
    static bool IsHardwareDecodeAvailable(const std::string& device, CodecType codec_type);
    
    /**
     * @brief 获取推荐的线程数
     * @return 推荐线程数
     */
    static int GetRecommendedThreadCount();
};
