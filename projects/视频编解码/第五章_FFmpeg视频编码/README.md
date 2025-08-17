# 第五章 FFmpeg视频编码

## 章节概述
本章节深入探讨FFmpeg视频编码技术，涵盖编码原理分析、AVPacket结构详解、x265跨平台编译、H.264编码算法原理，以及XEncode编码类封装等核心内容。

## 主要内容

### 视频编码原理分析
- 视频压缩基础理论
- 时间冗余和空间冗余消除
- 帧间预测和帧内预测
- 变换编码和量化
- 熵编码技术

#### 编码流程概述
```
原始视频 -> 预处理 -> 预测 -> 变换 -> 量化 -> 熵编码 -> 码流输出
    |         |        |       |       |        |         |
   YUV      滤波    运动估计   DCT    量化表   CABAC     H.264
```

### 压缩域AVPacket结构体详解
- AVPacket数据结构分析
- 压缩数据的组织方式
- 时间戳(PTS/DTS)管理
- 关键帧标识和处理
- 内存管理和引用计数

#### AVPacket核心字段
```c
typedef struct AVPacket {
    uint8_t *data;          // 压缩数据指针
    int size;               // 数据大小
    int64_t pts;            // 显示时间戳
    int64_t dts;            // 解码时间戳
    int flags;              // 标志位(关键帧等)
    int stream_index;       // 流索引
    // ... 其他字段
} AVPacket;
```

### x265源码跨平台编译
- x265编译环境配置
- CMake构建系统使用
- 跨平台编译注意事项
- 依赖库管理和链接
- 编译优化参数设置

#### 编译步骤
```bash
# Linux/macOS编译
mkdir build && cd build
cmake ../source -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Windows编译
cmake ../source -G "Visual Studio 16 2019"
cmake --build . --config Release
```

### FFmpeg编码代码实验（h264和h265）
- H.264编码器配置和使用
- H.265编码器性能对比
- 编码参数优化实验
- 质量评估和码率控制

#### H.264编码示例
```c
AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);
AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);

codec_ctx->width = 1920;
codec_ctx->height = 1080;
codec_ctx->time_base = (AVRational){1, 30};
codec_ctx->framerate = (AVRational){30, 1};
codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
codec_ctx->bit_rate = 2000000;

avcodec_open2(codec_ctx, codec, NULL);
```

### H264编码算法原理（宏块、帧内预测...）
- 宏块(Macroblock)结构和分割
- 帧内预测模式详解
- 帧间预测和运动估计
- 变换和量化过程
- 环路滤波技术

#### 宏块预测模式
- **Intra 4x4**: 4x4块帧内预测
- **Intra 16x16**: 16x16块帧内预测
- **Inter P**: P帧帧间预测
- **Inter B**: B帧双向预测

### x264 x265编码参数设置
- 预设(preset)参数选择
- 码率控制模式配置
- 质量参数(CRF/QP)设置
- 高级编码选项调优

#### 关键编码参数
```c
// x264参数设置
av_opt_set(codec_ctx->priv_data, "preset", "medium", 0);
av_opt_set(codec_ctx->priv_data, "tune", "zerolatency", 0);
codec_ctx->bit_rate = 2000000;
codec_ctx->rc_max_rate = 2500000;
codec_ctx->rc_buffer_size = 1000000;
```

#### GOP ultrafast zerolatency
- **GOP结构**: I/P/B帧组织方式
- **ultrafast预设**: 最快编码速度
- **zerolatency调优**: 低延迟编码
- **实时编码**: 直播应用优化

#### ABR CQP QP参数
- **ABR (Average Bit Rate)**: 平均码率控制
- **CQP (Constant Quantization Parameter)**: 恒定量化参数
- **QP (Quantization Parameter)**: 量化参数范围

#### CBR CRF VBV
- **CBR (Constant Bit Rate)**: 恒定码率
- **CRF (Constant Rate Factor)**: 恒定质量因子
- **VBV (Video Buffering Verifier)**: 缓冲区验证

### H264裸流分析（NALU SPS PPS IDR）
- NALU (Network Abstraction Layer Unit) 结构
- SPS (Sequence Parameter Set) 解析
- PPS (Picture Parameter Set) 分析
- IDR (Instantaneous Decoder Refresh) 帧处理

#### NALU类型
| 类型 | 值 | 描述 |
|------|----|----- |
| SPS | 7 | 序列参数集 |
| PPS | 8 | 图像参数集 |
| IDR | 5 | 即时解码刷新帧 |
| P帧 | 1 | 预测帧 |
| B帧 | 2 | 双向预测帧 |

### XEncode 编码类封装
- 面向对象的编码器设计
- 多编码器统一接口
- 参数配置和管理
- 错误处理和异常恢复

#### XEncode类设计
```cpp
class XEncode {
public:
    virtual bool Init(const EncodeParams& params) = 0;
    virtual bool Encode(AVFrame* frame, AVPacket* packet) = 0;
    virtual void Close() = 0;
    
protected:
    AVCodecContext* codec_ctx_;
    AVCodec* codec_;
};

class X264Encoder : public XEncode {
    // H.264编码器实现
};

class X265Encoder : public XEncode {
    // H.265编码器实现
};
```

## 技术要点
- 深入理解视频编码原理和算法
- 掌握FFmpeg编码API的使用
- 熟悉H.264/H.265编码标准
- 实现高效的编码器封装

## 性能优化
- 多线程编码支持
- 硬件加速集成
- 内存池管理
- SIMD指令优化

## 质量评估指标
- **PSNR**: 峰值信噪比
- **SSIM**: 结构相似性
- **VMAF**: 视频多方法评估融合
- **BD-Rate**: 比特率-失真性能

## 实验环境
- FFmpeg 4.4+ 或 5.x
- x264/x265编码库
- C/C++编译环境
- 性能分析工具

## 性能指标
- 1080p@30fps实时编码
- H.264编码速度 > 100fps
- H.265编码速度 > 50fps
- 内存使用 < 300MB

## 相关文件
```
第五章_FFmpeg视频编码/
├── README.md              # 本文档
├── src/                   # 源代码实现
│   ├── encoders/          # 编码器实现
│   ├── xencode/           # XEncode封装类
│   ├── analysis/          # 码流分析工具
│   └── utils/             # 工具函数
├── examples/              # 示例代码
├── test_videos/           # 测试视频
└── docs/                  # 详细文档
```

## 学习目标
完成本章学习后，您将能够：
1. 深入理解视频编码原理和H.264/H.265标准
2. 熟练使用FFmpeg进行视频编码
3. 优化编码参数以平衡质量和性能
4. 设计可扩展的编码器架构
