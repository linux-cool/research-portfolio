#pragma once

#include "common.h"
#include "avframe_manager.h"
#include "xvideo_view.h"
#include "sws_converter.h"
#include <map>
#include <atomic>

/**
 * @brief 视频流信息
 */
struct VideoStreamInfo {
    std::string filename;
    int width, height;
    AVPixelFormat format;
    AVRational frame_rate;
    AVRational time_base;
    int64_t duration;  // 微秒
    bool is_valid;
    
    VideoStreamInfo() : width(0), height(0), format(AV_PIX_FMT_NONE),
                       frame_rate({0, 1}), time_base({1, 1000000}),
                       duration(0), is_valid(false) {}
};

/**
 * @brief 单个视频播放器
 */
class VideoPlayer {
public:
    enum class State {
        STOPPED,
        PLAYING,
        PAUSED,
        ERROR
    };
    
    explicit VideoPlayer(int player_id);
    ~VideoPlayer();
    
    /**
     * @brief 打开视频文件
     * @param filename 文件名
     * @return 成功返回true
     */
    bool Open(const std::string& filename);
    
    /**
     * @brief 关闭视频
     */
    void Close();
    
    /**
     * @brief 开始播放
     * @return 成功返回true
     */
    bool Play();
    
    /**
     * @brief 暂停播放
     */
    void Pause();
    
    /**
     * @brief 停止播放
     */
    void Stop();
    
    /**
     * @brief 跳转到指定时间
     * @param timestamp_us 时间戳(微秒)
     * @return 成功返回true
     */
    bool Seek(int64_t timestamp_us);
    
    /**
     * @brief 设置播放速度
     * @param speed 播放速度倍数
     */
    void SetPlaybackSpeed(double speed);
    
    /**
     * @brief 获取下一帧
     * @param frame 输出帧
     * @return 成功返回true，到达文件末尾返回false
     */
    bool GetNextFrame(AVFrame* frame);
    
    /**
     * @brief 获取当前播放时间
     * @return 当前时间戳(微秒)
     */
    int64_t GetCurrentTime() const;
    
    /**
     * @brief 获取播放状态
     */
    State GetState() const { return state_; }
    
    /**
     * @brief 获取视频信息
     */
    VideoStreamInfo GetVideoInfo() const { return video_info_; }
    
    /**
     * @brief 获取播放器ID
     */
    int GetPlayerID() const { return player_id_; }

private:
    int player_id_;
    State state_;
    VideoStreamInfo video_info_;
    
    AVFormatContext* format_ctx_;
    AVCodecContext* codec_ctx_;
    int video_stream_index_;
    
    double playback_speed_;
    int64_t start_time_;
    int64_t current_pts_;
    
    mutable std::mutex player_mutex_;
    
    bool InitializeDecoder();
    void CleanupDecoder();
    bool ReadPacketAndDecode(AVFrame* frame);
};

/**
 * @brief 多路视频播放器
 */
class MultiVideoPlayer {
public:
    struct PlayerConfig {
        int player_id;
        std::string filename;
        int render_x, render_y;      // 渲染位置
        int render_width, render_height;  // 渲染大小
        bool enable_audio;           // 是否启用音频
        double playback_speed;       // 播放速度
        
        PlayerConfig() : player_id(-1), render_x(0), render_y(0),
                        render_width(0), render_height(0),
                        enable_audio(false), playback_speed(1.0) {}
    };
    
    explicit MultiVideoPlayer(size_t max_players = 4);
    ~MultiVideoPlayer();
    
    /**
     * @brief 添加播放器
     * @param config 播放器配置
     * @return 播放器ID，失败返回-1
     */
    int AddPlayer(const PlayerConfig& config);
    
    /**
     * @brief 移除播放器
     * @param player_id 播放器ID
     * @return 成功返回true
     */
    bool RemovePlayer(int player_id);
    
    /**
     * @brief 开始所有播放器
     * @return 成功返回true
     */
    bool StartAll();
    
    /**
     * @brief 暂停所有播放器
     */
    void PauseAll();
    
    /**
     * @brief 停止所有播放器
     */
    void StopAll();
    
    /**
     * @brief 同步所有播放器到指定时间
     * @param timestamp_us 时间戳(微秒)
     * @return 成功返回true
     */
    bool SyncAll(int64_t timestamp_us);
    
    /**
     * @brief 设置主渲染器
     * @param renderer 渲染器
     * @param width 渲染区域宽度
     * @param height 渲染区域高度
     * @return 成功返回true
     */
    bool SetRenderer(std::unique_ptr<XVideoView> renderer, int width, int height);
    
    /**
     * @brief 开始渲染循环
     * @return 成功返回true
     */
    bool StartRendering();
    
    /**
     * @brief 停止渲染循环
     */
    void StopRendering();
    
    /**
     * @brief 获取播放器统计信息
     */
    struct PlayerStats {
        int player_id;
        VideoPlayer::State state;
        int64_t current_time_us;
        double fps;
        size_t frames_rendered;
        size_t frames_dropped;
    };
    
    std::vector<PlayerStats> GetPlayersStats() const;
    
    /**
     * @brief 设置同步模式
     * @param enable_sync 是否启用同步
     */
    void SetSyncMode(bool enable_sync) { sync_enabled_ = enable_sync; }

private:
    size_t max_players_;
    std::map<int, std::unique_ptr<VideoPlayer>> players_;
    std::map<int, PlayerConfig> player_configs_;
    
    // 渲染相关
    std::unique_ptr<XVideoView> main_renderer_;
    int render_width_, render_height_;
    std::unique_ptr<std::thread> render_thread_;
    std::atomic<bool> rendering_;
    
    // 同步相关
    bool sync_enabled_;
    int64_t sync_start_time_;
    std::mutex sync_mutex_;
    
    // 帧管理
    std::shared_ptr<AVFrameManager> frame_manager_;
    std::unique_ptr<SwsConverter> format_converter_;
    
    // 统计信息
    mutable std::mutex stats_mutex_;
    std::map<int, PlayerStats> player_stats_;
    
    void RenderLoop();
    bool RenderFrame();
    bool CompositeFrames(const std::map<int, AVFrame*>& frames, AVFrame* output_frame);
    void UpdatePlayerStats(int player_id, const PlayerStats& stats);
    int64_t GetSyncTime() const;
};

/**
 * @brief 多路播放器工厂
 */
class MultiPlayerFactory {
public:
    /**
     * @brief 创建多路播放器
     * @param max_players 最大播放器数量
     * @param renderer_type 渲染器类型
     * @return 播放器实例
     */
    static std::unique_ptr<MultiVideoPlayer> CreateMultiPlayer(
        size_t max_players = 4,
        XVideoViewFactory::RendererType renderer_type = XVideoViewFactory::RendererType::SDL);
    
    /**
     * @brief 创建预设配置的多路播放器
     * @param layout 布局类型 ("2x2", "1+3", "4x1")
     * @param filenames 文件列表
     * @param total_width 总宽度
     * @param total_height 总高度
     * @return 播放器实例
     */
    static std::unique_ptr<MultiVideoPlayer> CreateWithLayout(
        const std::string& layout,
        const std::vector<std::string>& filenames,
        int total_width = 1280,
        int total_height = 720);
};
