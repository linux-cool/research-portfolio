# 第三阶段：FFmpeg 与 V4L2 集成

## 阶段概述
本阶段目标是理解 FFmpeg 如何通过 V4L2 接口采集视频，分析 FFmpeg 中 V4L2 相关源码，掌握硬件编解码 (V4L2 M2M) 的使用。

**预计时间**: 2-3 周

## 学习目标
- [ ] 理解 FFmpeg v4l2 输入设备实现原理
- [ ] 掌握 V4L2 M2M 硬件编解码使用
- [ ] 了解 DMA-BUF 和零拷贝技术
- [ ] 能分析 FFmpeg 中 V4L2 相关源码

## 每周学习计划

### 第1周：FFmpeg V4L2 输入设备

#### 学习内容
| 主题 | 内容 |
|------|------|
| v4l2 设备使用 | -f v4l2 参数 |
| 设备能力查询 | 分辨率/格式/帧率 |
| 源码分析 | libavdevice/v4l2.c |

#### 核心命令
```bash
# 列出 V4L2 设备支持的格式
ffmpeg -f v4l2 -list_formats all -i /dev/video0

# 指定格式采集
ffmpeg -f v4l2 -input_format mjpeg -video_size 1920x1080 \
       -framerate 30 -i /dev/video0 -c:v libx264 output.mp4

# 使用不同的输入格式
ffmpeg -f v4l2 -input_format yuyv422 -i /dev/video0 output.mp4
ffmpeg -f v4l2 -input_format nv12 -i /dev/video0 output.mp4
```

#### FFmpeg 源码分析
```
ffmpeg/libavdevice/
├── v4l2.c              # V4L2 输入设备实现
├── v4l2-common.c       # V4L2 公共函数
└── v4l2-common.h       # V4L2 结构定义

关键函数:
├── v4l2_read_header()  # 打开设备, 设置格式
├── v4l2_read_packet()  # 读取帧数据
├── v4l2_read_close()   # 关闭设备
└── v4l2_set_parameters() # 设置采集参数
```

#### 源码阅读要点
```c
// v4l2.c 核心流程
1. open() 打开设备
2. ioctl(VIDIOC_QUERYCAP) 查询能力
3. ioctl(VIDIOC_S_FMT) 设置格式
4. ioctl(VIDIOC_REQBUFS) 请求缓冲区
5. mmap() 映射缓冲区
6. ioctl(VIDIOC_STREAMON) 开始采集
7. ioctl(VIDIOC_DQBUF) 获取帧 → 转换为 AVPacket
```

---

### 第2周：V4L2 M2M 硬件编解码

#### 学习内容
| 主题 | 内容 |
|------|------|
| M2M 概念 | Memory-to-Memory 编解码 |
| 硬件编码器 | h264_v4l2m2m |
| 硬件解码器 | h264_v4l2m2m |
| 源码分析 | libavcodec/v4l2_m2m*.c |

#### 核心命令
```bash
# 查看可用的 V4L2 M2M 编解码器
ffmpeg -encoders | grep v4l2
ffmpeg -decoders | grep v4l2

# 使用 V4L2 硬件编码
ffmpeg -f v4l2 -i /dev/video0 -c:v h264_v4l2m2m -b:v 4M output.mp4

# 使用 V4L2 硬件解码
ffmpeg -c:v h264_v4l2m2m -i input.mp4 -f null -

# 采集 + 硬件编码 + 推流
ffmpeg -f v4l2 -video_size 1920x1080 -i /dev/video0 \
       -c:v h264_v4l2m2m -b:v 4M \
       -f flv rtmp://server/live/stream
```

#### FFmpeg M2M 源码结构
```
ffmpeg/libavcodec/
├── v4l2_m2m.c          # M2M 框架核心
├── v4l2_m2m.h          # M2M 头文件
├── v4l2_m2m_enc.c      # 硬件编码器
├── v4l2_m2m_dec.c      # 硬件解码器
├── v4l2_buffers.c      # 缓冲区管理
└── v4l2_context.c      # V4L2 上下文

关键结构:
├── V4L2m2mContext      # M2M 上下文
├── V4L2Context         # 输入/输出上下文
└── V4L2Buffer          # 缓冲区封装
```

---

### 第3周：DMA-BUF 与零拷贝

#### 学习内容
| 主题 | 内容 |
|------|------|
| DMA-BUF | 跨设备内存共享 |
| 零拷贝 | 避免 CPU 拷贝 |
| hwaccel | 硬件加速框架 |

#### 零拷贝流程
```
┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│   Camera    │    │    GPU/     │    │   Display/  │
│   Sensor    │───▶│   Encoder   │───▶│   Network   │
└─────────────┘    └─────────────┘    └─────────────┘
        │                 │                 │
        └────────────────┬┘                 │
                         │                  │
                  ┌──────▼──────┐           │
                  │   DMA-BUF   │───────────┘
                  │  (共享内存)  │
                  └─────────────┘
```

#### 性能对比实验
```bash
# 软件编码 (CPU 拷贝)
time ffmpeg -f v4l2 -i /dev/video0 -c:v libx264 -frames 1000 out.mp4

# 硬件编码 (零拷贝)
time ffmpeg -f v4l2 -i /dev/video0 -c:v h264_v4l2m2m -frames 1000 out.mp4

# 观察 CPU 占用
htop
```

## 实践项目

### 项目1: 对比软硬件编码
- 采集相同视频源
- 分别使用 libx264 和 h264_v4l2m2m
- 对比 CPU 占用、延迟、质量

### 项目2: 阅读 FFmpeg V4L2 源码
- 绘制 v4l2.c 的调用流程图
- 理解 mmap 缓冲区管理
- 分析 AVPacket 如何从 V4L2 buffer 构造

## 参考资料

### 源码路径
```bash
# FFmpeg 源码
git clone https://git.ffmpeg.org/ffmpeg.git
cd ffmpeg/libavdevice/  # V4L2 输入设备
cd ffmpeg/libavcodec/   # V4L2 M2M 编解码
```

### 官方文档
- FFmpeg V4L2: https://ffmpeg.org/ffmpeg-devices.html#v4l2
- V4L2 M2M: https://docs.kernel.org/userspace-api/media/v4l/dev-mem2mem.html

## 阶段验收标准
- [ ] 能解释 FFmpeg V4L2 采集的完整流程
- [ ] 能使用 V4L2 M2M 硬件编解码
- [ ] 能阅读并理解 FFmpeg V4L2 相关源码
- [ ] 产出源码分析文档

## 文件结构
```
第三阶段_FFmpeg与V4L2集成/
├── README.md
├── src/
│   └── v4l2_capture_demo.c  # V4L2采集示例
├── analysis/
│   ├── v4l2_source_analysis.md  # 源码分析
│   └── m2m_flow.md              # M2M流程分析
└── benchmarks/
    └── soft_vs_hard.sh          # 性能对比脚本
```

---
进入下一阶段: [第四阶段_V4L2用户空间编程](../第四阶段_V4L2用户空间编程/README.md)

