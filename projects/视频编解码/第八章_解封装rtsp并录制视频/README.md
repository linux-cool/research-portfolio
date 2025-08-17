# 第八章 解封装rtsp并录制视频

## 章节概述
本章节深入探讨RTSP协议的解封装和视频录制技术，涵盖RTSP协议分析、大华和海康相机集成、实时解封装、多线程录制、以及相机重连等核心内容。

## 主要内容

### RTSP协议详细分析包括鉴权协议分析
- RTSP协议基础和消息格式
- 会话建立和管理流程
- RTP/RTCP数据传输协议
- 鉴权机制详解(Basic/Digest)
- NAT穿透和防火墙处理

#### RTSP协议交互流程
```
客户端                    服务器
  |                        |
  |--- OPTIONS ----------->|  (查询支持的方法)
  |<-- 200 OK -------------|
  |                        |
  |--- DESCRIBE ---------->|  (获取媒体描述)
  |<-- 200 OK + SDP ------|
  |                        |
  |--- SETUP ------------->|  (建立传输会话)
  |<-- 200 OK + Session --|
  |                        |
  |--- PLAY -------------->|  (开始播放)
  |<-- 200 OK -------------|
  |                        |
  |<== RTP数据流 =========|
```

#### 鉴权协议实现
```c
// Digest认证示例
char *auth_header = "Digest username=\"admin\", "
                   "realm=\"IP Camera\", "
                   "nonce=\"dcd98b7102dd2f0e8b11d0f600bfb0c093\", "
                   "uri=\"rtsp://192.168.1.100/stream1\", "
                   "response=\"6629fae49393a05397450978507c4ef1\"";
```

### 大华和海康相机设置和rtsp协议抓包分析
- 相机网络配置和参数设置
- RTSP URL格式和参数说明
- Wireshark抓包分析技巧
- 厂商特定协议扩展
- 故障诊断和调试方法

#### 常用RTSP URL格式
```bash
# 海康威视
rtsp://admin:password@192.168.1.100:554/Streaming/Channels/101

# 大华
rtsp://admin:password@192.168.1.100:554/cam/realmonitor?channel=1&subtype=0

# 通用格式
rtsp://username:password@ip:port/path
```

### 实现解封装XDemuxTask线程类
- 多线程解封装架构设计
- 任务队列和调度机制
- 线程安全和同步控制
- 资源管理和异常处理

#### XDemuxTask类设计
```cpp
class XDemuxTask : public QThread {
    Q_OBJECT
    
public:
    explicit XDemuxTask(QObject *parent = nullptr);
    virtual ~XDemuxTask();
    
    bool Start(const std::string& url);
    void Stop();
    void Pause();
    void Resume();
    
signals:
    void PacketReady(AVPacket* packet, int stream_index);
    void Error(const QString& error_msg);
    void Connected();
    void Disconnected();
    
protected:
    void run() override;
    
private:
    bool OpenStream(const std::string& url);
    void CloseStream();
    bool ReadPacket();
    void HandleError(int error_code);
    
    AVFormatContext* fmt_ctx_;
    std::atomic<bool> running_;
    std::atomic<bool> paused_;
    std::mutex mutex_;
    std::condition_variable cv_;
};
```

### 解封装rtsp断网或摄像机重启后自动重连处理
- 网络状态监控机制
- 自动重连策略设计
- 指数退避算法实现
- 连接状态通知机制

#### 重连策略实现
```cpp
class ReconnectManager {
private:
    int max_retry_count_ = 10;
    int base_delay_ms_ = 1000;
    int max_delay_ms_ = 30000;
    
public:
    bool ShouldRetry(int attempt) {
        return attempt < max_retry_count_;
    }
    
    int GetRetryDelay(int attempt) {
        int delay = base_delay_ms_ * (1 << attempt);  // 指数退避
        return std::min(delay, max_delay_ms_);
    }
};
```

### 使用责任链设计模式低耦合传递数据
- 责任链模式在媒体处理中的应用
- 处理器链的设计和实现
- 数据传递和转换机制
- 可扩展的处理架构

#### 责任链模式实现
```cpp
class MediaProcessor {
public:
    virtual ~MediaProcessor() = default;
    virtual bool Process(MediaData* data) = 0;
    virtual void SetNext(std::shared_ptr<MediaProcessor> next) {
        next_ = next;
    }
    
protected:
    bool PassToNext(MediaData* data) {
        if (next_) {
            return next_->Process(data);
        }
        return true;
    }
    
private:
    std::shared_ptr<MediaProcessor> next_;
};

class PacketDecoder : public MediaProcessor {
public:
    bool Process(MediaData* data) override {
        // 解码处理
        if (DecodePacket(data)) {
            return PassToNext(data);
        }
        return false;
    }
};
```

### 线程安全的XAVPacketList实现
- 线程安全的数据结构设计
- 读写锁和条件变量使用
- 内存管理和引用计数
- 性能优化和无锁设计

#### XAVPacketList实现
```cpp
class XAVPacketList {
public:
    XAVPacketList(size_t max_size = 100);
    ~XAVPacketList();
    
    bool Push(AVPacket* packet, int timeout_ms = -1);
    AVPacket* Pop(int timeout_ms = -1);
    void Clear();
    size_t Size() const;
    bool IsFull() const;
    
private:
    mutable std::mutex mutex_;
    std::condition_variable not_full_;
    std::condition_variable not_empty_;
    std::queue<AVPacket*> packets_;
    size_t max_size_;
    std::atomic<bool> stopped_;
};
```

### rtsp的多线程责任链解封装解码和渲染
- 多线程处理管线设计
- 解封装、解码、渲染分离
- 线程间通信和同步
- 性能监控和调优

#### 多线程架构
```
RTSP源 -> 解封装线程 -> 包队列 -> 解码线程 -> 帧队列 -> 渲染线程
   |          |           |         |           |         |
网络接收   协议解析    缓冲管理   视频解码    格式转换   显示输出
```

### 相机录制重新封装处理pts计算问题
- PTS/DTS时间戳处理
- 时间基转换和同步
- 录制文件时间轴连续性
- 断流重连时间戳修正

#### PTS计算和修正
```cpp
class PTSCalculator {
private:
    int64_t start_pts_ = AV_NOPTS_VALUE;
    int64_t last_pts_ = AV_NOPTS_VALUE;
    int64_t pts_offset_ = 0;
    
public:
    int64_t CalculatePTS(int64_t original_pts, AVRational time_base) {
        if (start_pts_ == AV_NOPTS_VALUE) {
            start_pts_ = original_pts;
            pts_offset_ = 0;
        }
        
        int64_t adjusted_pts = original_pts - start_pts_ + pts_offset_;
        
        // 处理时间戳跳跃
        if (last_pts_ != AV_NOPTS_VALUE) {
            int64_t diff = adjusted_pts - last_pts_;
            if (diff < 0 || diff > MAX_PTS_JUMP) {
                pts_offset_ += (last_pts_ + EXPECTED_FRAME_DURATION) - adjusted_pts;
                adjusted_pts = last_pts_ + EXPECTED_FRAME_DURATION;
            }
        }
        
        last_pts_ = adjusted_pts;
        return adjusted_pts;
    }
};
```

## RTSP协议技术要点
- **实时性**: 低延迟传输和处理
- **可靠性**: 网络异常处理和恢复
- **兼容性**: 支持多厂商设备
- **安全性**: 鉴权和加密传输

## 性能优化策略
- **缓冲管理**: 合理的缓冲区大小设置
- **多线程**: 解封装、解码、渲染分离
- **内存池**: 减少内存分配开销
- **网络优化**: TCP/UDP传输选择

## 错误处理机制
- **网络错误**: 超时、断线、DNS解析失败
- **协议错误**: 格式错误、版本不兼容
- **资源错误**: 内存不足、文件写入失败
- **设备错误**: 相机离线、权限不足

## 实验环境
- FFmpeg 4.4+ (支持RTSP)
- 网络摄像头或RTSP服务器
- 网络分析工具(Wireshark)
- 多线程调试工具

## 性能指标
- 1080p@25fps RTSP流实时处理
- 网络延迟 < 200ms
- 重连时间 < 5s
- 内存使用 < 150MB
- CPU使用率 < 50%

## 相关文件
```
第八章_解封装rtsp并录制视频/
├── README.md              # 本文档
├── src/                   # 源代码实现
│   ├── rtsp_client/       # RTSP客户端实现
│   ├── xdemux_task/       # XDemuxTask线程类
│   ├── packet_list/       # 线程安全包列表
│   ├── recorder/          # 录制功能实现
│   └── processors/        # 责任链处理器
├── examples/              # 示例代码
├── configs/               # 相机配置文件
└── docs/                  # 详细文档
```

## 学习目标
完成本章学习后，您将能够：
1. 深入理解RTSP协议和实时流媒体传输
2. 实现稳定的网络摄像头接入和录制
3. 设计高效的多线程媒体处理架构
4. 处理网络异常和设备重连问题
5. 优化实时视频处理的性能和稳定性
