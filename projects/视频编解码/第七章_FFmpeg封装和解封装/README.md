# 第七章 FFmpeg封装和解封装

## 章节概述
本章节深入探讨FFmpeg的封装(Muxing)和解封装(Demuxing)技术，涵盖容器格式分析、MP4封装详解、流程控制、音视频同步等核心内容。

## 主要内容

### 封装分析和MP4封装格式详解
- 容器格式基础理论
- MP4文件结构分析
- Atom/Box层次结构
- 元数据管理和索引
- 兼容性和标准化

#### MP4文件结构
```
MP4文件结构:
├── ftyp (File Type Box)
├── moov (Movie Box)
│   ├── mvhd (Movie Header)
│   ├── trak (Track Box)
│   │   ├── tkhd (Track Header)
│   │   └── mdia (Media Box)
│   │       ├── mdhd (Media Header)
│   │       ├── hdlr (Handler Reference)
│   │       └── minf (Media Information)
│   └── udta (User Data)
└── mdat (Media Data Box)
```

### FFmpeg解封装
- AVFormatContext初始化
- 流信息探测和分析
- 数据包读取和分发
- 时间基转换和同步
- 错误处理和恢复

#### 解封装核心流程
```c
// 打开输入文件
AVFormatContext *fmt_ctx = NULL;
avformat_open_input(&fmt_ctx, filename, NULL, NULL);
avformat_find_stream_info(fmt_ctx, NULL);

// 查找视频和音频流
int video_stream_idx = -1, audio_stream_idx = -1;
for (int i = 0; i < fmt_ctx->nb_streams; i++) {
    if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        video_stream_idx = i;
    else if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        audio_stream_idx = i;
}

// 读取数据包
AVPacket packet;
while (av_read_frame(fmt_ctx, &packet) >= 0) {
    // 处理数据包
    av_packet_unref(&packet);
}
```

#### 流程和接口详解
- `avformat_open_input()`: 打开输入源
- `avformat_find_stream_info()`: 分析流信息
- `av_read_frame()`: 读取数据包
- `av_seek_frame()`: 随机访问定位

#### 区分音视频并解码渲染视频
- 流类型识别和分类
- 多路解码器管理
- 音视频同步机制
- 渲染时序控制

### FFmpeg重封装
- 输出格式上下文创建
- 流复制和转换
- 时间戳重映射
- 元数据传递和修改

#### 重封装实现
```c
// 创建输出上下文
AVFormatContext *out_fmt_ctx = NULL;
avformat_alloc_output_context2(&out_fmt_ctx, NULL, NULL, output_filename);

// 复制流信息
for (int i = 0; i < in_fmt_ctx->nb_streams; i++) {
    AVStream *in_stream = in_fmt_ctx->streams[i];
    AVStream *out_stream = avformat_new_stream(out_fmt_ctx, NULL);
    avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
}

// 写入文件头
avformat_write_header(out_fmt_ctx, NULL);

// 复制数据包
while (av_read_frame(in_fmt_ctx, &packet) >= 0) {
    av_interleaved_write_frame(out_fmt_ctx, &packet);
    av_packet_unref(&packet);
}

// 写入文件尾
av_write_trailer(out_fmt_ctx);
```

#### 流程分析和接口详解
- `avformat_alloc_output_context2()`: 创建输出上下文
- `avformat_new_stream()`: 添加新流
- `avformat_write_header()`: 写入文件头
- `av_interleaved_write_frame()`: 交错写入数据包

#### av_seek_frame进度控制
- 随机访问实现原理
- 关键帧定位算法
- 时间戳计算和转换
- 精确定位策略

#### 重封装实现剪辑视频
- 时间段选择和裁剪
- 无损剪辑实现
- 关键帧对齐处理
- 输出文件优化

### 封装XDemux并实现重封装h265到mp4文件
- 统一解封装接口设计
- H.265码流处理
- MP4容器适配
- 性能优化策略

#### XDemux类设计
```cpp
class XDemux {
public:
    virtual bool Open(const std::string& filename) = 0;
    virtual bool ReadPacket(AVPacket* packet) = 0;
    virtual bool Seek(int64_t timestamp) = 0;
    virtual void Close() = 0;
    
    // 流信息获取
    virtual int GetVideoStreamIndex() const = 0;
    virtual int GetAudioStreamIndex() const = 0;
    virtual AVCodecParameters* GetVideoCodecPar() const = 0;
    virtual AVCodecParameters* GetAudioCodecPar() const = 0;
    
protected:
    AVFormatContext* fmt_ctx_;
    int video_stream_idx_;
    int audio_stream_idx_;
};

class XDemuxImpl : public XDemux {
    // 具体实现
};
```

## 容器格式支持

| 格式 | 扩展名 | 特点 | 应用场景 |
|------|--------|------|----------|
| MP4 | .mp4, .m4v | 广泛兼容，流式友好 | 网络传输，移动设备 |
| MKV | .mkv | 开源，功能丰富 | 高质量存储 |
| AVI | .avi | 简单，兼容性好 | 传统应用 |
| MOV | .mov | 苹果标准，专业 | 视频制作 |
| FLV | .flv | 流媒体优化 | 网络直播 |
| TS | .ts | 广播标准 | 数字电视 |

## 技术要点
- 深入理解容器格式结构和原理
- 掌握FFmpeg封装和解封装API
- 实现高效的流处理和同步
- 优化文件I/O和内存使用

## 性能优化策略
- **缓冲管理**: 优化读写缓冲区大小
- **并行处理**: 多线程读写分离
- **内存池**: 减少内存分配开销
- **索引优化**: 快速随机访问支持

## 时间戳处理
- **时间基(Time Base)**: 不同流的时间单位
- **PTS/DTS**: 显示时间戳和解码时间戳
- **时间戳转换**: 不同时间基之间的转换
- **同步策略**: 音视频同步算法

```c
// 时间戳转换示例
int64_t pts_in_ms = av_rescale_q(packet->pts, 
                                 stream->time_base, 
                                 (AVRational){1, 1000});
```

## 实验环境
- FFmpeg 4.4+ 或 5.x
- 支持多种容器格式
- 文件I/O性能测试工具
- 媒体分析工具

## 性能指标
- 4K视频重封装速度 > 10x实时
- 内存使用 < 100MB
- 随机访问延迟 < 100ms
- 支持文件大小 > 100GB

## 测试用例
```bash
# 格式转换测试
ffmpeg -i input.mkv -c copy output.mp4

# 剪辑测试
ffmpeg -i input.mp4 -ss 00:01:00 -t 00:02:00 -c copy output.mp4

# 流复制测试
ffmpeg -i input.mp4 -map 0:v -map 0:a -c copy output.mkv
```

## 相关文件
```
第七章_FFmpeg封装和解封装/
├── README.md              # 本文档
├── src/                   # 源代码实现
│   ├── demuxer/           # 解封装器实现
│   ├── muxer/             # 封装器实现
│   ├── xdemux/            # XDemux封装类
│   └── utils/             # 工具函数
├── examples/              # 示例代码
│   ├── remux_example/     # 重封装示例
│   ├── clip_example/      # 剪辑示例
│   └── format_convert/    # 格式转换示例
├── test_files/            # 测试文件
└── docs/                  # 详细文档
```

## 学习目标
完成本章学习后，您将能够：
1. 深入理解各种容器格式的结构和特点
2. 熟练使用FFmpeg进行封装和解封装操作
3. 实现高效的媒体文件处理和转换
4. 设计可扩展的媒体处理架构
5. 优化文件I/O和内存使用性能
