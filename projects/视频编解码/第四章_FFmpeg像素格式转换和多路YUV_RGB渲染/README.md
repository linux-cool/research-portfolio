# 第四章 FFmpeg像素格式转换和多路YUV、RGB渲染

## 章节概述
本章节深入探讨FFmpeg中的像素格式转换技术，重点介绍sws_scale接口的使用、多路视频渲染架构设计，以及YUV和RGB格式的高效处理方法。

## 主要内容

### sws_scale接口
- SwsContext上下文创建和配置
- 像素格式转换算法选择
- 缩放质量参数优化
- 内存对齐和性能调优

#### sws_scale核心API
```c
struct SwsContext *sws_getContext(
    int srcW, int srcH, enum AVPixelFormat srcFormat,
    int dstW, int dstH, enum AVPixelFormat dstFormat,
    int flags, SwsFilter *srcFilter,
    SwsFilter *dstFilter, const double *param
);

int sws_scale(struct SwsContext *c,
    const uint8_t *const srcSlice[], const int srcStride[],
    int srcSliceY, int srcSliceH,
    uint8_t *const dst[], const int dstStride[]
);
```

### 像素格式转换函数分析
- 支持的像素格式枚举
- 转换算法性能对比
- 色彩空间转换矩阵
- 精度损失和质量评估

#### 常用像素格式转换
- **YUV420P → RGB24**: 最常用的转换路径
- **YUV422P → RGBA**: 支持透明通道
- **RGB24 → YUV420P**: 编码前预处理
- **NV12 → RGB**: 硬件解码常用格式

### YUV420P转换为RGBA并写入文件
- YUV420P格式解析和处理
- RGBA像素数据组织
- 文件写入优化策略
- 内存管理和错误处理

#### 转换实现示例
```c
// YUV420P to RGBA conversion
SwsContext *sws_ctx = sws_getContext(
    width, height, AV_PIX_FMT_YUV420P,
    width, height, AV_PIX_FMT_RGBA,
    SWS_BILINEAR, NULL, NULL, NULL
);

uint8_t *rgba_buffer = (uint8_t*)malloc(width * height * 4);
uint8_t *dst_data[4] = {rgba_buffer, NULL, NULL, NULL};
int dst_linesize[4] = {width * 4, 0, 0, 0};

sws_scale(sws_ctx, frame->data, frame->linesize,
          0, height, dst_data, dst_linesize);
```

### 像素格式RGBA转换为YUV420P
- RGBA到YUV的逆向转换
- 色彩空间压缩处理
- 采样率降低算法
- 质量保持策略

### 多路YUV_RGB文件播放器
- 多文件同步播放架构
- 内存池管理优化
- 线程安全的播放控制
- 用户交互界面设计

#### 多路播放器架构
```
文件1 -> 解码器1 -> 格式转换1 -> 渲染器1
文件2 -> 解码器2 -> 格式转换2 -> 渲染器2
文件3 -> 解码器3 -> 格式转换3 -> 渲染器3
                    |
                同步控制器
```

#### 需求和设计说明
- **功能需求**: 支持多路视频同时播放
- **性能需求**: 4路1080p@30fps同时播放
- **用户需求**: 简洁的控制界面
- **扩展需求**: 支持不同格式混合播放

#### 界面设计和实现
- QT/GTK界面框架选择
- 多窗口布局管理
- 播放控制按钮设计
- 进度条和时间显示

#### 支持多路视频播放
- 多线程解码架构
- 内存资源分配策略
- 同步机制设计
- 错误恢复处理

#### 多路视频支持不同的帧数播放
- 独立帧率控制
- 时间轴同步算法
- 缓冲区管理策略
- 丢帧和补帧机制

## 技术要点
- 深入理解sws_scale转换机制
- 掌握多种像素格式特点和转换
- 实现高效的多路视频播放
- 优化内存使用和性能表现

## 性能优化策略
- **SIMD加速**: 利用SSE/AVX指令集
- **多线程并行**: 转换和渲染分离
- **内存池复用**: 减少内存分配开销
- **硬件加速**: GPU转换支持

## 支持的像素格式
| 格式 | 描述 | 用途 |
|------|------|------|
| YUV420P | 平面YUV，4:2:0采样 | 视频编码标准格式 |
| YUV422P | 平面YUV，4:2:2采样 | 专业视频处理 |
| RGB24 | 24位RGB | 图像显示 |
| RGBA | 32位RGBA | 支持透明度 |
| NV12 | 半平面YUV | 硬件解码输出 |

## 实验环境
- FFmpeg 4.4+ 或 5.x
- swscale库支持
- 多线程环境
- 图形显示支持

## 性能指标
- 4路1080p@30fps同时转换和播放
- 转换延迟 < 5ms
- 内存使用 < 500MB
- CPU使用率 < 60%

## 相关文件
```
第四章_FFmpeg像素格式转换和多路YUV_RGB渲染/
├── README.md              # 本文档
├── src/                   # 源代码实现
│   ├── sws_converter/     # sws_scale封装
│   ├── multi_player/      # 多路播放器
│   ├── format_converter/  # 格式转换器
│   └── ui/                # 用户界面
├── examples/              # 示例代码
├── test_files/            # 测试视频文件
└── docs/                  # 详细文档
```

## 学习目标
完成本章学习后，您将能够：
1. 熟练使用sws_scale进行像素格式转换
2. 理解各种像素格式的特点和应用场景
3. 设计和实现多路视频播放系统
4. 优化视频处理的性能和资源使用
