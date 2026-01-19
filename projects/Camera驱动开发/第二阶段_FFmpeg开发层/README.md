# 第二阶段：FFmpeg 开发层

## 阶段概述
本阶段目标是使用 FFmpeg API (libav*) 开发音视频应用程序，掌握解封装、解码、编码、封装的完整流程。

**预计时间**: 3-4 周

## 学习目标
- [ ] 理解 FFmpeg 库结构 (libavformat, libavcodec, libavutil...)
- [ ] 掌握解封装流程 (demuxing)
- [ ] 掌握解码流程 (decoding)
- [ ] 掌握编码流程 (encoding)
- [ ] 掌握封装流程 (muxing)
- [ ] 实现简单的视频播放器/转码器

## 每周学习计划

### 第1周：libavformat - 封装/解封装

#### 学习内容
| 主题 | 核心API |
|------|---------|
| 打开输入 | `avformat_open_input()` |
| 获取流信息 | `avformat_find_stream_info()` |
| 读取数据包 | `av_read_frame()` |
| 创建输出 | `avformat_alloc_output_context2()` |
| 写入数据包 | `av_write_frame()` / `av_interleaved_write_frame()` |

#### 核心数据结构
```c
AVFormatContext    // 封装格式上下文
AVStream           // 流信息
AVPacket           // 压缩数据包
AVIOContext        // I/O 上下文
```

#### 实践项目
```c
// 解封装基础流程
AVFormatContext *fmt_ctx = NULL;
avformat_open_input(&fmt_ctx, filename, NULL, NULL);
avformat_find_stream_info(fmt_ctx, NULL);

// 遍历流
for (int i = 0; i < fmt_ctx->nb_streams; i++) {
    AVStream *stream = fmt_ctx->streams[i];
    // 处理视频流/音频流
}

// 读取数据包
AVPacket pkt;
while (av_read_frame(fmt_ctx, &pkt) >= 0) {
    // 处理数据包
    av_packet_unref(&pkt);
}
```

---

### 第2周：libavcodec - 编解码

#### 学习内容
| 主题 | 核心API |
|------|---------|
| 查找解码器 | `avcodec_find_decoder()` |
| 打开解码器 | `avcodec_open2()` |
| 发送数据包 | `avcodec_send_packet()` |
| 接收帧 | `avcodec_receive_frame()` |
| 编码流程 | `avcodec_send_frame()` / `avcodec_receive_packet()` |

#### 核心数据结构
```c
AVCodecContext     // 编解码器上下文
AVCodec            // 编解码器
AVFrame            // 原始帧 (YUV/PCM)
AVPacket           // 压缩包 (H.264/AAC)
```

#### 解码流程
```c
// 查找并打开解码器
const AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
avcodec_parameters_to_context(codec_ctx, stream->codecpar);
avcodec_open2(codec_ctx, codec, NULL);

// 解码循环
AVFrame *frame = av_frame_alloc();
avcodec_send_packet(codec_ctx, &pkt);
while (avcodec_receive_frame(codec_ctx, frame) >= 0) {
    // 处理解码后的帧
}
```

---

### 第3周：libswscale/libswresample - 格式转换

#### 学习内容
| 主题 | 核心API |
|------|---------|
| 视频缩放 | `sws_getContext()`, `sws_scale()` |
| 像素格式转换 | YUV420P → RGB24 等 |
| 音频重采样 | `swr_alloc_set_opts()`, `swr_convert()` |

#### 像素格式转换示例
```c
struct SwsContext *sws_ctx = sws_getContext(
    src_width, src_height, AV_PIX_FMT_YUV420P,
    dst_width, dst_height, AV_PIX_FMT_RGB24,
    SWS_BILINEAR, NULL, NULL, NULL
);

sws_scale(sws_ctx,
          (const uint8_t *const *)src_frame->data, src_frame->linesize,
          0, src_height,
          dst_frame->data, dst_frame->linesize);
```

---

### 第4周：综合项目 - 简单播放器/转码器

#### 项目1: 简单视频解码器
```
┌─────────────┐    ┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│  打开文件   │───▶│   解封装    │───▶│    解码     │───▶│  保存YUV   │
│ avformat    │    │ AVPacket    │    │  AVFrame    │    │   文件     │
└─────────────┘    └─────────────┘    └─────────────┘    └─────────────┘
```

#### 项目2: 视频转码器
```
输入文件 → 解封装 → 解码 → [滤镜处理] → 编码 → 封装 → 输出文件
```

## 参考资料

### 头文件
```c
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
```

### 编译命令
```bash
gcc -o player player.c \
    $(pkg-config --cflags --libs libavformat libavcodec libavutil libswscale)
```

### 官方示例
- FFmpeg 源码 `doc/examples/` 目录

## 阶段验收标准
- [ ] 能独立实现视频文件解析
- [ ] 能实现视频解码并保存 YUV
- [ ] 能实现简单的转码程序
- [ ] 理解 FFmpeg 的线程安全注意事项

## 文件结构
```
第二阶段_FFmpeg开发层/
├── README.md
├── src/
│   ├── demuxer/        # 解封装示例
│   ├── decoder/        # 解码示例
│   ├── encoder/        # 编码示例
│   └── transcoder/     # 转码示例
├── examples/
└── docs/
```

---
进入下一阶段: [第三阶段_FFmpeg与V4L2集成](../第三阶段_FFmpeg与V4L2集成/README.md)

