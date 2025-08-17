#pragma once

#include "common.h"
#include "xdemux.h"
#include <memory>
#include <functional>
#include <thread>
#include <atomic>
#include <queue>
#include <condition_variable>

// 前向声明
class XMux;

/**
 * @brief RTSP连接状态
 */
enum class RTSPState {
    DISCONNECTED,   // 未连接
    CONNECTING,     // 连接中
    CONNECTED,      // 已连接
    PLAYING,        // 播放中
    PAUSED,         // 暂停
    ERROR           // 错误状态
};

/**
 * @brief RTSP配置
 */
struct RTSPConfig {
    std::string url;                    // RTSP URL
    std::string username;               // 用户名
    std::string password;               // 密码
    int timeout_ms;                     // 超时时间（毫秒）
    int buffer_size;                    // 缓冲区大小
    bool enable_tcp;                    // 是否使用TCP传输
    bool enable_audio;                  // 是否启用音频
    bool enable_video;                  // 是否启用视频
    
    // 重连配置
    bool auto_reconnect;                // 是否自动重连
    int max_reconnect_attempts;         // 最大重连次数
    int reconnect_interval_ms;          // 重连间隔（毫秒）
    
    // 回调函数
    std::function<void(AVPacket*, int)> packet_callback;    // 包回调
    std::function<void(RTSPState)> state_callback;          // 状态回调
    std::function<void(const std::string&)> error_callback; // 错误回调
    
    RTSPConfig() : timeout_ms(10000), buffer_size(1024*1024),
                  enable_tcp(false), enable_audio(true), enable_video(true),
                  auto_reconnect(true), max_reconnect_attempts(5),
                  reconnect_interval_ms(3000) {}
};

/**
 * @brief RTSP统计信息
 */
struct RTSPStats {
    uint64_t packets_received;          // 已接收包数
    uint64_t bytes_received;            // 已接收字节数
    uint64_t video_packets;             // 视频包数
    uint64_t audio_packets;             // 音频包数
    uint64_t dropped_packets;           // 丢包数
    double avg_bitrate_kbps;            // 平均码率（kbps）
    int64_t connection_time_ms;         // 连接时间（毫秒）
    int reconnect_count;                // 重连次数
    RTSPState current_state;            // 当前状态
    
    RTSPStats() : packets_received(0), bytes_received(0), video_packets(0),
                 audio_packets(0), dropped_packets(0), avg_bitrate_kbps(0.0),
                 connection_time_ms(0), reconnect_count(0), 
                 current_state(RTSPState::DISCONNECTED) {}
};

/**
 * @brief RTSP客户端基类
 */
class XRTSPClient {
public:
    XRTSPClient();
    virtual ~XRTSPClient();
    
    /**
     * @brief 连接RTSP服务器
     * @param config RTSP配置
     * @return 成功返回true
     */
    virtual bool Connect(const RTSPConfig& config);
    
    /**
     * @brief 断开连接
     */
    virtual void Disconnect();
    
    /**
     * @brief 开始播放
     * @return 成功返回true
     */
    virtual bool Play();
    
    /**
     * @brief 暂停播放
     * @return 成功返回true
     */
    virtual bool Pause();
    
    /**
     * @brief 停止播放
     * @return 成功返回true
     */
    virtual bool Stop();
    
    /**
     * @brief 获取统计信息
     */
    virtual RTSPStats GetStats() const;
    
    /**
     * @brief 获取当前状态
     */
    RTSPState GetState() const { return state_; }
    
    /**
     * @brief 检查是否已连接
     */
    bool IsConnected() const { return state_ == RTSPState::CONNECTED || state_ == RTSPState::PLAYING; }
    
    /**
     * @brief 获取媒体信息
     */
    virtual MediaInfo GetMediaInfo() const;

protected:
    /**
     * @brief 接收线程函数
     */
    virtual void ReceiveThread();
    
    /**
     * @brief 重连线程函数
     */
    virtual void ReconnectThread();
    
    /**
     * @brief 处理接收的包
     * @param packet 包
     */
    virtual void ProcessPacket(AVPacket* packet);
    
    /**
     * @brief 更新统计信息
     * @param packet 包
     */
    virtual void UpdateStats(const AVPacket* packet);
    
    /**
     * @brief 设置状态
     * @param state 新状态
     */
    virtual void SetState(RTSPState state);
    
    /**
     * @brief 尝试重连
     * @return 成功返回true
     */
    virtual bool TryReconnect();

protected:
    AVFormatContext* format_ctx_;
    RTSPConfig config_;
    MediaInfo media_info_;
    
    std::atomic<RTSPState> state_;
    std::atomic<bool> running_;
    std::atomic<bool> should_reconnect_;
    
    std::thread receive_thread_;
    std::thread reconnect_thread_;
    
    // 统计信息
    mutable std::mutex stats_mutex_;
    RTSPStats stats_;
    std::chrono::steady_clock::time_point start_time_;
    
    // 包队列
    std::queue<AVPacket*> packet_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    size_t max_queue_size_;
};

/**
 * @brief RTSP录制配置
 */
struct RTSPRecordConfig {
    std::string rtsp_url;               // RTSP URL
    std::string output_file;            // 输出文件
    std::string output_format;          // 输出格式
    int64_t max_duration_ms;            // 最大录制时长（毫秒，0表示无限制）
    int64_t max_file_size;              // 最大文件大小（字节，0表示无限制）
    
    // RTSP配置
    RTSPConfig rtsp_config;
    
    // 录制回调
    std::function<void(const std::string&)> file_completed_callback; // 文件完成回调
    std::function<void(int64_t, int64_t)> progress_callback;         // 进度回调(当前时长ms, 文件大小)
    
    RTSPRecordConfig() : max_duration_ms(0), max_file_size(0) {}
};

/**
 * @brief RTSP录制器
 */
class XRTSPRecorder {
public:
    XRTSPRecorder();
    virtual ~XRTSPRecorder();
    
    /**
     * @brief 开始录制
     * @param config 录制配置
     * @return 成功返回true
     */
    virtual bool StartRecord(const RTSPRecordConfig& config);
    
    /**
     * @brief 停止录制
     */
    virtual void StopRecord();
    
    /**
     * @brief 暂停录制
     * @return 成功返回true
     */
    virtual bool PauseRecord();
    
    /**
     * @brief 恢复录制
     * @return 成功返回true
     */
    virtual bool ResumeRecord();
    
    /**
     * @brief 检查是否正在录制
     */
    bool IsRecording() const { return recording_; }
    
    /**
     * @brief 获取录制统计信息
     */
    virtual RTSPStats GetRecordStats() const;
    
    /**
     * @brief 获取当前录制文件信息
     */
    virtual std::string GetCurrentFile() const { return current_file_; }

protected:
    /**
     * @brief 录制线程函数
     */
    virtual void RecordThread();
    
    /**
     * @brief 处理录制包
     * @param packet 包
     * @param stream_index 流索引
     */
    virtual void ProcessRecordPacket(AVPacket* packet, int stream_index);
    
    /**
     * @brief 检查是否需要分割文件
     * @return 需要分割返回true
     */
    virtual bool ShouldSplitFile();
    
    /**
     * @brief 创建新的录制文件
     * @return 成功返回true
     */
    virtual bool CreateNewFile();

protected:
    std::unique_ptr<XRTSPClient> rtsp_client_;
    std::unique_ptr<XMux> muxer_;
    
    RTSPRecordConfig config_;
    std::atomic<bool> recording_;
    std::atomic<bool> paused_;
    
    std::thread record_thread_;
    
    std::string current_file_;
    int64_t current_duration_ms_;
    int64_t current_file_size_;
    int file_sequence_;
    
    mutable std::mutex record_mutex_;
};

/**
 * @brief RTSP多路录制器
 */
class XRTSPMultiRecorder {
public:
    XRTSPMultiRecorder();
    virtual ~XRTSPMultiRecorder();
    
    /**
     * @brief 添加录制任务
     * @param id 任务ID
     * @param config 录制配置
     * @return 成功返回true
     */
    virtual bool AddRecordTask(const std::string& id, const RTSPRecordConfig& config);
    
    /**
     * @brief 移除录制任务
     * @param id 任务ID
     * @return 成功返回true
     */
    virtual bool RemoveRecordTask(const std::string& id);
    
    /**
     * @brief 开始所有录制任务
     * @return 成功返回true
     */
    virtual bool StartAllRecords();
    
    /**
     * @brief 停止所有录制任务
     */
    virtual void StopAllRecords();
    
    /**
     * @brief 获取任务列表
     */
    virtual std::vector<std::string> GetTaskIds() const;
    
    /**
     * @brief 获取任务统计信息
     * @param id 任务ID
     */
    virtual RTSPStats GetTaskStats(const std::string& id) const;

protected:
    std::map<std::string, std::unique_ptr<XRTSPRecorder>> recorders_;
    mutable std::mutex recorders_mutex_;
};

/**
 * @brief RTSP工具类
 */
class RTSPUtils {
public:
    /**
     * @brief 解析RTSP URL
     * @param url RTSP URL
     * @param host 输出主机名
     * @param port 输出端口
     * @param path 输出路径
     * @return 成功返回true
     */
    static bool ParseURL(const std::string& url, std::string& host, int& port, std::string& path);
    
    /**
     * @brief 验证RTSP URL格式
     * @param url RTSP URL
     * @return 有效返回true
     */
    static bool ValidateURL(const std::string& url);
    
    /**
     * @brief 测试RTSP连接
     * @param url RTSP URL
     * @param timeout_ms 超时时间（毫秒）
     * @return 连接成功返回true
     */
    static bool TestConnection(const std::string& url, int timeout_ms = 5000);
    
    /**
     * @brief 获取RTSP流信息
     * @param url RTSP URL
     * @param timeout_ms 超时时间（毫秒）
     * @return 媒体信息
     */
    static MediaInfo GetStreamInfo(const std::string& url, int timeout_ms = 10000);
    
    /**
     * @brief 生成录制文件名
     * @param base_name 基础文件名
     * @param sequence 序列号
     * @param timestamp 时间戳
     * @return 生成的文件名
     */
    static std::string GenerateFileName(const std::string& base_name, 
                                       int sequence = 0, 
                                       int64_t timestamp = 0);
};
