# 响度检测与校正模块

## 概述
响度标准化是广播电视音频质量的核心要求，确保不同节目间的音量一致性。本模块基于EBU R128、ATSC A/85等标准实现响度检测与校正。

## 核心标准

### EBU R128 (欧洲广播联盟)
- **目标响度**: -23 LUFS (Loudness Units relative to Full Scale)
- **真峰值**: -1.0 dBTP
- **响度范围**: 20 LU
- **测量方法**: ITU-R BS.1770-4

### ATSC A/85 (北美)
- **目标响度**: -24 LKFS (Loudness, K-weighted, relative to Full Scale)
- **真峰值**: -2.0 dBTP
- **测量方法**: ITU-R BS.1770-4

### ITU-R BS.1770-4
- **K-weighting**: 人耳频率响应模拟
- **门限**: -70 LUFS
- **积分时间**: 400ms

## 开源工具

### FFmpeg 响度检测
```bash
# 响度检测 (EBU R128)
ffmpeg -i input.wav -af loudnorm=I=-23:TP=-1:LRA=20 -f null -

# 响度校正
ffmpeg -i input.wav -af loudnorm=I=-23:TP=-1:LRA=20:measured_I=-18:measured_LRA=15:measured_TP=-0.5:measured_thresh=-30:offset=0:linear=true -ar 48000 output.wav

# 真峰值检测
ffmpeg -i input.wav -af volumedetect -f null -
```

### SoX 音频处理
```bash
# 响度检测
sox input.wav -n stat -v 2>&1 | grep "RMS amplitude"

# 音量标准化
sox input.wav output.wav norm -3
```

### Python 响度库
```bash
# 安装 pyloudnorm
pip install pyloudnorm

# 安装 pydub
pip install pydub
```

## 示例代码

### Python 响度检测器
```python
#!/usr/bin/env python3
"""
响度检测与校正器
基于第一性原理：人耳对响度的感知是非线性的，需要K-weighting和门限处理
"""

import numpy as np
import librosa
import pyloudnorm as pyln
from typing import Dict, Tuple, Optional
import json
import argparse

class LoudnessAnalyzer:
    """响度分析器"""
    
    def __init__(self, sample_rate: int = 48000):
        self.sample_rate = sample_rate
        self.meter = pyln.Meter(sample_rate)
        
    def analyze_file(self, audio_file: str) -> Dict:
        """分析音频文件响度"""
        try:
            # 加载音频
            y, sr = librosa.load(audio_file, sr=self.sample_rate)
            
            # 转换为float32
            y = y.astype(np.float32)
            
            # 响度测量
            loudness = self.meter.integrated_loudness(y)
            peak = self.meter.true_peak(y)
            lra = self.meter.loudness_range(y)
            
            # 统计信息
            rms = np.sqrt(np.mean(y**2))
            dynamic_range = np.max(y) - np.min(y)
            
            return {
                'file': audio_file,
                'sample_rate': sr,
                'duration': len(y) / sr,
                'loudness': {
                    'integrated': loudness,
                    'peak': peak,
                    'lra': lra,
                    'rms': rms,
                    'dynamic_range': dynamic_range
                },
                'compliance': self._check_compliance(loudness, peak, lra)
            }
            
        except Exception as e:
            return {'error': str(e)}
    
    def _check_compliance(self, loudness: float, peak: float, lra: float) -> Dict:
        """检查合规性"""
        # EBU R128 标准
        ebu_r128 = {
            'target_loudness': -23.0,
            'target_peak': -1.0,
            'target_lra': 20.0,
            'loudness_tolerance': 1.0,
            'peak_tolerance': 0.5
        }
        
        # ATSC A/85 标准
        atsc_a85 = {
            'target_loudness': -24.0,
            'target_peak': -2.0,
            'target_lra': 20.0,
            'loudness_tolerance': 1.0,
            'peak_tolerance': 0.5
        }
        
        # 检查EBU R128
        ebu_compliant = (
            abs(loudness - ebu_r128['target_loudness']) <= ebu_r128['loudness_tolerance'] and
            peak <= ebu_r128['target_peak'] + ebu_r128['peak_tolerance'] and
            lra <= ebu_r128['target_lra']
        )
        
        # 检查ATSC A/85
        atsc_compliant = (
            abs(loudness - atsc_a85['target_loudness']) <= atsc_a85['loudness_tolerance'] and
            peak <= atsc_a85['target_peak'] + atsc_a85['peak_tolerance'] and
            lra <= atsc_a85['target_lra']
        )
        
        return {
            'ebu_r128': {
                'compliant': ebu_compliant,
                'target': ebu_r128['target_loudness'],
                'actual': loudness,
                'difference': loudness - ebu_r128['target_loudness']
            },
            'atsc_a85': {
                'compliant': atsc_compliant,
                'target': atsc_a85['target_loudness'],
                'actual': loudness,
                'difference': loudness - atsc_a85['target_loudness']
            }
        }

class LoudnessCorrector:
    """响度校正器"""
    
    def __init__(self, sample_rate: int = 48000):
        self.sample_rate = sample_rate
        self.analyzer = LoudnessAnalyzer(sample_rate)
        
    def correct_loudness(self, input_file: str, output_file: str, 
                        target_loudness: float = -23.0, target_peak: float = -1.0,
                        target_lra: float = 20.0) -> Dict:
        """校正响度"""
        try:
            # 分析输入文件
            analysis = self.analyzer.analyze_file(input_file)
            if 'error' in analysis:
                return analysis
            
            # 计算校正参数
            current_loudness = analysis['loudness']['integrated']
            current_peak = analysis['loudness']['peak']
            current_lra = analysis['loudness']['lra']
            
            # 响度增益
            loudness_gain = target_loudness - current_loudness
            
            # 峰值限制
            peak_gain = min(loudness_gain, target_peak - current_peak)
            
            # 应用校正
            import subprocess
            cmd = [
                'ffmpeg', '-i', input_file,
                '-af', f'loudnorm=I={target_loudness}:TP={target_peak}:LRA={target_lra}:'
                       f'measured_I={current_loudness}:measured_LRA={current_lra}:'
                       f'measured_TP={current_peak}:measured_thresh=-30:'
                       f'offset={loudness_gain}:linear=true',
                '-ar', str(self.sample_rate),
                '-y', output_file
            ]
            
            result = subprocess.run(cmd, capture_output=True, text=True)
            
            if result.returncode == 0:
                # 验证校正结果
                corrected_analysis = self.analyzer.analyze_file(output_file)
                return {
                    'success': True,
                    'input_analysis': analysis,
                    'output_analysis': corrected_analysis,
                    'correction_applied': {
                        'loudness_gain': loudness_gain,
                        'peak_gain': peak_gain
                    }
                }
            else:
                return {
                    'success': False,
                    'error': result.stderr
                }
                
        except Exception as e:
            return {'error': str(e)}

class BatchLoudnessProcessor:
    """批量响度处理器"""
    
    def __init__(self, sample_rate: int = 48000):
        self.analyzer = LoudnessAnalyzer(sample_rate)
        self.corrector = LoudnessCorrector(sample_rate)
        
    def process_directory(self, input_dir: str, output_dir: str, 
                         target_loudness: float = -23.0) -> Dict:
        """处理目录中的所有音频文件"""
        import os
        import glob
        
        # 支持的音频格式
        audio_extensions = ['*.wav', '*.mp3', '*.aac', '*.flac', '*.m4a']
        
        results = {
            'total_files': 0,
            'processed_files': 0,
            'errors': [],
            'summary': {}
        }
        
        # 创建输出目录
        os.makedirs(output_dir, exist_ok=True)
        
        # 处理所有音频文件
        for ext in audio_extensions:
            pattern = os.path.join(input_dir, ext)
            for audio_file in glob.glob(pattern):
                results['total_files'] += 1
                
                try:
                    # 生成输出文件名
                    filename = os.path.basename(audio_file)
                    name, ext = os.path.splitext(filename)
                    output_file = os.path.join(output_dir, f"{name}_corrected{ext}")
                    
                    # 校正响度
                    result = self.corrector.correct_loudness(
                        audio_file, output_file, target_loudness
                    )
                    
                    if result.get('success'):
                        results['processed_files'] += 1
                        
                        # 统计信息
                        if 'summary' not in results:
                            results['summary'] = {
                                'loudness_before': [],
                                'loudness_after': [],
                                'peak_before': [],
                                'peak_after': []
                            }
                        
                        input_analysis = result['input_analysis']
                        output_analysis = result['output_analysis']
                        
                        results['summary']['loudness_before'].append(
                            input_analysis['loudness']['integrated']
                        )
                        results['summary']['loudness_after'].append(
                            output_analysis['loudness']['integrated']
                        )
                        results['summary']['peak_before'].append(
                            input_analysis['loudness']['peak']
                        )
                        results['summary']['peak_after'].append(
                            output_analysis['loudness']['peak']
                        )
                        
                    else:
                        results['errors'].append({
                            'file': audio_file,
                            'error': result.get('error', 'Unknown error')
                        })
                        
                except Exception as e:
                    results['errors'].append({
                        'file': audio_file,
                        'error': str(e)
                    })
        
        # 计算统计摘要
        if results['summary']:
            for key in results['summary']:
                if results['summary'][key]:
                    values = results['summary'][key]
                    results['summary'][f'{key}_stats'] = {
                        'mean': np.mean(values),
                        'std': np.std(values),
                        'min': np.min(values),
                        'max': np.max(values)
                    }
        
        return results

def main():
    """主函数"""
    parser = argparse.ArgumentParser(description='响度检测与校正工具')
    parser.add_argument('input', help='输入音频文件或目录')
    parser.add_argument('output', help='输出文件或目录')
    parser.add_argument('--target-loudness', type=float, default=-23.0,
                       help='目标响度 (默认: -23 LUFS)')
    parser.add_argument('--target-peak', type=float, default=-1.0,
                       help='目标峰值 (默认: -1 dBTP)')
    parser.add_argument('--analyze-only', action='store_true',
                       help='仅分析，不校正')
    
    args = parser.parse_args()
    
    if args.analyze_only:
        # 仅分析模式
        analyzer = LoudnessAnalyzer()
        if os.path.isfile(args.input):
            result = analyzer.analyze_file(args.input)
            print(json.dumps(result, indent=2))
        else:
            print("分析模式需要指定单个文件")
    else:
        # 校正模式
        if os.path.isfile(args.input):
            # 单个文件
            corrector = LoudnessCorrector()
            result = corrector.correct_loudness(
                args.input, args.output, 
                args.target_loudness, args.target_peak
            )
            print(json.dumps(result, indent=2))
        else:
            # 批量处理
            processor = BatchLoudnessProcessor()
            result = processor.process_directory(
                args.input, args.output, args.target_loudness
            )
            print(json.dumps(result, indent=2))

if __name__ == "__main__":
    import os
    main()
```

### 响度检测脚本
```bash
#!/bin/bash
# 响度检测与校正脚本

# 配置
INPUT_DIR="input_audio"
OUTPUT_DIR="output_audio"
TARGET_LOUDNESS="-23"  # LUFS
TARGET_PEAK="-1"       # dBTP
REPORT_FILE="loudness_report.json"

echo "=== Loudness Analysis and Correction ==="

# 1. 检查依赖
check_dependencies() {
    echo "Checking dependencies..."
    
    if ! command -v ffmpeg &> /dev/null; then
        echo "Error: FFmpeg not found"
        exit 1
    fi
    
    if ! command -v python3 &> /dev/null; then
        echo "Error: Python3 not found"
        exit 1
    fi
    
    echo "Dependencies OK"
}

# 2. 批量响度检测
analyze_loudness() {
    echo "Analyzing loudness for all audio files..."
    
    python3 -c "
import os
import glob
import json
from loudness_analyzer import LoudnessAnalyzer

analyzer = LoudnessAnalyzer()
results = []

# 支持的音频格式
audio_extensions = ['*.wav', '*.mp3', '*.aac', '*.flac', '*.m4a']

for ext in audio_extensions:
    pattern = os.path.join('$INPUT_DIR', ext)
    for audio_file in glob.glob(pattern):
        result = analyzer.analyze_file(audio_file)
        results.append(result)

# 保存结果
with open('$REPORT_FILE', 'w') as f:
    json.dump(results, f, indent=2)

print(f'Analysis complete. {len(results)} files processed.')
"
}

# 3. 响度校正
correct_loudness() {
    echo "Correcting loudness..."
    
    # 创建输出目录
    mkdir -p "$OUTPUT_DIR"
    
    # 使用Python脚本进行校正
    python3 -c "
from loudness_corrector import BatchLoudnessProcessor

processor = BatchLoudnessProcessor()
result = processor.process_directory(
    '$INPUT_DIR', 
    '$OUTPUT_DIR', 
    $TARGET_LOUDNESS
)

print('Correction complete.')
print(f'Processed: {result[\"processed_files\"]}/{result[\"total_files\"]} files')
if result['errors']:
    print(f'Errors: {len(result[\"errors\"])}')
"
}

# 4. 生成报告
generate_report() {
    echo "Generating report..."
    
    if [ -f "$REPORT_FILE" ]; then
        echo "Loudness Analysis Report:"
        echo "========================"
        
        # 使用jq解析JSON (如果可用)
        if command -v jq &> /dev/null; then
            jq -r '.[] | "\(.file): \(.loudness.integrated) LUFS"' "$REPORT_FILE"
        else
            # 简单的文本解析
            grep -E '"integrated":' "$REPORT_FILE" | head -10
        fi
    fi
}

# 主流程
main() {
    check_dependencies
    
    if [ ! -d "$INPUT_DIR" ]; then
        echo "Error: Input directory $INPUT_DIR not found"
        exit 1
    fi
    
    analyze_loudness
    correct_loudness
    generate_report
    
    echo "Process completed. Check $OUTPUT_DIR for corrected files."
}

main
```

## 配置文件

### FFmpeg 响度配置
```bash
# ffmpeg_loudness.conf
# 响度校正配置

# EBU R128 标准
[ebu_r128]
target_loudness = -23.0
target_peak = -1.0
target_lra = 20.0
measurement_threshold = -30.0

# ATSC A/85 标准
[atsc_a85]
target_loudness = -24.0
target_peak = -2.0
target_lra = 20.0
measurement_threshold = -30.0

# 处理参数
[processing]
sample_rate = 48000
channels = 2
bit_depth = 16
```

## 验证标准 (DoD)

### 功能验证
- [ ] 响度测量精度 (±0.1 LUFS)
- [ ] 峰值检测精度 (±0.1 dBTP)
- [ ] 校正后响度符合目标值
- [ ] 音频质量无显著下降

### 性能验证
- [ ] 处理速度 > 实时 (1x)
- [ ] 内存占用 < 100MB
- [ ] 支持批量处理

### 第一性原理验证
- [ ] K-weighting滤波器响应验证
- [ ] 门限处理对测量精度的影响验证
- [ ] 响度与主观感知的相关性验证

## 相关开源项目

1. **FFmpeg** - 多媒体处理框架
   - loudnorm滤镜：https://ffmpeg.org/ffmpeg-filters.html#loudnorm
   - 特点：完整的响度检测与校正

2. **SoX** - 音频处理工具
   - 响度检测：http://sox.sourceforge.net/
   - 特点：轻量级音频处理

3. **pyloudnorm** - Python响度库
   - GitHub: https://github.com/csteinmetz1/pyloudnorm
   - 特点：ITU-R BS.1770-4标准实现

4. **librosa** - 音频分析库
   - GitHub: https://github.com/librosa/librosa
   - 特点：音频特征提取

## 技术趋势

- **AI响度控制**: 基于内容的智能响度调整
- **实时处理**: 流媒体响度监控
- **云原生**: 分布式响度处理
- **标准化**: 与新音频标准集成 (如MPEG-H)