#!/usr/bin/env python3
"""
HLS低延迟优化器
基于第一性原理：分片大小 + 缓存策略 = 延迟优化
"""

import os
import asyncio
import logging
import time
from typing import Dict, List, Optional, Tuple
from dataclasses import dataclass
from pathlib import Path
import m3u8
import ffmpeg
import json
from datetime import datetime, timedelta

@dataclass
class HLSSegment:
    """HLS分片信息"""
    duration: float
    uri: str
    byte_range: Optional[Tuple[int, int]] = None
    discontinuity: bool = False

class HLSLowLatencyOptimizer:
    """HLS低延迟优化器"""
    
    def __init__(self, 
                 segment_duration: float = 2.0,
                 target_latency: float = 3.0,
                 max_segments: int = 6):
        self.segment_duration = segment_duration
        self.target_latency = target_latency
        self.max_segments = max_segments
        
        # 低延迟参数
        self.part_duration = segment_duration / 4  # 0.5秒分片
        self.chunk_duration = segment_duration / 8  # 0.25秒chunk
        
        # 缓存策略
        self.cache_size = 100
        self.cache_ttl = 300  # 5分钟
        
        # 统计信息
        self.segments_created = 0
        self.total_encoding_time = 0
        self.average_latency = 0
    
    def optimize_manifest(self, 
                         input_file: str, 
                         output_dir: str,
                         quality_levels: List[Dict] = None) -> Dict:
        """优化HLS清单文件"""
        
        if quality_levels is None:
            quality_levels = [
                {"name": "1080p", "bitrate": "5000000", "resolution": "1920x1080"},
                {"name": "720p", "bitrate": "2500000", "resolution": "1280x720"},
                {"name": "480p", "bitrate": "1000000", "resolution": "854x480"}
            ]
        
        output_dir = Path(output_dir)
        output_dir.mkdir(parents=True, exist_ok=True)
        
        # 创建主播放列表
        master_playlist = self._create_master_playlist(
            quality_levels, 
            output_dir.name
        )
        
        # 为每个质量级别创建媒体播放列表
        results = {}
        for quality in quality_levels:
            result = self._create_media_playlist(
                input_file,
                output_dir,
                quality
            )
            results[quality["name"]] = result
        
        # 保存主播放列表
        master_path = output_dir / "master.m3u8"
        with open(master_path, 'w') as f:
            f.write(master_playlist)
        
        return {
            "master_playlist": str(master_path),
            "quality_levels": results,
            "optimization_params": {
                "segment_duration": self.segment_duration,
                "part_duration": self.part_duration,
                "target_latency": self.target_latency
            }
        }
    
    def _create_master_playlist(self, 
                               quality_levels: List[Dict], 
                               base_name: str) -> str:
        """创建主播放列表"""
        playlist_lines = [
            "#EXTM3U",
            "#EXT-X-VERSION:6",
            "#EXT-X-TARGETDURATION:2",
            "#EXT-X-MEDIA-SEQUENCE:0",
            "#EXT-X-PLAYLIST-TYPE:VOD",
            "#EXT-X-INDEPENDENT-SEGMENTS",
            "#EXT-X-ALLOW-CACHE:YES",
            "#EXT-X-START:TIME-OFFSET=0",
            ""
        ]
        
        for quality in quality_levels:
            playlist_lines.extend([
                f"#EXT-X-STREAM-INF:BANDWIDTH={quality['bitrate']},"
                f"RESOLUTION={quality['resolution']},"
                f"CODECS=\"avc1.640028,mp4a.40.2\"",
                f"{quality['name']}.m3u8"
            ])
        
        return "\n".join(playlist_lines)
    
    def _create_media_playlist(self, 
                              input_file: str,
                              output_dir: Path,
                              quality: Dict) -> Dict:
        """创建媒体播放列表"""
        
        output_prefix = output_dir / quality["name"]
        playlist_path = output_prefix.with_suffix('.m3u8')
        
        # 使用FFmpeg进行分片和转码
        command = [
            'ffmpeg',
            '-i', input_file,
            '-c:v', 'libx264',
            '-c:a', 'aac',
            '-b:v', quality['bitrate'],
            '-s', quality['resolution'],
            '-preset', 'veryfast',
            '-hls_time', str(self.segment_duration),
            '-hls_playlist_type', 'vod',
            '-hls_flags', 'independent_segments',
            '-hls_segment_filename', f'{output_prefix}_%03d.ts',
            str(playlist_path)
        ]
        
        start_time = time.time()
        
        try:
            # 执行FFmpeg命令
            (
                ffmpeg
                .input(input_file)
                .output(
                    str(playlist_path),
                    vcodec="libx264",
                    acodec="aac",
                    b=quality['bitrate'],
                    s=quality['resolution'],
                    preset="veryfast",
                    hls_time=self.segment_duration,
                    hls_playlist_type="vod",
                    hls_flags="independent_segments",
                    hls_segment_filename=f"{output_prefix}_%03d.ts"
                )
                .overwrite_output()
                .run()
            )
            
            encoding_time = time.time() - start_time
            
            # 优化播放列表
            self._optimize_playlist(str(playlist_path))
            
            # 统计信息
            segments = self._count_segments(output_dir, quality["name"])
            
            return {
                "playlist_path": str(playlist_path),
                "segments_count": segments,
                "encoding_time": encoding_time,
                "average_segment_size": self._calculate_average_segment_size(
                    output_dir, quality["name"]
                )
            }
            
        except Exception as e:
            logging.error(f"HLS优化失败: {e}")
            return {"error": str(e)}
    
    def _optimize_playlist(self, playlist_path: str):
        """优化播放列表"""
        try:
            playlist = m3u8.load(playlist_path)
            
            # 添加低延迟优化标签
            playlist.version = 6
            playlist.is_endlist = True
            playlist.target_duration = self.segment_duration
            
            # 保存优化后的播放列表
            playlist.dump(playlist_path)
            
        except Exception as e:
            logging.error(f"播放列表优化失败: {e}")
    
    def _count_segments(self, output_dir: Path, prefix: str) -> int:
        """计算分片数量"""
        return len(list(output_dir.glob(f"{prefix}_*.ts")))
    
    def _calculate_average_segment_size(self, 
                                       output_dir: Path, 
                                       prefix: str) -> float:
        """计算平均分片大小"""
        segments = list(output_dir.glob(f"{prefix}_*.ts"))
        if not segments:
            return 0.0
        
        total_size = sum(seg.stat().st_size for seg in segments)
        return total_size / len(segments)

class LLHLSOptimizer(HLSLowLatencyOptimizer):
    """低延迟HLS优化器 (LL-HLS)"""
    
    def __init__(self, 
                 target_latency: float = 2.0,
                 part_duration: float = 0.5,
                 preload_hints: bool = True):
        super().__init__(
            segment_duration=2.0,
            target_latency=target_latency,
            max_segments=4
        )
        self.part_duration = part_duration
        self.preload_hints = preload_hints
    
    def create_llhls_playlist(self, 
                             input_file: str,
                             output_dir: str,
                             quality: Dict) -> Dict:
        """创建低延迟HLS播放列表"""
        
        output_dir = Path(output_dir)
        output_dir.mkdir(parents=True, exist_ok=True)
        
        output_prefix = output_dir / quality["name"]
        playlist_path = output_prefix.with_suffix('.m3u8')
        
        # 使用FFmpeg创建LL-HLS
        command = [
            'ffmpeg',
            '-i', input_file,
            '-c:v', 'libx264',
            '-c:a', 'aac',
            '-b:v', quality['bitrate'],
            '-s', quality['resolution'],
            '-preset', 'ultrafast',
            '-tune', 'zerolatency',
            '-hls_time', str(self.segment_duration),
            '-hls_playlist_type', 'event',
            '-hls_flags', 'independent_segments+program_date_time',
            '-hls_segment_type', 'mpegts',
            '-hls_part_size', str(self.part_duration),
            '-hls_playlist_duration', str(self.target_latency),
            f'{output_prefix}_%03d.ts'
        ]
        
        try:
            start_time = time.time()
            
            (
                ffmpeg
                .input(input_file)
                .output(
                    str(playlist_path),
                    vcodec="libx264",
                    acodec="aac",
                    b=quality['bitrate'],
                    s=quality['resolution'],
                    preset="ultrafast",
                    tune="zerolatency",
                    hls_time=self.segment_duration,
                    hls_playlist_type="event",
                    hls_flags="independent_segments+program_date_time",
                    hls_segment_type="mpegts",
                    hls_part_size=self.part_duration,
                    hls_playlist_duration=self.target_latency
                )
                .overwrite_output()
                .run()
            )
            
            encoding_time = time.time() - start_time
            
            # 手动添加LL-HLS标签
            self._add_llhls_tags(str(playlist_path))
            
            return {
                "playlist_path": str(playlist_path),
                "encoding_time": encoding_time,
                "target_latency": self.target_latency,
                "part_duration": self.part_duration
            }
            
        except Exception as e:
            logging.error(f"LL-HLS优化失败: {e}")
            return {"error": str(e)}
    
    def _add_llhls_tags(self, playlist_path: str):
        """添加LL-HLS标签"""
        try:
            with open(playlist_path, 'r') as f:
                content = f.read()
            
            # 添加LL-HLS标签
            optimized_content = content.replace(
                "#EXTM3U",
                "#EXTM3U\n#EXT-X-VERSION:9\n#EXT-X-SERVER-CONTROL:CAN-BLOCK-RELOAD=YES,PART-HOLD-BACK=0.252\n#EXT-X-PART-INF:PART-TARGET=0.5"
            )
            
            with open(playlist_path, 'w') as f:
                f.write(optimized_content)
                
        except Exception as e:
            logging.error(f"添加LL-HLS标签失败: {e}")

class HLSAnalyzer:
    """HLS分析器"""
    
    def __init__(self):
        self.analytics = {
            "total_segments": 0,
            "total_duration": 0,
            "average_bitrate": 0,
            "latency_metrics": {}
        }
    
    def analyze_playlist(self, playlist_path: str) -> Dict:
        """分析HLS播放列表"""
        try:
            playlist = m3u8.load(playlist_path)
            
            segments = playlist.segments
            total_duration = sum(segment.duration for segment in segments)
            
            # 计算统计信息
            durations = [segment.duration for segment in segments]
            bitrates = []
            
            # 分析分片文件
            playlist_dir = Path(playlist_path).parent
            segment_sizes = []
            
            for segment in segments:
                segment_path = playlist_dir / segment.uri
                if segment_path.exists():
                    size = segment_path.stat().st_size
                    segment_sizes.append(size)
                    
                    # 计算码率
                    if segment.duration > 0:
                        bitrate = (size * 8) / segment.duration
                        bitrates.append(bitrate)
            
            analysis = {
                "total_segments": len(segments),
                "total_duration": total_duration,
                "average_segment_duration": np.mean(durations) if durations else 0,
                "segment_duration_variance": np.var(durations) if durations else 0,
                "average_bitrate": np.mean(bitrates) / 1000 if bitrates else 0,  # kbps
                "total_size_mb": sum(segment_sizes) / (1024 * 1024),
                "efficiency_score": self._calculate_efficiency(segments, segment_sizes)
            }
            
            return analysis
            
        except Exception as e:
            logging.error(f"HLS分析失败: {e}")
            return {"error": str(e)}
    
    def _calculate_efficiency(self, segments: List, segment_sizes: List) -> float:
        """计算HLS效率分数"""
        if not segments or not segment_sizes:
            return 0.0
        
        # 基于分片大小和时长的效率计算
        total_duration = sum(segment.duration for segment in segments)
        total_size = sum(segment_sizes)
        
        if total_duration == 0:
            return 0.0
        
        # 计算效率分数 (0-100)
        bitrate = (total_size * 8) / total_duration
        efficiency = min(100, (bitrate / 5000000) * 100)  # 标准化到5Mbps
        
        return efficiency

# 使用示例
if __name__ == "__main__":
    optimizer = HLSLowLatencyOptimizer(
        segment_duration=2.0,
        target_latency=3.0
    )
    
    # 示例使用
    input_file = "input.mp4"
    output_dir = "hls_output"
    
    result = optimizer.optimize_manifest(
        input_file,
        output_dir,
        quality_levels=[
            {"name": "720p", "bitrate": "2500000", "resolution": "1280x720"}
        ]
    )
    
    print("HLS优化结果:")
    print(json.dumps(result, indent=2, ensure_ascii=False))