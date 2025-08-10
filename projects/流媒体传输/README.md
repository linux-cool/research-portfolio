# 流媒体传输研究

## 研究领域概述
本文件夹包含基于FFmpeg的流媒体传输协议研究内容，涵盖RTMP、HLS、DASH等协议的优化研究，支持大规模并发流媒体服务。

## 主要研究方向
- RTMP协议优化
- HLS分片策略改进（含 LL-HLS）
- DASH自适应码率传输与 ABR 策略
- 网络拥塞控制（BBR/Congestion Window 调优）
- 多CDN分发与回源容错策略

## 技术特点
- 网络传输效率优化与首帧时延优化
- 自适应码率调整算法（基于缓冲/吞吐/混合信号）
- 实时流媒体质量监控（QoS/QoE）与告警
- 多协议兼容性与服务端/边缘协同
- 负载均衡、健康检查与故障转移

## 快速开始
```bash
# 推流（RTMP 示例，comments in English)
ffmpeg -re -i input.mp4 -c:v libx264 -preset veryfast -b:v 3M -c:a aac -f flv rtmp://localhost/live/stream

# 生成 HLS（含低延时参数示意）
ffmpeg -re -i input.mp4 -c:v libx264 -c:a aac -f hls \
  -hls_time 2 -hls_flags independent_segments+program_date_time \
  -hls_playlist_type event -master_pl_name master.m3u8 out_%v.m3u8
```

## 相关项目
- FFmpeg流媒体传输协议研究与优化

## 文件结构
```
流媒体传输/
├── README.md          # 本文件
├── RTMP优化/          # RTMP协议优化代码
├── HLS优化/           # HLS协议优化实现
├── DASH实现/          # DASH协议实现
├── 网络优化/          # 网络传输优化
└── 文档/             # 研究文档和论文
```

## 验证标准（DoD）
- 基线推流/拉流在本地与云环境均可稳定运行
- HLS/DASH 自适应切换无明显卡顿，起播<2s（示例阈值）
- 监控指标（卡顿、时延、错误率）可采集并可视化
- 至少一项第一性假设验证：吞吐/缓冲驱动的 ABR 能显著提升 QoE

## 风险与回退
- 风险：跨 CDN 兼容性差 → 策略：对齐缓存/Range/Origin 行为与灰度
- 风险：低延时引入不稳定 → 策略：动态回退到常规 HLS/DASH 参数
- 回退：服务端异常时切换备用域名/回源路径并限流保护

## 联系方式
- 维护者：待定
- 邮箱：待定
- Issues：请在仓库 `Issues` 区提交

—
最后更新时间：2025-08
版本：v1.1.0
