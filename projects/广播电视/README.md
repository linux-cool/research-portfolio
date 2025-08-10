# 广播电视技术研究

## 研究领域概述
本目录关注传统广播电视到新媒体融合的技术路径，涵盖采集/制作/播出/传输/监测全链路：MPEG-2 TS、DVB/DTMB/ATSC、SCTE-35/104、字幕与闭合字幕、响度标准、色彩与HDR、超低延时链路等。

## 主要研究方向
- 复用与传输：PMT/PAT/SDT/PSI/SI、PCR 抖动、码率整形与统计复用
- 标准与信令：SCTE-35 广告插入、SCTE-104、EIT/EPG、CAPTION/TTML/ARIB
- 画质与色彩：HDR10/HLG、WCG、色域映射、反交错/场序处理
- 响度与音频：EBU R128、ATSC A/85、AC-3/E-AC-3、音量一致性
- 传输形态：卫星/有线/地面/OTT 混合分发与回传监测

## 技术特点
- 基于 FFmpeg/tsduck 的 TS 复用/分析与探针
- 广告标记与切换流程验证（SCTE-35 → 播出 → OTT/HLS 映射）
- 低延时直播（LL-HLS/Chunked-Transfer）与专业编码器链路对照
- 监测告警：码率、PCR、丢包、PSI/SI 完整性、字幕/响度合规

## 快速开始
```bash
# TS 分析（使用 tsduck）
tsp -I file input.ts -P analyze -O drop

# SCTE-35 注入示例（示意）
ffmpeg -re -i input.mp4 -stream_loop -1 -c:v libx264 -c:a aac \
  -scte35 "<splice_info_section ...>" -f mpegts udp://239.0.0.1:1234

# HLS（含字幕）
ffmpeg -i input.ts -c copy -hls_flags independent_segments+program_date_time \
  -hls_time 2 -hls_segment_type mpegts out.m3u8
```

## 合规与测试
- 响度：ITU-R BS.1770, EBU R128，目标 -23 LUFS（欧洲）/ -24 LKFS（北美）
- 字幕：CEA-608/708、DVB-Sub、TTML/IMSC，字符集与定位验证
- 传输：PCR 抖动<500ns、连续性计数(CC)错误监测、PAT/PMT 完整性

## 文件结构
```
广播电视/
├── README.md        # 本文件
├── ts/              # TS 复用/分析脚本与配置
├── scte/            # SCTE-35/104 信令研究与样例
├── loudness/        # 响度检测与校正
├── captions/        # 字幕/闭合字幕工作流
└── reports/         # 合规测试与报告
```

## 验证标准（DoD）
- 基线 TS 流可通过 PSI/SI 完整性与 PCR 抖动阈值检查
- HLS/OTT 侧正确映射广告标记与字幕轨道
- 响度检测/校正流程在样例素材上可复现并达成目标响度
- 至少一项第一性假设验证：合规指标与用户可感知质量/合规风险强相关

## 风险与回退
- 风险：跨标准互操作性差 → 策略：引入转码/转封装桥接与自动化验证
- 风险：直播链路延迟波动 → 策略：冗余链路与缓冲区上限保护，回退至常规 HLS
- 回退：广告信令异常时兜底为整段播出并发告警

## 参考资料
- DVB/ATSC/DTMB 标准文档与实施指南
- SCTE-35/104, EBU R128, ITU-R BS.1770
- TSDuck, FFmpeg 官方文档

## 联系方式
- 维护者：待定
- 邮箱：待定
- Issues：请在仓库 `Issues` 区提交

—
最后更新时间：2025-08
版本：v1.0.0
