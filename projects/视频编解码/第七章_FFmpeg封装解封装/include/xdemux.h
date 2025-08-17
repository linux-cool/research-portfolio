#pragma once

#include "common.h"
#include <memory>
#include <functional>
#include <vector>
#include <map>

/**
 * @brief 流信息
 */
struct StreamInfo {
    int index;                  // 流索引
    AVMediaType type;          // 流类型
    CodecType codec_type;      // 编码类型
    AVRational time_base;      // 时间基
    AVRational frame_rate;     // 帧率（视频流）
    int64_t duration;          // 时长（时间基单位）
    int width, height;         // 尺寸（视频流）
    int sample_rate;           // 采样率（音频流）
    int channels;              // 声道数（音频流）
    int64_t bit_rate;          // 码率
    std::string codec_name;    // 编码器名称
    bool is_valid;             // 是否有效
    
    StreamInfo() : index(-1), type(AVMEDIA_TYPE_UNKNOWN), codec_type(CodecType::UNKNOWN),
                  time_base({1, 1000}), frame_rate({0, 1}), duration(0),
                  width(0), height(0), sample_rate(0), channels(0),
                  bit_rate(0), is_valid(false) {}
};

/**
 * @brief 媒体文件信息
 */
struct MediaInfo {
    std::string filename;           // 文件名
    std::string format_name;        // 格式名称
    int64_t duration_us;           // 总时长（微秒）
    int64_t file_size;             // 文件大小
    int64_t bit_rate;              // 总码率
    std::vector<StreamInfo> streams; // 流信息列表
    std::map<std::string, std::string> metadata; // 元数据
    bool is_valid;                 // 是否有效
    
    MediaInfo() : duration_us(0), file_size(0), bit_rate(0), is_valid(false) {}
};

/**
 * @brief 解封装配置
 */
struct DemuxConfig {
    std::string filename;           // 输入文件名
    bool enable_video;             // 是否启用视频流
    bool enable_audio;             // 是否启用音频流
    int video_stream_index;        // 指定视频流索引（-1表示自动选择）
    int audio_stream_index;        // 指定音频流索引（-1表示自动选择）
    
    // 回调函数
    std::function<void(AVPacket*, int)> packet_callback;  // 包回调
    std::function<void(const std::string&)> error_callback; // 错误回调
    
    DemuxConfig() : enable_video(true), enable_audio(true),
                   video_stream_index(-1), audio_stream_index(-1) {}
};

/**
 * @brief 解封装统计信息
 */
struct DemuxStats {
    uint64_t packets_read;         // 已读取包数
    uint64_t bytes_read;           // 已读取字节数
    uint64_t video_packets;        // 视频包数
    uint64_t audio_packets;        // 音频包数
    double avg_read_time_ms;       // 平均读取时间
    int64_t total_time_ms;         // 总时间
    
    DemuxStats() : packets_read(0), bytes_read(0), video_packets(0),
                  audio_packets(0), avg_read_time_ms(0.0), total_time_ms(0) {}
};

/**
 * @brief 解封装器基类
 */
class XDemux {
public:
    XDemux();
    virtual ~XDemux();
    
    /**
     * @brief 打开媒体文件
     * @param config 解封装配置
     * @return 成功返回true
     */
    virtual bool Open(const DemuxConfig& config);
    
    /**
     * @brief 关闭解封装器
     */
    virtual void Close();
    
    /**
     * @brief 读取下一个包
     * @param packet 输出包
     * @return 成功返回true，到达文件末尾返回false
     */
    virtual bool ReadPacket(AVPacket* packet);
    
    /**
     * @brief 跳转到指定时间
     * @param timestamp_us 时间戳（微秒）
     * @param stream_index 流索引（-1表示默认流）
     * @return 成功返回true
     */
    virtual bool Seek(int64_t timestamp_us, int stream_index = -1);
    
    /**
     * @brief 获取媒体信息
     */
    virtual MediaInfo GetMediaInfo() const;
    
    /**
     * @brief 获取统计信息
     */
    virtual DemuxStats GetStats() const;
    
    /**
     * @brief 检查是否已打开
     */
    bool IsOpened() const { return opened_; }
    
    /**
     * @brief 获取视频流索引
     */
    int GetVideoStreamIndex() const { return video_stream_index_; }
    
    /**
     * @brief 获取音频流索引
     */
    int GetAudioStreamIndex() const { return audio_stream_index_; }

protected:
    /**
     * @brief 分析流信息
     * @return 成功返回true
     */
    virtual bool AnalyzeStreams();
    
    /**
     * @brief 处理读取的包
     * @param packet 包
     */
    virtual void ProcessPacket(AVPacket* packet);
    
    /**
     * @brief 更新统计信息
     * @param packet 包
     * @param read_time_us 读取时间（微秒）
     */
    virtual void UpdateStats(const AVPacket* packet, int64_t read_time_us);

protected:
    AVFormatContext* format_ctx_;
    DemuxConfig config_;
    MediaInfo media_info_;
    bool opened_;
    
    int video_stream_index_;
    int audio_stream_index_;
    
    // 统计信息
    mutable std::mutex stats_mutex_;
    DemuxStats stats_;
    std::chrono::steady_clock::time_point start_time_;
};

/**
 * @brief 封装器配置
 */
struct MuxConfig {
    std::string filename;           // 输出文件名
    std::string format_name;        // 格式名称（如"mp4", "avi"）
    
    // 视频流配置
    bool enable_video;             // 是否启用视频流
    CodecType video_codec;         // 视频编码器
    int video_width, video_height; // 视频尺寸
    AVRational video_frame_rate;   // 视频帧率
    int64_t video_bit_rate;        // 视频码率
    
    // 音频流配置
    bool enable_audio;             // 是否启用音频流
    CodecType audio_codec;         // 音频编码器
    int audio_sample_rate;         // 音频采样率
    int audio_channels;            // 音频声道数
    int64_t audio_bit_rate;        // 音频码率
    
    // 回调函数
    std::function<void(const std::string&)> error_callback; // 错误回调
    
    MuxConfig() : enable_video(true), video_codec(CodecType::H264),
                 video_width(1920), video_height(1080),
                 video_frame_rate({30, 1}), video_bit_rate(2000000),
                 enable_audio(false), audio_codec(CodecType::UNKNOWN),
                 audio_sample_rate(44100), audio_channels(2),
                 audio_bit_rate(128000) {}
};

/**
 * @brief 封装统计信息
 */
struct MuxStats {
    uint64_t packets_written;      // 已写入包数
    uint64_t bytes_written;        // 已写入字节数
    uint64_t video_packets;        // 视频包数
    uint64_t audio_packets;        // 音频包数
    double avg_write_time_ms;      // 平均写入时间
    int64_t total_time_ms;         // 总时间
    
    MuxStats() : packets_written(0), bytes_written(0), video_packets(0),
                audio_packets(0), avg_write_time_ms(0.0), total_time_ms(0) {}
};

/**
 * @brief 封装器基类
 */
class XMux {
public:
    XMux();
    virtual ~XMux();
    
    /**
     * @brief 打开输出文件
     * @param config 封装配置
     * @return 成功返回true
     */
    virtual bool Open(const MuxConfig& config);
    
    /**
     * @brief 关闭封装器
     */
    virtual void Close();
    
    /**
     * @brief 写入包
     * @param packet 输入包
     * @param stream_index 流索引
     * @return 成功返回true
     */
    virtual bool WritePacket(AVPacket* packet, int stream_index);
    
    /**
     * @brief 获取统计信息
     */
    virtual MuxStats GetStats() const;
    
    /**
     * @brief 检查是否已打开
     */
    bool IsOpened() const { return opened_; }
    
    /**
     * @brief 获取视频流索引
     */
    int GetVideoStreamIndex() const { return video_stream_index_; }
    
    /**
     * @brief 获取音频流索引
     */
    int GetAudioStreamIndex() const { return audio_stream_index_; }

protected:
    /**
     * @brief 创建输出流
     * @return 成功返回true
     */
    virtual bool CreateStreams();
    
    /**
     * @brief 处理写入的包
     * @param packet 包
     * @param stream_index 流索引
     */
    virtual void ProcessPacket(AVPacket* packet, int stream_index);
    
    /**
     * @brief 更新统计信息
     * @param packet 包
     * @param write_time_us 写入时间（微秒）
     */
    virtual void UpdateStats(const AVPacket* packet, int64_t write_time_us);

protected:
    AVFormatContext* format_ctx_;
    MuxConfig config_;
    bool opened_;
    bool header_written_;
    
    int video_stream_index_;
    int audio_stream_index_;
    
    // 统计信息
    mutable std::mutex stats_mutex_;
    MuxStats stats_;
    std::chrono::steady_clock::time_point start_time_;
};

/**
 * @brief 解封装工厂
 */
class XDemuxFactory {
public:
    /**
     * @brief 创建解封装器
     * @param filename 文件名
     * @return 解封装器实例
     */
    static std::unique_ptr<XDemux> Create(const std::string& filename);
    
    /**
     * @brief 获取支持的格式列表
     */
    static std::vector<std::string> GetSupportedFormats();
    
    /**
     * @brief 检测文件格式
     * @param filename 文件名
     * @return 格式名称
     */
    static std::string DetectFormat(const std::string& filename);
};

/**
 * @brief 封装工厂
 */
class XMuxFactory {
public:
    /**
     * @brief 创建封装器
     * @param format_name 格式名称
     * @return 封装器实例
     */
    static std::unique_ptr<XMux> Create(const std::string& format_name);

    /**
     * @brief 获取支持的格式列表
     */
    static std::vector<std::string> GetSupportedFormats();
};

/**
 * @brief 媒体处理工具类
 */
class MediaUtils {
public:
    /**
     * @brief 重封装媒体文件
     * @param input_file 输入文件
     * @param output_file 输出文件
     * @param output_format 输出格式（可选）
     * @return 成功返回true
     */
    static bool Remux(const std::string& input_file,
                     const std::string& output_file,
                     const std::string& output_format = "");

    /**
     * @brief 提取媒体信息
     * @param filename 文件名
     * @return 媒体信息
     */
    static MediaInfo GetMediaInfo(const std::string& filename);

    /**
     * @brief 视频剪辑
     * @param input_file 输入文件
     * @param output_file 输出文件
     * @param start_time_us 开始时间（微秒）
     * @param duration_us 持续时间（微秒）
     * @return 成功返回true
     */
    static bool Clip(const std::string& input_file,
                    const std::string& output_file,
                    int64_t start_time_us,
                    int64_t duration_us);
};
