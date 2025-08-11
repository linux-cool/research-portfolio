# TS 复用与分析模块

## 概述
Transport Stream (TS) 复用与分析是广播电视技术的核心，涵盖PSI/SI表解析、PCR抖动控制、码率整形等关键技术。

## 核心功能

### 1. PSI/SI 表分析
- **PAT (Program Association Table)**: 程序关联表，映射PID到PMT
- **PMT (Program Map Table)**: 程序映射表，定义流组成
- **SDT (Service Description Table)**: 服务描述表
- **EIT (Event Information Table)**: 事件信息表

### 2. PCR 抖动控制
- PCR (Program Clock Reference) 精度要求：±500ns
- 抖动检测与校正算法
- 缓冲区管理策略

### 3. 码率整形与统计复用
- CBR/VBR 码率控制
- 统计复用算法
- 带宽分配优化

## 开源工具

### TSDuck
```bash
# 安装
sudo apt-get install tsduck

# 基础分析
tsp -I file input.ts -P analyze -O drop

# PSI/SI 表提取
tsp -I file input.ts -P tables --pid 0x0000 -O file pat.ts
tsp -I file input.ts -P tables --pid 0x1000 -O file pmt.ts

# PCR 分析
tsp -I file input.ts -P pcrextract -O drop
```

### FFmpeg
```bash
# TS 流分析
ffprobe -v quiet -print_format json -show_format -show_streams input.ts

# TS 复用
ffmpeg -i video.mp4 -i audio.aac -c copy -f mpegts output.ts

# 实时流推送
ffmpeg -re -i input.mp4 -c copy -f mpegts udp://239.0.0.1:1234
```

## 示例代码

### Python TS 分析器
```python
#!/usr/bin/env python3
"""
TS Stream Analyzer
基于第一性原理：TS流必须包含完整的PSI/SI信息才能被正确解码
"""

import struct
import sys
from typing import Dict, List, Optional

class TSPacket:
    """TS包结构解析"""
    
    def __init__(self, data: bytes):
        if len(data) < 188:
            raise ValueError("Invalid TS packet size")
        
        # TS header (4 bytes)
        self.sync_byte = data[0]
        self.transport_error_indicator = (data[1] & 0x80) >> 7
        self.payload_unit_start_indicator = (data[1] & 0x40) >> 6
        self.transport_priority = (data[1] & 0x20) >> 5
        self.pid = ((data[1] & 0x1F) << 8) | data[2]
        self.transport_scrambling_control = (data[3] & 0xC0) >> 6
        self.adaptation_field_control = (data[3] & 0x30) >> 4
        self.continuity_counter = data[3] & 0x0F
        
        # Payload
        self.payload = data[4:] if self.adaptation_field_control & 0x01 else b''

class TSAnalyzer:
    """TS流分析器"""
    
    def __init__(self):
        self.pat_data = {}
        self.pmt_data = {}
        self.pcr_values = []
        self.packet_count = 0
        self.error_count = 0
        
    def analyze_file(self, filename: str) -> Dict:
        """分析TS文件"""
        with open(filename, 'rb') as f:
            while True:
                packet_data = f.read(188)
                if len(packet_data) < 188:
                    break
                
                try:
                    packet = TSPacket(packet_data)
                    self._process_packet(packet)
                    self.packet_count += 1
                except Exception as e:
                    self.error_count += 1
                    print(f"Error processing packet {self.packet_count}: {e}")
        
        return self._generate_report()
    
    def _process_packet(self, packet: TSPacket):
        """处理单个TS包"""
        # PAT (PID 0x0000)
        if packet.pid == 0x0000 and packet.payload:
            self._parse_pat(packet.payload)
        
        # PMT (从PAT中获取的PID)
        elif packet.pid in self.pat_data.values() and packet.payload:
            self._parse_pmt(packet.payload)
        
        # PCR 提取
        if packet.adaptation_field_control & 0x02:
            self._extract_pcr(packet)
    
    def _parse_pat(self, payload: bytes):
        """解析PAT表"""
        # 简化PAT解析，实际应处理完整PSI语法
        if len(payload) >= 8:
            table_id = payload[0]
            if table_id == 0x00:  # PAT
                # 提取program_number和pid
                offset = 8
                while offset < len(payload) - 4:
                    program_number = struct.unpack('>H', payload[offset:offset+2])[0]
                    pid = struct.unpack('>H', payload[offset+2:offset+4])[0] & 0x1FFF
                    self.pat_data[program_number] = pid
                    offset += 4
    
    def _parse_pmt(self, payload: bytes):
        """解析PMT表"""
        # 简化PMT解析
        if len(payload) >= 8:
            table_id = payload[0]
            if table_id == 0x02:  # PMT
                pcr_pid = struct.unpack('>H', payload[8:10])[0] & 0x1FFF
                self.pmt_data['pcr_pid'] = pcr_pid
    
    def _extract_pcr(self, packet: TSPacket):
        """提取PCR值"""
        # 简化PCR提取，实际应解析adaptation field
        if len(packet.payload) >= 6:
            # 假设PCR在adaptation field中
            pcr_base = struct.unpack('>Q', b'\x00\x00' + packet.payload[:6])[0] >> 15
            self.pcr_values.append(pcr_base)
    
    def _generate_report(self) -> Dict:
        """生成分析报告"""
        return {
            'total_packets': self.packet_count,
            'error_packets': self.error_count,
            'error_rate': self.error_count / self.packet_count if self.packet_count > 0 else 0,
            'pat_programs': len(self.pat_data),
            'pmt_info': self.pmt_data,
            'pcr_samples': len(self.pcr_values),
            'pcr_jitter': self._calculate_pcr_jitter()
        }
    
    def _calculate_pcr_jitter(self) -> Optional[float]:
        """计算PCR抖动"""
        if len(self.pcr_values) < 2:
            return None
        
        # 计算相邻PCR值的差异
        diffs = []
        for i in range(1, len(self.pcr_values)):
            diff = abs(self.pcr_values[i] - self.pcr_values[i-1])
            diffs.append(diff)
        
        # 计算标准差作为抖动指标
        mean_diff = sum(diffs) / len(diffs)
        variance = sum((d - mean_diff) ** 2 for d in diffs) / len(diffs)
        return (variance ** 0.5) / 90000.0  # 转换为秒

def main():
    """主函数"""
    if len(sys.argv) != 2:
        print("Usage: python ts_analyzer.py <ts_file>")
        sys.exit(1)
    
    analyzer = TSAnalyzer()
    try:
        report = analyzer.analyze_file(sys.argv[1])
        print("TS Analysis Report:")
        print(f"Total packets: {report['total_packets']}")
        print(f"Error rate: {report['error_rate']:.4f}")
        print(f"Programs found: {report['pat_programs']}")
        print(f"PCR samples: {report['pcr_samples']}")
        if report['pcr_jitter']:
            print(f"PCR jitter: {report['pcr_jitter']:.6f} seconds")
    except Exception as e:
        print(f"Analysis failed: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
```

### Shell 脚本工具
```bash
#!/bin/bash
# TS 流质量检测脚本

# 配置参数
TS_FILE="${1:-input.ts}"
REPORT_FILE="ts_quality_report.txt"
PCR_THRESHOLD=500  # ns
CC_THRESHOLD=1

echo "=== TS Stream Quality Analysis ===" > "$REPORT_FILE"
echo "File: $TS_FILE" >> "$REPORT_FILE"
echo "Analysis time: $(date)" >> "$REPORT_FILE"
echo "" >> "$REPORT_FILE"

# 1. 基础信息
echo "1. Basic Information:" >> "$REPORT_FILE"
if command -v tsduck &> /dev/null; then
    echo "Using TSDuck for analysis..." >> "$REPORT_FILE"
    tsp -I file "$TS_FILE" -P analyze 2>&1 | head -20 >> "$REPORT_FILE"
else
    echo "TSDuck not found, using FFmpeg..." >> "$REPORT_FILE"
    ffprobe -v quiet -print_format json -show_format -show_streams "$TS_FILE" 2>&1 | head -20 >> "$REPORT_FILE"
fi

# 2. PCR 抖动检测
echo "" >> "$REPORT_FILE"
echo "2. PCR Jitter Analysis:" >> "$REPORT_FILE"
if command -v tsduck &> /dev/null; then
    tsp -I file "$TS_FILE" -P pcrextract 2>&1 | grep -E "(jitter|PCR)" >> "$REPORT_FILE"
fi

# 3. 连续性计数检查
echo "" >> "$REPORT_FILE"
echo "3. Continuity Counter Check:" >> "$REPORT_FILE"
if command -v tsduck &> /dev/null; then
    tsp -I file "$TS_FILE" -P continuity 2>&1 | head -10 >> "$REPORT_FILE"
fi

# 4. PSI/SI 完整性
echo "" >> "$REPORT_FILE"
echo "4. PSI/SI Integrity:" >> "$REPORT_FILE"
if command -v tsduck &> /dev/null; then
    echo "PAT table:" >> "$REPORT_FILE"
    tsp -I file "$TS_FILE" -P tables --pid 0x0000 2>&1 | head -5 >> "$REPORT_FILE"
    
    echo "PMT table:" >> "$REPORT_FILE"
    tsp -I file "$TS_FILE" -P tables --pid 0x1000 2>&1 | head -5 >> "$REPORT_FILE"
fi

echo "" >> "$REPORT_FILE"
echo "Analysis complete. Check $REPORT_FILE for results."
```

## 配置文件

### TSDuck 配置文件
```xml
<!-- ts_analysis.xml -->
<tsduck>
  <input>
    <file name="input.ts"/>
  </input>
  
  <plugins>
    <!-- PSI/SI 分析 -->
    <analyze/>
    
    <!-- PCR 提取 -->
    <pcrextract/>
    
    <!-- 连续性计数检查 -->
    <continuity/>
    
    <!-- 表提取 -->
    <tables pid="0x0000" output="pat.xml"/>
    <tables pid="0x1000" output="pmt.xml"/>
  </plugins>
  
  <output>
    <drop/>
  </output>
</tsduck>
```

## 验证标准 (DoD)

### 功能验证
- [ ] TS包同步字节检测 (0x47)
- [ ] PAT/PMT表完整解析
- [ ] PCR抖动计算 (<500ns)
- [ ] 连续性计数错误检测

### 性能验证
- [ ] 处理速度 > 10MB/s
- [ ] 内存占用 < 100MB
- [ ] 错误检测准确率 > 95%

### 第一性原理验证
- [ ] PCR值与系统时钟同步性验证
- [ ] PSI/SI表完整性对解码成功的影响验证

## 相关开源项目

1. **TSDuck** - 专业的TS流处理工具
   - GitHub: https://github.com/tsduck/tsduck
   - 特点：完整的PSI/SI解析、PCR分析、表操作

2. **FFmpeg** - 多媒体处理框架
   - GitHub: https://github.com/FFmpeg/FFmpeg
   - 特点：TS复用、编码、流媒体

3. **GStreamer** - 多媒体框架
   - GitHub: https://gitlab.freedesktop.org/gstreamer/gstreamer
   - 特点：插件化TS处理

4. **VLC** - 开源媒体播放器
   - GitHub: https://github.com/videolan/vlc
   - 特点：TS流播放与调试

## 技术趋势

- **AI辅助分析**: 基于机器学习的异常检测
- **云原生**: 容器化部署与微服务架构
- **实时处理**: 流式分析与告警
- **标准化**: 与SMPTE 2110等新标准集成