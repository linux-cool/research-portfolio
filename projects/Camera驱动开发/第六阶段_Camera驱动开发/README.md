# 第六阶段：Camera 驱动开发

## 阶段概述
本阶段目标是能够开发和调试摄像头传感器驱动，掌握 I2C 通信、GPIO/Regulator 控制、MIPI CSI-2 协议等关键技术。

**预计时间**: 6-8 周

## 学习目标
- [ ] 掌握 I2C 子系统和 regmap 框架
- [ ] 掌握 GPIO/Regulator/Clock 控制
- [ ] 理解 MIPI CSI-2 协议
- [ ] 能独立移植传感器驱动
- [ ] 能调试常见驱动问题

## 每周学习计划

### 第1-2周：I2C 子系统与 regmap

#### I2C 子系统
```c
/* I2C 客户端驱动 */
static int sensor_probe(struct i2c_client *client)
{
    /* 通过 I2C 读写寄存器 */
}

static const struct i2c_device_id sensor_id[] = {
    { "sensor-name", 0 },
    { }
};

static struct i2c_driver sensor_driver = {
    .driver = {
        .name = "sensor-name",
        .of_match_table = sensor_of_match,
    },
    .probe = sensor_probe,
    .remove = sensor_remove,
    .id_table = sensor_id,
};
module_i2c_driver(sensor_driver);
```

#### regmap 框架
```c
/* regmap 配置 */
static const struct regmap_config sensor_regmap_config = {
    .reg_bits = 16,
    .val_bits = 8,
    .max_register = 0xFFFF,
};

/* 初始化 */
sensor->regmap = devm_regmap_init_i2c(client, &sensor_regmap_config);

/* 读写寄存器 */
regmap_read(sensor->regmap, REG_CHIP_ID, &val);
regmap_write(sensor->regmap, REG_MODE, 0x01);
regmap_update_bits(sensor->regmap, REG_CTRL, MASK, VAL);
```

---

### 第3周：GPIO / Regulator / Clock

#### GPIO 控制
```c
/* 获取 GPIO */
sensor->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
sensor->pwdn_gpio = devm_gpiod_get_optional(dev, "powerdown", GPIOD_OUT_HIGH);

/* 控制 GPIO */
gpiod_set_value_cansleep(sensor->reset_gpio, 0);  /* 释放复位 */
gpiod_set_value_cansleep(sensor->pwdn_gpio, 0);   /* 退出掉电 */
```

#### Regulator 控制
```c
/* 电源定义 */
static const char * const sensor_supply_names[] = {
    "AVDD",   /* 模拟电源 2.8V */
    "DOVDD",  /* IO电源 1.8V */
    "DVDD",   /* 数字核心电源 1.2V */
};

/* 获取电源 */
ret = devm_regulator_bulk_get(dev, ARRAY_SIZE(sensor_supply_names),
                               sensor->supplies);

/* 使能/禁用电源 */
regulator_bulk_enable(num, sensor->supplies);
regulator_bulk_disable(num, sensor->supplies);
```

#### Clock 控制
```c
/* 获取时钟 */
sensor->xclk = devm_clk_get(dev, "xclk");
clk_set_rate(sensor->xclk, 24000000);  /* 24MHz */

/* 使能/禁用时钟 */
clk_prepare_enable(sensor->xclk);
clk_disable_unprepare(sensor->xclk);
```

---

### 第4周：MIPI CSI-2 协议

#### MIPI CSI-2 基础
```
┌─────────────────────────────────────────────────────────┐
│                    MIPI CSI-2 架构                       │
├─────────────────────────────────────────────────────────┤
│  应用层: 图像传感器                                       │
│  ─────────────────────────────────────────────────────  │
│  协议层: CSI-2 协议 (短包/长包)                           │
│  ─────────────────────────────────────────────────────  │
│  物理层: D-PHY (差分信号, 1/2/4 Lane)                    │
└─────────────────────────────────────────────────────────┘
```

#### 设备树配置
```dts
sensor@3c {
    compatible = "ovti,ov5640";
    reg = <0x3c>;
    
    clocks = <&ccu CLK_CSI_MCLK>;
    clock-names = "xclk";
    
    port {
        sensor_ep: endpoint {
            remote-endpoint = <&csi_ep>;
            bus-type = <4>;         /* MIPI CSI-2 */
            clock-lanes = <0>;
            data-lanes = <1 2>;     /* 2 Lane */
            link-frequencies = /bits/ 64 <456000000>;
        };
    };
};
```

---

### 第5-6周：传感器驱动实现

#### 驱动框架
```c
struct sensor_priv {
    struct v4l2_subdev sd;
    struct media_pad pad;
    struct v4l2_ctrl_handler ctrl_handler;
    
    struct i2c_client *client;
    struct regmap *regmap;
    struct clk *xclk;
    struct gpio_desc *reset_gpio;
    struct gpio_desc *pwdn_gpio;
    struct regulator_bulk_data supplies[3];
    
    struct mutex lock;
    const struct sensor_mode *cur_mode;
    bool streaming;
};
```

#### 子设备操作实现
```c
/* Pad 操作 */
static const struct v4l2_subdev_pad_ops sensor_pad_ops = {
    .enum_mbus_code = sensor_enum_mbus_code,
    .enum_frame_size = sensor_enum_frame_size,
    .get_fmt = sensor_get_fmt,
    .set_fmt = sensor_set_fmt,
};

/* Video 操作 */
static const struct v4l2_subdev_video_ops sensor_video_ops = {
    .s_stream = sensor_s_stream,
};

/* 汇总 */
static const struct v4l2_subdev_ops sensor_ops = {
    .core = &sensor_core_ops,
    .video = &sensor_video_ops,
    .pad = &sensor_pad_ops,
};
```

---

### 第7-8周：调试与优化

#### 常见问题排查
| 问题 | 排查方法 |
|------|----------|
| I2C 通信失败 | i2cdetect, 检查地址/上拉 |
| 无图像输出 | 检查时钟/电源/复位时序 |
| 图像异常 | 检查格式/分辨率配置 |
| 帧率不对 | 检查 HTS/VTS 配置 |

#### 调试命令
```bash
# I2C 检测
i2cdetect -y 1
i2cdump -y 1 0x3c

# V4L2 调试
v4l2-ctl -d /dev/video0 --all
v4l2-ctl -d /dev/v4l-subdev0 --all

# 内核日志
dmesg | grep -i sensor
echo 8 > /proc/sys/kernel/printk
```

## 实践项目

### 项目: 移植传感器驱动
```
选择传感器 (GC2053/SC2336/OV2640)
     │
     ▼
获取数据手册
     │
     ▼
编写设备树
     │
     ▼
实现驱动框架
     │
     ▼
实现电源/时钟控制
     │
     ▼
实现 I2C 通信
     │
     ▼
实现格式/流控制
     │
     ▼
实现控制器
     │
     ▼
测试调试
```

## 阶段验收标准
- [ ] 能独立编写传感器驱动框架
- [ ] 能正确配置电源时序
- [ ] 能通过 I2C 读写传感器寄存器
- [ ] 能成功移植一款传感器

## 文件结构
```
第六阶段_Camera驱动开发/
├── README.md
├── src/
│   ├── example_sensor.c    # 示例驱动
│   └── example_sensor.dts  # 设备树
├── docs/
│   ├── i2c-guide.md
│   ├── mipi-csi2.md
│   └── debugging.md
└── datasheets/             # 数据手册
```

---
进入下一阶段: [第七阶段_综合项目实战](../第七阶段_综合项目实战/README.md)

