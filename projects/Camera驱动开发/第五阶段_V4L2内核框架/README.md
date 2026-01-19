# 第五阶段：V4L2 内核框架

## 阶段概述
本阶段目标是深入理解 V4L2 内核子系统架构，阅读内核源码，掌握 v4l2-core、videobuf2、Media Controller 等核心框架。

**预计时间**: 4-6 周

## 学习目标
- [ ] 理解 V4L2 核心框架架构
- [ ] 掌握 videobuf2 缓冲区管理机制
- [ ] 理解 Media Controller 实体/链接模型
- [ ] 能分析现有驱动源码

## 每周学习计划

### 第1-2周：V4L2 核心框架

#### 源码路径
```
drivers/media/v4l2-core/
├── v4l2-dev.c          # video_device 管理
├── v4l2-device.c       # v4l2_device 管理
├── v4l2-subdev.c       # 子设备框架
├── v4l2-ioctl.c        # ioctl 处理
├── v4l2-ctrls-core.c   # 控制器框架
├── v4l2-async.c        # 异步注册
└── v4l2-fwnode.c       # 设备树解析
```

#### 核心数据结构
```c
/* V4L2 设备容器 */
struct v4l2_device {
    struct device *dev;
    struct media_device *mdev;
    struct list_head subdevs;
    struct v4l2_ctrl_handler *ctrl_handler;
};

/* 视频设备节点 */
struct video_device {
    struct media_entity entity;
    const struct v4l2_ioctl_ops *ioctl_ops;
    const struct v4l2_file_operations *fops;
    struct vb2_queue *queue;
    struct v4l2_device *v4l2_dev;
    enum vfl_devnode_type vfl_type;
    int minor;
};

/* 子设备 */
struct v4l2_subdev {
    struct media_entity entity;
    struct v4l2_device *v4l2_dev;
    const struct v4l2_subdev_ops *ops;
    struct v4l2_ctrl_handler *ctrl_handler;
    struct v4l2_subdev_state *active_state;  /* 6.x */
};
```

#### 阅读重点
```
第1周:
├── v4l2-dev.c
│   ├── video_register_device()   # 注册视频设备
│   ├── v4l2_open()               # 打开设备
│   └── v4l2_ioctl()              # ioctl入口
│
├── v4l2-device.c
│   ├── v4l2_device_register()
│   └── v4l2_device_unregister()

第2周:
├── v4l2-subdev.c
│   ├── v4l2_subdev_init()
│   ├── v4l2_subdev_call()        # 子设备调用宏
│   └── subdev pad operations
│
├── v4l2-ioctl.c
│   └── __video_do_ioctl()        # ioctl分发
```

---

### 第3周：videobuf2 框架

#### 源码路径
```
drivers/media/common/videobuf2/
├── videobuf2-core.c        # VB2 核心
├── videobuf2-v4l2.c        # VB2 V4L2 适配
├── videobuf2-memops.c      # 内存操作
├── videobuf2-vmalloc.c     # vmalloc 分配器
├── videobuf2-dma-contig.c  # DMA 连续内存
└── videobuf2-dma-sg.c      # DMA scatter-gather
```

#### 核心数据结构
```c
/* 视频缓冲队列 */
struct vb2_queue {
    unsigned int type;           /* 缓冲区类型 */
    unsigned int io_modes;       /* MMAP/USERPTR/DMABUF */
    const struct vb2_ops *ops;   /* 队列操作 */
    const struct vb2_mem_ops *mem_ops;  /* 内存操作 */
    void *drv_priv;              /* 驱动私有数据 */
    unsigned int buf_struct_size;
    /* ... */
};

/* 视频缓冲区 */
struct vb2_buffer {
    struct vb2_queue *vb2_queue;
    unsigned int index;
    unsigned int type;
    unsigned int memory;
    unsigned int num_planes;
    u64 timestamp;
    enum vb2_buffer_state state;
};
```

#### VB2 操作回调
```c
struct vb2_ops {
    int (*queue_setup)(struct vb2_queue *q, ...);    /* 队列设置 */
    int (*buf_prepare)(struct vb2_buffer *vb);       /* 缓冲区准备 */
    void (*buf_queue)(struct vb2_buffer *vb);        /* 缓冲区入队 */
    int (*start_streaming)(struct vb2_queue *q, ...);/* 开始流 */
    void (*stop_streaming)(struct vb2_queue *q);     /* 停止流 */
};
```

---

### 第4周：Media Controller 框架

#### 源码路径
```
drivers/media/mc/
├── mc-device.c     # media_device 管理
├── mc-devnode.c    # 设备节点
├── mc-entity.c     # 实体/链接管理
└── mc-request.c    # 请求 API
```

#### 核心数据结构
```c
/* 媒体设备 */
struct media_device {
    struct device *dev;
    struct list_head entities;
    struct list_head pads;
    struct list_head links;
};

/* 媒体实体 */
struct media_entity {
    const char *name;
    u32 function;            /* MEDIA_ENT_F_* */
    u16 num_pads;
    struct media_pad *pads;
    struct list_head links;
};

/* 媒体 Pad */
struct media_pad {
    struct media_entity *entity;
    u16 index;
    unsigned long flags;     /* SINK/SOURCE */
};
```

---

### 第5-6周：现有驱动分析

#### 推荐分析的驱动
```
drivers/media/i2c/           # 传感器驱动
├── ov5640.c                 # 经典 MIPI 传感器
├── imx219.c                 # 树莓派传感器
└── imx477.c                 # 高端传感器

drivers/media/platform/      # 平台驱动
├── sunxi/sun6i-csi/         # 全志 CSI
├── rockchip/rkisp1/         # 瑞芯微 ISP
└── ti/                       # TI 平台
```

#### 驱动分析要点
```
传感器驱动分析:
├── probe() 流程
│   ├── 资源获取 (clk, gpio, regulator)
│   ├── 芯片 ID 验证
│   ├── 子设备初始化
│   └── 异步注册
│
├── 子设备操作
│   ├── pad ops (enum/get/set_fmt)
│   ├── video ops (s_stream)
│   └── core ops
│
└── 控制器
    ├── 曝光、增益
    └── 帧率控制

平台驱动分析:
├── probe() 流程
├── video_device 注册
├── vb2_queue 初始化
├── DMA 配置
└── 中断处理
```

## 实践项目

### 项目1: V4L2 框架流程图
- 绘制 ioctl 处理流程
- 绘制缓冲区流转过程
- 绘制设备注册流程

### 项目2: 驱动源码注释
- 为 ov5640.c 添加中文注释
- 理解每个函数的作用

## 参考资料

### 内核文档
- https://docs.kernel.org/driver-api/media/v4l2-core.html
- https://docs.kernel.org/driver-api/media/mc-core.html

### 调试方法
```bash
# 启用 V4L2 调试
echo 0x1f > /sys/module/videobuf2_common/parameters/debug
echo 3 > /sys/class/video4linux/video0/dev_debug
```

## 阶段验收标准
- [ ] 能解释 V4L2 框架核心数据结构
- [ ] 理解 videobuf2 缓冲区管理机制
- [ ] 能分析现有传感器驱动
- [ ] 产出框架分析文档

## 文件结构
```
第五阶段_V4L2内核框架/
├── README.md
├── analysis/
│   ├── v4l2-core-analysis.md   # 核心框架分析
│   ├── videobuf2-analysis.md   # VB2分析
│   ├── mc-analysis.md          # Media Controller分析
│   └── ov5640-analysis.md      # 驱动分析
├── diagrams/                    # 流程图
└── docs/
```

---
进入下一阶段: [第六阶段_Camera驱动开发](../第六阶段_Camera驱动开发/README.md)

