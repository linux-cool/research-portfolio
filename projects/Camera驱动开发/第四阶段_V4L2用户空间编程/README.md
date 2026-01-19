# 第四阶段：V4L2 用户空间编程

## 阶段概述
本阶段目标是直接使用 V4L2 API 进行视频采集，脱离 FFmpeg，深入理解 V4L2 ioctl 接口和缓冲区管理机制。

**预计时间**: 3-4 周

## 学习目标
- [ ] 掌握 V4L2 核心 ioctl 接口
- [ ] 理解 MMAP/USERPTR/DMABUF 三种缓冲模式
- [ ] 能实现完整的视频采集程序
- [ ] 掌握 Media Controller 和子设备操作

## 每周学习计划

### 第1周：V4L2 核心 ioctl

#### 学习内容
| ioctl | 功能 |
|-------|------|
| VIDIOC_QUERYCAP | 查询设备能力 |
| VIDIOC_ENUM_FMT | 枚举支持格式 |
| VIDIOC_G_FMT / S_FMT | 获取/设置格式 |
| VIDIOC_REQBUFS | 请求缓冲区 |
| VIDIOC_QUERYBUF | 查询缓冲区信息 |
| VIDIOC_QBUF / DQBUF | 入队/出队缓冲区 |
| VIDIOC_STREAMON / OFF | 开始/停止流 |

#### 核心头文件
```c
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
```

#### 基础采集流程
```c
// 1. 打开设备
int fd = open("/dev/video0", O_RDWR);

// 2. 查询能力
struct v4l2_capability cap;
ioctl(fd, VIDIOC_QUERYCAP, &cap);

// 3. 设置格式
struct v4l2_format fmt = {0};
fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
fmt.fmt.pix.width = 1920;
fmt.fmt.pix.height = 1080;
fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
ioctl(fd, VIDIOC_S_FMT, &fmt);

// 4. 请求缓冲区
struct v4l2_requestbuffers req = {0};
req.count = 4;
req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
req.memory = V4L2_MEMORY_MMAP;
ioctl(fd, VIDIOC_REQBUFS, &req);
```

---

### 第2周：缓冲区管理

#### 三种缓冲模式对比
| 模式 | 特点 | 适用场景 |
|------|------|----------|
| MMAP | 内核分配，用户映射 | 最常用 |
| USERPTR | 用户分配，传给内核 | 自定义内存管理 |
| DMABUF | 跨设备共享 | 零拷贝，硬件加速 |

#### MMAP 模式实现
```c
// 映射缓冲区
struct buffer {
    void *start;
    size_t length;
};
struct buffer buffers[4];

for (int i = 0; i < 4; i++) {
    struct v4l2_buffer buf = {0};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = i;
    ioctl(fd, VIDIOC_QUERYBUF, &buf);
    
    buffers[i].length = buf.length;
    buffers[i].start = mmap(NULL, buf.length,
                            PROT_READ | PROT_WRITE,
                            MAP_SHARED, fd, buf.m.offset);
}

// 入队所有缓冲区
for (int i = 0; i < 4; i++) {
    struct v4l2_buffer buf = {0};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = i;
    ioctl(fd, VIDIOC_QBUF, &buf);
}

// 开始流
enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
ioctl(fd, VIDIOC_STREAMON, &type);
```

---

### 第3周：采集循环与多路采集

#### 采集循环
```c
while (running) {
    // 等待帧就绪
    struct pollfd fds = { .fd = fd, .events = POLLIN };
    poll(&fds, 1, 5000);
    
    // 出队
    struct v4l2_buffer buf = {0};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    ioctl(fd, VIDIOC_DQBUF, &buf);
    
    // 处理帧
    process_frame(buffers[buf.index].start, buf.bytesused);
    
    // 重新入队
    ioctl(fd, VIDIOC_QBUF, &buf);
}
```

#### 多摄像头采集
```c
// 使用 select/poll 同时等待多个设备
struct pollfd fds[2];
fds[0].fd = fd1;
fds[0].events = POLLIN;
fds[1].fd = fd2;
fds[1].events = POLLIN;

while (running) {
    poll(fds, 2, 5000);
    if (fds[0].revents & POLLIN) { /* 处理摄像头1 */ }
    if (fds[1].revents & POLLIN) { /* 处理摄像头2 */ }
}
```

---

### 第4周：Media Controller 和子设备

#### Media Controller
```bash
# 查看媒体拓扑
media-ctl -d /dev/media0 -p

# 设置链接
media-ctl -l "'sensor':0 -> 'csi':0[1]"

# 设置子设备格式
media-ctl -V "'sensor':0[fmt:UYVY8_2X8/1920x1080]"
```

#### 子设备 ioctl
```c
#include <linux/v4l2-subdev.h>

// 获取子设备格式
struct v4l2_subdev_format fmt = {0};
fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
fmt.pad = 0;
ioctl(subdev_fd, VIDIOC_SUBDEV_G_FMT, &fmt);

// 设置子设备格式
fmt.format.width = 1920;
fmt.format.height = 1080;
fmt.format.code = MEDIA_BUS_FMT_UYVY8_2X8;
ioctl(subdev_fd, VIDIOC_SUBDEV_S_FMT, &fmt);
```

## 实践项目

### 项目1: 完整采集程序
- 实现从打开设备到采集帧的完整流程
- 支持保存 YUV/JPEG 文件
- 支持命令行参数设置分辨率/格式

### 项目2: v4l2-ctl 简化版
- 实现设备能力查询
- 实现格式枚举
- 实现格式设置

## 参考工具
```bash
# v4l2-ctl 常用命令
v4l2-ctl --list-devices
v4l2-ctl -d /dev/video0 --all
v4l2-ctl -d /dev/video0 --list-formats-ext
v4l2-ctl -d /dev/video0 --set-fmt-video=width=1920,height=1080
v4l2-ctl -d /dev/video0 --stream-mmap --stream-count=100
```

## 阶段验收标准
- [ ] 能独立编写 V4L2 采集程序
- [ ] 理解三种缓冲模式的区别
- [ ] 能使用 media-ctl 配置管道
- [ ] 产出完整的采集程序代码

## 文件结构
```
第四阶段_V4L2用户空间编程/
├── README.md
├── src/
│   ├── v4l2_capture.c      # 基础采集
│   ├── v4l2_mmap.c         # MMAP模式
│   ├── v4l2_userptr.c      # USERPTR模式
│   ├── v4l2_multi.c        # 多路采集
│   └── media_ctl.c         # Media Controller
├── examples/
└── docs/
```

---
进入下一阶段: [第五阶段_V4L2内核框架](../第五阶段_V4L2内核框架/README.md)

