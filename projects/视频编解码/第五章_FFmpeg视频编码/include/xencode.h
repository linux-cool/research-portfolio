#pragma once

#include "common.h"
#include <memory>
#include <functional>

// 使用common.h中定义的CodecType

/**
 * @brief 编码质量预设
 */
enum class QualityPreset {
    ULTRAFAST,      // 最快编码
    SUPERFAST,      // 超快编码
    VERYFAST,       // 很快编码
    FASTER,         // 较快编码
    FAST,           // 快速编码
    MEDIUM,         // 中等编码
    SLOW,           // 慢速编码
    SLOWER,         // 较慢编码
    VERYSLOW,       // 很慢编码
    PLACEBO         // 最慢编码（最高质量）
};

/**
 * @brief 编码器配置
 */
struct EncodeConfig {
    // 基本参数
    int width;                  // 视频宽度
    int height;                 // 视频高度
    AVPixelFormat pixel_format; // 像素格式
    AVRational frame_rate;      // 帧率
    AVRational time_base;       // 时间基
    
    // 编码参数
    CodecType codec_type;       // 编码器类型
    int64_t bit_rate;          // 码率 (bps)
    int gop_size;              // GOP大小
    int max_b_frames;          // 最大B帧数
    QualityPreset preset;      // 质量预设
    
    // 高级参数
    int crf;                   // 恒定质量因子 (0-51, -1表示不使用)
    int qmin, qmax;           // 量化参数范围
    bool use_global_header;    // 是否使用全局头
    std::string profile;       // 编码profile
    std::string level;         // 编码level
    
    // 硬件加速
    bool enable_hw_accel;      // 是否启用硬件加速
    std::string hw_device;     // 硬件设备类型 ("cuda", "vaapi", "qsv")
    
    // 回调函数
    std::function<void(const AVPacket*)> packet_callback;  // 编码包回调
    std::function<void(const std::string&)> error_callback; // 错误回调
    
    EncodeConfig() {
        width = 1920;
        height = 1080;
        pixel_format = AV_PIX_FMT_YUV420P;
        frame_rate = {30, 1};
        time_base = {1, 30};
        codec_type = CodecType::H264;
        bit_rate = 2000000;  // 2Mbps
        gop_size = 30;
        max_b_frames = 3;
        preset = QualityPreset::MEDIUM;
        crf = -1;
        qmin = 10;
        qmax = 51;
        use_global_header = false;
        profile = "high";
        level = "4.0";
        enable_hw_accel = false;
        hw_device = "";
    }
};

/**
 * @brief 编码统计信息
 */
struct EncodeStats {
    uint64_t frames_encoded;    // 已编码帧数
    uint64_t bytes_encoded;     // 已编码字节数
    double avg_fps;             // 平均编码FPS
    double avg_bitrate;         // 平均码率
    double avg_quality;         // 平均质量
    int64_t encode_time_ms;     // 总编码时间(毫秒)
    
    EncodeStats() : frames_encoded(0), bytes_encoded(0), avg_fps(0.0),
                   avg_bitrate(0.0), avg_quality(0.0), encode_time_ms(0) {}
};

/**
 * @brief 视频编码器基类
 */
class XEncode {
public:
    XEncode();
    virtual ~XEncode();
    
    /**
     * @brief 初始化编码器
     * @param config 编码配置
     * @return 成功返回true
     */
    virtual bool Init(const EncodeConfig& config);
    
    /**
     * @brief 编码一帧
     * @param frame 输入帧，nullptr表示flush
     * @return 成功返回true
     */
    virtual bool Encode(const AVFrame* frame);
    
    /**
     * @brief 刷新编码器缓冲区
     * @return 成功返回true
     */
    virtual bool Flush();
    
    /**
     * @brief 关闭编码器
     */
    virtual void Close();
    
    /**
     * @brief 获取编码统计信息
     */
    virtual EncodeStats GetStats() const;
    
    /**
     * @brief 设置编码参数
     * @param key 参数名
     * @param value 参数值
     * @return 成功返回true
     */
    virtual bool SetParameter(const std::string& key, const std::string& value);
    
    /**
     * @brief 获取编码器信息
     */
    virtual std::string GetEncoderInfo() const;
    
    /**
     * @brief 检查是否已初始化
     */
    bool IsInitialized() const { return initialized_; }
    
    /**
     * @brief 获取配置
     */
    EncodeConfig GetConfig() const { return config_; }

protected:
    /**
     * @brief 创建编码器上下文
     * @return 成功返回true
     */
    virtual bool CreateEncoder();
    
    /**
     * @brief 配置编码器参数
     * @return 成功返回true
     */
    virtual bool ConfigureEncoder();
    
    /**
     * @brief 处理编码后的包
     * @param packet 编码包
     */
    virtual void ProcessPacket(AVPacket* packet);
    
    /**
     * @brief 更新统计信息
     * @param packet 编码包
     * @param encode_time_us 编码时间(微秒)
     */
    virtual void UpdateStats(const AVPacket* packet, int64_t encode_time_us);

    /**
     * @brief 设置硬件加速
     * @return 成功返回true
     */
    virtual bool SetupHardwareAccel();

protected:
    AVCodecContext* codec_ctx_;
    const AVCodec* codec_;
    EncodeConfig config_;
    bool initialized_;
    
    // 统计信息
    mutable std::mutex stats_mutex_;
    EncodeStats stats_;
    std::chrono::steady_clock::time_point start_time_;
    
    // 硬件加速
    AVBufferRef* hw_device_ctx_;
    AVPixelFormat hw_pixel_format_;
};

/**
 * @brief H.264编码器
 */
class H264Encoder : public XEncode {
public:
    H264Encoder();
    ~H264Encoder() override;
    
    bool Init(const EncodeConfig& config) override;
    std::string GetEncoderInfo() const override;

protected:
    bool CreateEncoder() override;
    bool ConfigureEncoder() override;
    
private:
    void SetH264SpecificOptions();
};

/**
 * @brief H.265编码器
 */
class H265Encoder : public XEncode {
public:
    H265Encoder();
    ~H265Encoder() override;
    
    bool Init(const EncodeConfig& config) override;
    std::string GetEncoderInfo() const override;

protected:
    bool CreateEncoder() override;
    bool ConfigureEncoder() override;
    
private:
    void SetH265SpecificOptions();
};

/**
 * @brief 编码器工厂
 */
class XEncodeFactory {
public:
    /**
     * @brief 创建编码器
     * @param codec_type 编码器类型
     * @return 编码器实例
     */
    static std::unique_ptr<XEncode> Create(CodecType codec_type);
    
    /**
     * @brief 获取支持的编码器列表
     */
    static std::vector<CodecType> GetSupportedCodecs();
    
    /**
     * @brief 检查编码器是否支持
     * @param codec_type 编码器类型
     * @return 支持返回true
     */
    static bool IsCodecSupported(CodecType codec_type);
    
    /**
     * @brief 获取编码器名称
     * @param codec_type 编码器类型
     * @return 编码器名称
     */
    static std::string GetCodecName(CodecType codec_type);
    
    /**
     * @brief 获取质量预设名称
     * @param preset 质量预设
     * @return 预设名称
     */
    static std::string GetPresetName(QualityPreset preset);
};

/**
 * @brief 编码器辅助工具
 */
class EncodeUtils {
public:
    /**
     * @brief 计算推荐码率
     * @param width 宽度
     * @param height 高度
     * @param fps 帧率
     * @param codec_type 编码器类型
     * @return 推荐码率(bps)
     */
    static int64_t CalculateRecommendedBitrate(int width, int height, 
                                              double fps, CodecType codec_type);
    
    /**
     * @brief 验证编码配置
     * @param config 编码配置
     * @return 有效返回true
     */
    static bool ValidateConfig(const EncodeConfig& config);
    
    /**
     * @brief 获取硬件加速设备列表
     * @return 设备列表
     */
    static std::vector<std::string> GetHardwareDevices();
    
    /**
     * @brief 检查硬件加速是否可用
     * @param device 设备类型
     * @param codec_type 编码器类型
     * @return 可用返回true
     */
    static bool IsHardwareAccelAvailable(const std::string& device, CodecType codec_type);
};
