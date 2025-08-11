# SCTE-35/104 信令研究模块

## 概述
SCTE-35 (Splice Information Table) 和 SCTE-104 (Automation System to Compression System Communications) 是广播电视广告插入和自动化控制的核心标准。

## 核心概念

### SCTE-35 广告插入信令
- **Splice Insert**: 广告插入点标记
- **Time Signal**: 精确时间同步信号
- **Private Command**: 私有命令扩展
- **Segmentation Descriptor**: 内容分段描述

### SCTE-104 自动化控制
- **Program Start/Stop**: 节目开始/结束控制
- **Video Event**: 视频事件触发
- **Audio Event**: 音频事件触发
- **Data Event**: 数据事件触发

## 开源工具

### FFmpeg SCTE-35 支持
```bash
# 注入SCTE-35信令
ffmpeg -re -i input.mp4 -stream_loop -1 -c:v libx264 -c:a aac \
  -scte35 "<splice_info_section ...>" -f mpegts udp://239.0.0.1:1234

# 提取SCTE-35信令
ffmpeg -i input.ts -map 0:d:0 -c copy scte35.bin
```

### TSDuck SCTE 处理
```bash
# SCTE-35 表提取
tsp -I file input.ts -P tables --pid 0xFC2 -O file scte35.ts

# SCTE-35 注入
tsp -I file input.ts -P inject --pid 0xFC2 --inter-packet 1000 scte35.bin -O file output.ts
```

## 示例代码

### Python SCTE-35 解析器
```python
#!/usr/bin/env python3
"""
SCTE-35 信令解析器
基于第一性原理：广告插入必须精确同步到帧级别，确保无缝切换
"""

import struct
import binascii
from typing import Dict, List, Optional
from datetime import datetime, timedelta

class SCTE35Parser:
    """SCTE-35 信令解析器"""
    
    # SCTE-35 表ID
    TABLE_ID = 0xFC
    
    # 命令类型
    SPLICE_NULL = 0x00
    SPLICE_SCHEDULE = 0x04
    SPLICE_INSERT = 0x05
    TIME_SIGNAL = 0x06
    BANDWIDTH_RESERVATION = 0x07
    PRIVATE_COMMAND = 0xFF
    
    def __init__(self):
        self.splice_events = []
        self.time_signals = []
        
    def parse_ts_packet(self, packet_data: bytes) -> Optional[Dict]:
        """解析TS包中的SCTE-35信令"""
        if len(packet_data) < 188:
            return None
            
        # 检查PID是否为SCTE-35 (0xFC2)
        pid = ((packet_data[1] & 0x1F) << 8) | packet_data[2]
        if pid != 0xFC2:
            return None
            
        # 检查payload_unit_start_indicator
        if not (packet_data[1] & 0x40):
            return None
            
        # 提取payload
        adaptation_field_control = (packet_data[3] & 0x30) >> 4
        if adaptation_field_control == 0x01:  # 只有payload
            payload = packet_data[4:]
        elif adaptation_field_control == 0x03:  # 有adaptation field和payload
            adaptation_field_length = packet_data[4]
            payload = packet_data[5 + adaptation_field_length:]
        else:
            return None
            
        return self._parse_scte35_payload(payload)
    
    def _parse_scte35_payload(self, payload: bytes) -> Optional[Dict]:
        """解析SCTE-35 payload"""
        if len(payload) < 8:
            return None
            
        # 检查table_id
        table_id = payload[0]
        if table_id != self.TABLE_ID:
            return None
            
        # 解析section header
        section_syntax_indicator = (payload[1] & 0x80) >> 7
        private_indicator = (payload[1] & 0x40) >> 6
        section_length = ((payload[1] & 0x0F) << 8) | payload[2]
        table_id_extension = (payload[3] << 8) | payload[4]
        version_number = (payload[5] & 0x3E) >> 1
        current_next_indicator = payload[5] & 0x01
        section_number = payload[6]
        last_section_number = payload[7]
        
        # 解析command
        command_length = payload[8]
        command_type = payload[9]
        
        result = {
            'table_id': table_id,
            'section_length': section_length,
            'table_id_extension': table_id_extension,
            'version_number': version_number,
            'section_number': section_number,
            'last_section_number': last_section_number,
            'command_type': command_type,
            'command_length': command_length
        }
        
        if command_type == self.SPLICE_INSERT:
            result.update(self._parse_splice_insert(payload[10:10+command_length]))
        elif command_type == self.TIME_SIGNAL:
            result.update(self._parse_time_signal(payload[10:10+command_length]))
            
        return result
    
    def _parse_splice_insert(self, command_data: bytes) -> Dict:
        """解析Splice Insert命令"""
        if len(command_data) < 5:
            return {}
            
        splice_event_id = struct.unpack('>I', command_data[:4])[0]
        splice_event_cancel_indicator = (command_data[4] & 0x80) >> 7
        out_of_network_indicator = (command_data[4] & 0x40) >> 6
        program_splice_flag = (command_data[4] & 0x20) >> 5
        duration_flag = (command_data[4] & 0x10) >> 4
        splice_immediate_flag = (command_data[4] & 0x08) >> 3
        
        result = {
            'splice_event_id': splice_event_id,
            'splice_event_cancel_indicator': splice_event_cancel_indicator,
            'out_of_network_indicator': out_of_network_indicator,
            'program_splice_flag': program_splice_flag,
            'duration_flag': duration_flag,
            'splice_immediate_flag': splice_immediate_flag
        }
        
        # 解析时间信息
        if not splice_immediate_flag and len(command_data) >= 9:
            time_specified_flag = (command_data[5] & 0x80) >> 7
            if time_specified_flag:
                # 解析PTS时间
                pts_time = struct.unpack('>Q', b'\x00' + command_data[6:11])[0] >> 15
                result['pts_time'] = pts_time
                result['pts_time_seconds'] = pts_time / 90000.0  # 90kHz时钟
        
        # 解析持续时间
        if duration_flag and len(command_data) >= 14:
            duration = struct.unpack('>Q', b'\x00' + command_data[9:14])[0] >> 15
            result['duration'] = duration
            result['duration_seconds'] = duration / 90000.0
            
        return result
    
    def _parse_time_signal(self, command_data: bytes) -> Dict:
        """解析Time Signal命令"""
        if len(command_data) < 5:
            return {}
            
        time_specified_flag = (command_data[0] & 0x80) >> 7
        if time_specified_flag:
            pts_time = struct.unpack('>Q', b'\x00' + command_data[1:6])[0] >> 15
            return {
                'pts_time': pts_time,
                'pts_time_seconds': pts_time / 90000.0
            }
        return {}
    
    def generate_scte35_binary(self, splice_event: Dict) -> bytes:
        """生成SCTE-35二进制数据"""
        # 简化的SCTE-35生成，实际应遵循完整规范
        command_data = bytearray()
        
        if splice_event['command_type'] == self.SPLICE_INSERT:
            # Splice Insert命令
            command_data.extend(struct.pack('>I', splice_event.get('splice_event_id', 0)))
            
            # 命令选项
            options = 0
            if splice_event.get('splice_event_cancel_indicator'):
                options |= 0x80
            if splice_event.get('out_of_network_indicator'):
                options |= 0x40
            if splice_event.get('program_splice_flag'):
                options |= 0x20
            if splice_event.get('duration_flag'):
                options |= 0x10
            if splice_event.get('splice_immediate_flag'):
                options |= 0x08
            command_data.append(options)
            
            # 时间信息
            if not splice_event.get('splice_immediate_flag') and 'pts_time' in splice_event:
                command_data.append(0x80)  # time_specified_flag
                pts_time = int(splice_event['pts_time'] * 90000) << 15
                command_data.extend(struct.pack('>Q', pts_time)[1:6])
            
            # 持续时间
            if splice_event.get('duration_flag') and 'duration' in splice_event:
                duration = int(splice_event['duration'] * 90000) << 15
                command_data.extend(struct.pack('>Q', duration)[1:6])
        
        # 构建完整的section
        section_data = bytearray()
        section_data.append(self.TABLE_ID)  # table_id
        
        # section_syntax_indicator + private_indicator + section_length
        section_length = len(command_data) + 9  # 命令长度 + 固定字段
        section_data.append(0x80 | ((section_length >> 8) & 0x0F))  # 0x80 = section_syntax_indicator
        section_data.append(section_length & 0xFF)
        
        # table_id_extension (通常为0)
        section_data.extend(b'\x00\x00')
        
        # version_number + current_next_indicator
        section_data.append(0x01)  # version 0, current
        
        # section_number + last_section_number
        section_data.extend(b'\x00\x00')
        
        # command_length + command_type
        section_data.append(len(command_data))
        section_data.append(splice_event['command_type'])
        
        # command data
        section_data.extend(command_data)
        
        # CRC32 (简化，实际应计算)
        section_data.extend(b'\x00\x00\x00\x00')
        
        return bytes(section_data)

class SCTE35Injector:
    """SCTE-35 信令注入器"""
    
    def __init__(self):
        self.parser = SCTE35Parser()
        
    def inject_splice_event(self, input_file: str, output_file: str, 
                          splice_time: float, duration: float = 30.0) -> bool:
        """注入广告插入事件"""
        try:
            # 创建SCTE-35事件
            splice_event = {
                'command_type': self.parser.SPLICE_INSERT,
                'splice_event_id': 12345,
                'splice_event_cancel_indicator': False,
                'out_of_network_indicator': True,
                'program_splice_flag': True,
                'duration_flag': True,
                'splice_immediate_flag': False,
                'pts_time': splice_time,
                'duration': duration
            }
            
            # 生成SCTE-35数据
            scte35_data = self.parser.generate_scte35_binary(splice_event)
            
            # 使用FFmpeg注入
            import subprocess
            cmd = [
                'ffmpeg', '-i', input_file,
                '-scte35', scte35_data.hex(),
                '-c', 'copy',
                '-f', 'mpegts',
                output_file
            ]
            
            result = subprocess.run(cmd, capture_output=True, text=True)
            return result.returncode == 0
            
        except Exception as e:
            print(f"Error injecting SCTE-35: {e}")
            return False

def main():
    """主函数"""
    import sys
    
    if len(sys.argv) < 2:
        print("Usage: python scte35_parser.py <ts_file>")
        sys.exit(1)
    
    parser = SCTE35Parser()
    
    # 解析TS文件
    with open(sys.argv[1], 'rb') as f:
        packet_count = 0
        scte35_count = 0
        
        while True:
            packet_data = f.read(188)
            if len(packet_data) < 188:
                break
                
            result = parser.parse_ts_packet(packet_data)
            if result:
                scte35_count += 1
                print(f"SCTE-35 Event #{scte35_count}:")
                print(f"  Command Type: {result['command_type']}")
                if 'pts_time_seconds' in result:
                    print(f"  PTS Time: {result['pts_time_seconds']:.3f}s")
                if 'duration_seconds' in result:
                    print(f"  Duration: {result['duration_seconds']:.3f}s")
                print()
                
            packet_count += 1
            if packet_count % 10000 == 0:
                print(f"Processed {packet_count} packets...")
    
    print(f"Total packets: {packet_count}")
    print(f"SCTE-35 events: {scte35_count}")

if __name__ == "__main__":
    main()
```

### SCTE-35 测试脚本
```bash
#!/bin/bash
# SCTE-35 信令测试脚本

# 配置
INPUT_TS="input.ts"
OUTPUT_TS="output_with_scte35.ts"
SCTE35_DATA="scte35_data.bin"
INJECT_TIME=30.0  # 秒

echo "=== SCTE-35 Signal Injection Test ==="

# 1. 检查输入文件
if [ ! -f "$INPUT_TS" ]; then
    echo "Error: Input file $INPUT_TS not found"
    exit 1
fi

# 2. 生成测试SCTE-35数据
echo "Generating test SCTE-35 data..."
python3 -c "
import struct
# 简化的SCTE-35 Splice Insert
data = bytearray([
    0xFC,  # table_id
    0x80, 0x0F,  # section_syntax + length
    0x00, 0x00,  # table_id_extension
    0x01,  # version + current
    0x00, 0x00,  # section numbers
    0x0A,  # command length
    0x05,  # splice_insert
    0x00, 0x00, 0x30, 0x39,  # splice_event_id
    0x60,  # options
    0x80,  # time_specified_flag
    0x00, 0x00, 0x00, 0x00, 0x00,  # PTS time
    0x00, 0x00, 0x00, 0x00, 0x00   # CRC placeholder
])
with open('$SCTE35_DATA', 'wb') as f:
    f.write(data)
"

# 3. 注入SCTE-35信令
echo "Injecting SCTE-35 signal..."
ffmpeg -i "$INPUT_TS" \
    -scte35 "$(cat $SCTE35_DATA | xxd -p)" \
    -c copy \
    -f mpegts \
    "$OUTPUT_TS"

# 4. 验证注入结果
echo "Verifying injection..."
if command -v tsduck &> /dev/null; then
    echo "SCTE-35 tables found:"
    tsp -I file "$OUTPUT_TS" -P tables --pid 0xFC2 2>&1 | head -10
else
    echo "TSDuck not available, using FFmpeg..."
    ffprobe -v quiet -print_format json -show_streams "$OUTPUT_TS" | grep -i scte
fi

echo "Test completed. Output file: $OUTPUT_TS"
```

## 配置文件

### FFmpeg SCTE-35 配置
```bash
# ffmpeg_scte35.conf
# SCTE-35 注入配置

# 输入流配置
[input]
format = mpegts
protocol = udp
address = 239.0.0.1
port = 1234

# SCTE-35 配置
[scte35]
pid = 0xFC2
table_id = 0xFC
injection_interval = 1000  # 包间隔

# 广告插入点配置
[splice_points]
default_duration = 30.0
out_of_network = true
immediate_splice = false
```

## 验证标准 (DoD)

### 功能验证
- [ ] SCTE-35信令正确解析
- [ ] 广告插入时间点精确到帧
- [ ] 信令注入不影响视频质量
- [ ] 多语言支持验证

### 性能验证
- [ ] 信令延迟 < 100ms
- [ ] 注入成功率 > 99%
- [ ] 内存占用 < 50MB

### 第一性原理验证
- [ ] 时间同步精度验证（90kHz时钟）
- [ ] 信令完整性对解码成功的影响验证

## 相关开源项目

1. **FFmpeg** - 多媒体处理框架
   - SCTE-35支持：https://ffmpeg.org/documentation.html
   - 特点：完整的信令注入与提取

2. **TSDuck** - TS流处理工具
   - SCTE处理：https://tsduck.io/
   - 特点：专业的SCTE-35/104支持

3. **GStreamer** - 多媒体框架
   - 插件：https://gstreamer.freedesktop.org/
   - 特点：模块化SCTE处理

4. **VLC** - 媒体播放器
   - 信令支持：https://www.videolan.org/
   - 特点：实时信令显示

## 技术趋势

- **AI广告插入**: 基于内容的智能广告识别
- **云原生信令**: 微服务架构的信令处理
- **实时分析**: 信令效果实时监控
- **标准化**: 与ATSC 3.0、DVB等新标准集成