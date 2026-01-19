# 第一阶段：FFmpeg 应用层

## 阶段概述
本阶段目标是熟练使用 FFmpeg 命令行工具进行多媒体处理，掌握视频格式转换、编解码、滤镜处理、流媒体推拉流等核心操作。

**预计时间**: 2-3 周

## 学习目标
- [ ] 掌握 FFmpeg 基础命令和参数
- [ ] 理解视频编解码基本概念
- [ ] 熟练进行格式转换和转码
- [ ] 掌握视频滤镜使用
- [ ] 了解流媒体协议 (RTMP/HLS/RTSP)

## 每周学习计划

### 第1周：基础命令与格式转换

#### 学习内容
| 主题 | 内容 |
|------|------|
| FFmpeg 安装 | 编译安装/包管理器安装 |
| 基础概念 | 容器格式 vs 编码格式 |
| 信息查看 | ffprobe 使用 |
| 格式转换 | 容器转换、编码转换 |
| 参数理解 | -i, -c:v, -c:a, -b:v, -crf |

#### 核心命令
```bash
# 查看文件信息
ffprobe -show_streams -show_format input.mp4

# 容器格式转换 (不重新编码)
ffmpeg -i input.mp4 -c copy output.mkv

# 编码格式转换
ffmpeg -i input.mp4 -c:v libx264 -crf 23 -c:a aac output.mp4

# 指定分辨率和码率
ffmpeg -i input.mp4 -c:v libx264 -s 1280x720 -b:v 2M output.mp4

# 提取音频/视频
ffmpeg -i input.mp4 -an video_only.mp4      # 去除音频
ffmpeg -i input.mp4 -vn audio_only.aac      # 去除视频
```

#### 实践项目
- 编写批量格式转换脚本
- 对比不同 CRF 值的质量和文件大小

---

### 第2周：滤镜系统与音视频处理

#### 学习内容
| 主题 | 内容 |
|------|------|
| 滤镜基础 | -vf, -af 参数 |
| 视频滤镜 | scale, crop, overlay, drawtext |
| 音频滤镜 | volume, loudnorm, aecho |
| 滤镜链 | 多滤镜组合 |
| 复杂滤镜 | -filter_complex |

#### 核心命令
```bash
# 缩放视频
ffmpeg -i input.mp4 -vf "scale=1280:720" output.mp4
ffmpeg -i input.mp4 -vf "scale=-1:720" output.mp4  # 保持宽高比

# 裁剪视频
ffmpeg -i input.mp4 -vf "crop=640:480:100:50" output.mp4

# 添加水印
ffmpeg -i input.mp4 -i logo.png -filter_complex "overlay=10:10" output.mp4

# 添加文字
ffmpeg -i input.mp4 -vf "drawtext=text='Hello':fontsize=24:fontcolor=white:x=10:y=10" output.mp4

# 视频拼接
ffmpeg -i input1.mp4 -i input2.mp4 -filter_complex "[0:v][0:a][1:v][1:a]concat=n=2:v=1:a=1" output.mp4

# 调整音量
ffmpeg -i input.mp4 -af "volume=2.0" output.mp4
```

#### 实践项目
- 实现视频水印添加工具
- 批量视频裁剪/缩放脚本

---

### 第3周：流媒体协议与摄像头采集

#### 学习内容
| 主题 | 内容 |
|------|------|
| RTMP | 推流/拉流 |
| HLS | 切片生成/播放 |
| RTSP | 拉流采集 |
| V4L2采集 | 摄像头采集基础 |
| 设备枚举 | 输入设备查看 |

#### 核心命令
```bash
# 查看支持的输入设备
ffmpeg -devices

# 摄像头采集 (Linux V4L2)
ffmpeg -f v4l2 -list_formats all -i /dev/video0
ffmpeg -f v4l2 -video_size 1920x1080 -framerate 30 -i /dev/video0 output.mp4

# 摄像头采集并编码
ffmpeg -f v4l2 -i /dev/video0 -c:v libx264 -preset ultrafast output.mp4

# RTMP 推流
ffmpeg -f v4l2 -i /dev/video0 -c:v libx264 -preset ultrafast \
       -f flv rtmp://server/live/stream

# RTSP 拉流
ffmpeg -rtsp_transport tcp -i rtsp://camera_ip/stream -c copy output.mp4

# 生成 HLS
ffmpeg -i input.mp4 -c:v libx264 -c:a aac \
       -hls_time 10 -hls_list_size 0 -f hls output.m3u8
```

#### 实践项目
- 搭建 RTMP 推流服务 (nginx-rtmp)
- 实现摄像头采集推流脚本

---

## 参考资料

### 官方文档
- FFmpeg 文档: https://ffmpeg.org/ffmpeg.html
- FFmpeg 滤镜: https://ffmpeg.org/ffmpeg-filters.html
- FFmpeg Wiki: https://trac.ffmpeg.org/wiki

### 推荐教程
- FFmpeg 从入门到精通（书籍）
- FFmpeg 官方示例代码

## 阶段验收标准
- [ ] 能独立完成常见格式转换任务
- [ ] 能使用滤镜处理视频
- [ ] 能实现摄像头采集和推流
- [ ] 产出至少3个实用脚本工具

## 文件结构
```
第一阶段_FFmpeg应用层/
├── README.md           # 本文件
├── scripts/            # 实用脚本
│   ├── batch_convert.sh
│   ├── add_watermark.sh
│   └── rtmp_push.sh
├── examples/           # 命令示例
└── notes/              # 学习笔记
```

---
进入下一阶段: [第二阶段_FFmpeg开发层](../第二阶段_FFmpeg开发层/README.md)

