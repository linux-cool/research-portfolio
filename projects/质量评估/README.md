# 质量评估研究

## 研究领域概述
本目录聚焦音视频质量评估（VQA/AAQ）方法论与工具链，涵盖主观与客观质量评价、参考/无参考指标体系、流媒体 QoE 指标建模与可视化。

## 主要研究方向
- 参考型指标：PSNR, SSIM, MS-SSIM, VMAF（含4K/低码率模型）
- 无参考指标：NIQE, BRISQUE, PIQE, NR-VQA/NR-AQA 深度模型
- 音频质量：PESQ, POLQA, STOI, ViSQOL
- 流媒体 QoE：起播时延、卡顿时长/次数、码率切换、时移、直播延迟
- 标注与主观实验：双刺激、单刺激、MOS 主观实验流程与统计

## 技术特点
- 指标计算统一封装：批处理、可并行、可扩展插件化
- 结果治理：一致的数据模式（JSON/Parquet），含溯源与元数据
- 可视化看板：对比实验、配对统计、分布与置信区间
- 与编码/传输链路打通：自动拉起转码与回放，端到端评估

## 快速开始
```bash
# 安装常用依赖（示例）
sudo apt-get update && sudo apt-get install -y ffmpeg python3-venv
python3 -m venv .venv && source .venv/bin/activate
pip install numpy scipy pandas matplotlib seaborn jupyter scikit-image
pip install vmaf # 如果使用 Python 绑定或直接用 ffmpeg vmaf filter
```

### 计算参考指标示例（VMAF）
```bash
# Use ffmpeg libvmaf filter (comments in English)
ffmpeg -i distorted.mp4 -i reference.mp4 \
  -lavfi "libvmaf=model_path=/usr/share/model/vmaf_v0.6.1.json:log_fmt=json:log_path=vmaf.json" \
  -f null -
```

### 计算无参考指标示例（NIQE/BRISQUE，Python）
```python
# Example only; actual implementation may vary (comments in English)
import glob, json
import numpy as np
from skimage import io
from skimage.metrics import peak_signal_noise_ratio as psnr

results = []
for path in glob.glob('samples/*.png'):
    img = io.imread(path)
    # TODO: compute NR metrics like NIQE/BRISQUE via available libs/models
    results.append({"path": path, "niqe": None, "brisque": None})

with open('nr_metrics.json', 'w') as f:
    json.dump(results, f, indent=2)
```

## 数据与评测
- 数据治理：统一目录结构、规范文件命名、校验哈希与采样率/分辨率元信息
- 基线用例：SDR 1080p/720p，HDR10 4K，语音/音乐不同声学场景
- 统计方法：配对t检验、Wilcoxon、分层分析（内容类型/运动强度）

## 性能与复现
- 基线耗时：1080p VMAF 单路约 1.0–1.5x 实时（CPU 8c 16G 环境）
- 并发能力：多进程/多机分布式执行（参考 `云原生` 目录 HPA）
- 复现脚本：`scripts/run_vmaf.sh`、`notebooks/analyze_qoe.ipynb`

## 文件结构
```
质量评估/
├── README.md            # 本文件
├── datasets/            # 数据集与抽样配置
├── metrics/             # 指标实现与封装
├── scripts/             # 评估与统计脚本
├── notebooks/           # 可视化与分析
└── reports/             # 评测报告与图表
```

## 验证标准（DoD）
- 指标计算链路可在样例数据上稳定运行并输出结构化结果
- 端到端评测（编码→传输→回放→评估）可复现且产出报告
- 指标间一致性与主观结果相关性>=既定阈值（例如 VMAF 与 MOS ρ≥0.8）
- 至少一项第一性假设验证：参考/无参考指标与用户感知强相关

## 风险与回退
- 风险：模型泛化不足 → 策略：引入跨域内容/多分辨率数据增强
- 风险：主观实验偏差 → 策略：随机化/平衡设计与统计显著性检验
- 回退：当 NR 模型不稳定时，回退采用参考型指标与规则基线

## 参考资料
- Netflix VMAF 文档与论文
- ITU-T P.1203/P.1204, PESQ/POLQA 标准
- NR-VQA/NR-AQA 相关公开数据集与论文

## 联系方式
- 维护者：待定
- 邮箱：待定
- Issues：请在仓库 `Issues` 区提交

—
最后更新时间：2025-08
版本：v1.0.0
