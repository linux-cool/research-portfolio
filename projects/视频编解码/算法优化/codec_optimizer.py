#!/usr/bin/env python3
"""
FFmpeg视频编解码优化器
基于第一性原理：算法选择 + 参数调优 = 质量效率平衡
"""

import subprocess
import json
import logging
import time
from typing import Dict, List, Optional, Tuple
from dataclasses import dataclass
import numpy as np
from pathlib import Path

@dataclass
class CodecConfig:
    """编解码器配置"""
    codec: str
    preset: str
    crf: int
    bitrate: Optional[str] = None
    profile: str = "main"
    level: str = "4.1"
    tune: Optional[str] = None
    additional_params: Dict[str, str] = None

class VideoCodecOptimizer:
    """视频编解码优化器"""
    
    def __init__(self):
        self.codec_profiles = {
            "h264": {
                "presets": ["ultrafast", "superfast", "veryfast", "faster", "fast", "medium", "slow", "slower", "veryslow"],
                "profiles": ["baseline", "main", "high", "high10", "high422", "high444"],
                "crf_range": (18, 28),
                "use_cases": {
                    "streaming": {"preset": "fast", "profile": "main", "crf": 23},
                    "archival": {"preset": "slow", "profile": "high", "crf": 18},
                    "realtime": {"preset": "ultrafast", "profile": "baseline", "crf": 28}
                }
            },
            "h265": {
                "presets": ["ultrafast", "superfast", "veryfast", "faster", "fast", "medium", "slow", "slower", "veryslow"],
                "profiles": ["main", "main10", "mainstillpicture"],
                "crf_range": (20, 30),
                "use_cases": {
                    "streaming": {"preset": "fast", "profile": "main", "crf": 25},
                    "archival": {"preset": "slow", "profile": "main10", "crf": 20},
                    "realtime": {"preset": "superfast", "profile": "main", "crf": 30}
                }
            },
            "av1": {
                "presets": ["realtime", "good", "allintra"],
                "profiles": ["main", "high", "professional"],
                "crf_range": (20, 40),
                "use_cases": {
                    "streaming": {"preset": "good", "crf": 30},
                    "archival": {"preset": "good", "crf": 25},
                    "realtime": {"preset": "realtime", "crf": 35}
                }
            }
        }
    
    def analyze_video_properties(self, input_file: str) -> Dict:
        """分析视频属性"""
        try:
            cmd = [
                "ffprobe", "-v", "quiet", "-print_format", "json",
                "-show_format", "-show_streams", input_file
            ]
            
            result = subprocess.run(cmd, capture_output=True, text=True)
            if result.returncode != 0:
                raise Exception(f"ffprobe failed: {result.stderr}")
            
            data = json.loads(result.stdout)
            
            # 提取视频流信息
            video_stream = None
            for stream in data.get("streams", []):
                if stream.get("codec_type") == "video":
                    video_stream = stream
                    break
            
            if not video_stream:
                raise Exception("No video stream found")
            
            return {
                "width": int(video_stream.get("width", 0)),
                "height": int(video_stream.get("height", 0)),
                "fps": eval(video_stream.get("r_frame_rate", "0/1")),
                "duration": float(video_stream.get("duration", 0)),
                "codec": video_stream.get("codec_name", ""),
                "bitrate": video_stream.get("bit_rate"),
                "pix_fmt": video_stream.get("pix_fmt", "")
            }
            
        except Exception as e:
            logging.error(f"视频分析失败: {e}")
            return {}
    
    def optimize_codec_config(self, 
                             input_file: str,
                             target_codec: str,
                             use_case: str,
                             target_size_mb: Optional[float] = None) -> CodecConfig:
        """优化编解码器配置"""
        
        video_props = self.analyze_video_properties(input_file)
        if not video_props:
            raise ValueError("无法分析视频属性")
        
        # 获取基础配置
        base_config = self.codec_profiles[target_codec]["use_cases"][use_case]
        
        # 根据视频属性调整配置
        adjusted_config = self._adjust_config_for_video(
            base_config, video_props, target_size_mb
        )
        
        return CodecConfig(
            codec=target_codec,
            preset=adjusted_config["preset"],
            crf=adjusted_config["crf"],
            profile=adjusted_config["profile"],
            additional_params=adjusted_config.get("additional_params", {})
        )
    
    def _adjust_config_for_video(self, 
                               base_config: Dict,
                               video_props: Dict,
                               target_size_mb: Optional[float]) -> Dict:
        """根据视频属性调整配置"""
        
        adjusted = base_config.copy()
        
        # 根据分辨率调整CRF
        resolution_factor = (video_props["width"] * video_props["height"]) / (1920 * 1080)
        if resolution_factor > 1.5:  # 4K及以上
            adjusted["crf"] = max(adjusted["crf"] - 2, 15)
        elif resolution_factor < 0.5:  # 720p及以下
            adjusted["crf"] = min(adjusted["crf"] + 2, 30)
        
        # 根据帧率调整预设
        fps = video_props.get("fps", 30)
        if fps > 60:
            adjusted["preset"] = "faster"  # 高帧率需要更快的预设
        elif fps < 24:
            adjusted["preset"] = "slow"    # 低帧率可以使用更慢的预设
        
        # 根据目标大小调整码率
        if target_size_mb:
            duration = video_props.get("duration", 60)
            target_bitrate = (target_size_mb * 8 * 1024 * 1024) / duration
            adjusted["bitrate"] = f"{int(target_bitrate)}"
        
        return adjusted
    
    def build_ffmpeg_command(self, 
                           input_file: str,
                           output_file: str,
                           config: CodecConfig) -> List[str]:
        """构建FFmpeg命令"""
        
        cmd = [
            "ffmpeg",
            "-i", input_file,
            "-c:v", config.codec,
            "-preset", config.preset,
            "-crf", str(config.crf),
            "-profile:v", config.profile,
            "-level", config.level
        ]
        
        if config.bitrate:
            cmd.extend(["-b:v", config.bitrate])
        
        if config.tune:
            cmd.extend(["-tune", config.tune])
        
        # 添加额外参数
        if config.additional_params:
            for key, value in config.additional_params.items():
                cmd.extend([key, value])
        
        # 音频编码
        cmd.extend(["-c:a", "aac", "-b:a", "128k"])
        
        cmd.append(output_file)
        
        return cmd
    
    def encode_with_benchmark(self, 
                            input_file: str,
                            output_file: str,
                            config: CodecConfig) -> Dict:
        """编码并基准测试"""
        
        cmd = self.build_ffmpeg_command(input_file, output_file, config)
        
        start_time = time.time()
        
        try:
            result = subprocess.run(cmd, capture_output=True, text=True)
            
            if result.returncode != 0:
                raise Exception(f"编码失败: {result.stderr}")
            
            encoding_time = time.time() - start_time
            
            # 获取输出文件信息
            output_props = self.analyze_video_properties(output_file)
            
            # 计算压缩比
            input_size = Path(input_file).stat().st_size
            output_size = Path(output_file).stat().st_size
            compression_ratio = input_size / output_size
            
            return {
                "success": True,
                "encoding_time": encoding_time,
                "input_size_mb": input_size / (1024 * 1024),
                "output_size_mb": output_size / (1024 * 1024),
                "compression_ratio": compression_ratio,
                "config": config.__dict__,
                "output_properties": output_props
            }
            
        except Exception as e:
            return {
                "success": False,
                "error": str(e),
                "command": " ".join(cmd)
            }
    
    def compare_codecs(self, 
                      input_file: str,
                      output_dir: str,
                      use_case: str = "streaming") -> List[Dict]:
        """比较不同编解码器"""
        
        Path(output_dir).mkdir(parents=True, exist_ok=True)
        
        codecs = ["h264", "h265", "av1"]
        results = []
        
        for codec in codecs:
            try:
                config = self.optimize_codec_config(input_file, codec, use_case)
                output_file = Path(output_dir) / f"output_{codec}.mp4"
                
                result = self.encode_with_benchmark(input_file, str(output_file), config)
                results.append({
                    "codec": codec,
                    "result": result
                })
                
            except Exception as e:
                results.append({
                    "codec": codec,
                    "error": str(e)
                })
        
        return results

class HardwareAccelerationOptimizer:
    """硬件加速优化器"""
    
    def __init__(self):
        self.hw_accelerators = {
            "nvidia": {
                "encoder": "h264_nvenc",
                "decoder": "h264_cuvid",
                "presets": ["default", "slow", "medium", "fast", "hp", "hq", "bd", "ll", "llhq", "llhp"],
                "devices": ["cuda", "cuvid", "nvenc", "opencl"]
            },
            "intel": {
                "encoder": "h264_qsv",
                "decoder": "h264_qsv",
                "presets": ["veryfast", "faster", "fast", "medium", "slow", "slower", "veryslow"],
                "devices": ["qsv", "vaapi", "opencl"]
            },
            "amd": {
                "encoder": "h264_amf",
                "decoder": "h264_amf",
                "presets": ["balanced", "speed", "quality"],
                "devices": ["amf", "opencl"]
            }
        }
    
    def detect_hardware(self) -> Dict:
        """检测可用硬件"""
        detected = {}
        
        # 检测NVIDIA
        try:
            result = subprocess.run(["nvidia-smi"], capture_output=True)
            if result.returncode == 0:
                detected["nvidia"] = True
        except:
            detected["nvidia"] = False
        
        # 检测Intel QSV
        try:
            result = subprocess.run(["vainfo"], capture_output=True)
            if result.returncode == 0:
                detected["intel"] = True
        except:
            detected["intel"] = False
        
        return detected
    
    def get_hw_config(self, hw_type: str, codec: str) -> Dict:
        """获取硬件加速配置"""
        if hw_type not in self.hw_accelerators:
            raise ValueError(f"不支持的硬件类型: {hw_type}")
        
        hw_config = self.hw_accelerators[hw_type]
        
        return {
            "encoder": hw_config["encoder"],
            "decoder": hw_config["decoder"],
            "presets": hw_config["presets"],
            "default_preset": "medium",
            "additional_params": {
                "-gpu": "0",
                "-preset": "medium"
            }
        }

# 使用示例
if __name__ == "__main__":
    optimizer = VideoCodecOptimizer()
    
    # 优化配置
    config = optimizer.optimize_codec_config(
        "input.mp4",
        "h264",
        "streaming",
        target_size_mb=50
    )
    
    print("优化配置:", config)
    
    # 执行编码
    result = optimizer.encode_with_benchmark(
        "input.mp4",
        "output_optimized.mp4",
        config
    )
    
    print("编码结果:", result)
    
    # 比较不同编解码器
    results = optimizer.compare_codecs(
        "input.mp4",
        "codec_comparison",
        "streaming"
    )
    
    print("编解码器比较:", results)