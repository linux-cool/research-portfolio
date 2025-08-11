#!/usr/bin/env python3
"""
音视频质量评估工具
基于第一性原理：客观指标 + 主观感知 = 全面质量评价
"""

import subprocess
import json
import logging
from typing import Dict, List, Tuple, Optional
from pathlib import Path
import numpy as np
from dataclasses import dataclass
import concurrent.futures
from datetime import datetime

@dataclass
class QualityMetrics:
    """质量指标"""
    psnr: float
    ssim: float
    vmaf: float
    ms_ssim: float
    niqe: float
    brisque: float
    processing_time: float
    file_size_mb: float
    bitrate_kbps: float

class VideoQualityAssessor:
    """视频质量评估器"""
    
    def __init__(self):
        self.supported_metrics = ["psnr", "ssim", "vmaf", "ms_ssim", "niqe", "brisque"]
        self.vmaf_models = {
            "vmaf_v0.6.1": "/usr/share/model/vmaf_v0.6.1.json",
            "vmaf_4k_v0.6.1": "/usr/share/model/vmaf_4k_v0.6.1.json",
            "vmaf_phone_v0.6.1": "/usr/share/model/vmaf_phone_v0.6.1.json"
        }
    
    def calculate_psnr(self, reference: str, distorted: str) -> float:
        """计算PSNR"""
        cmd = [
            "ffmpeg", "-i", distorted, "-i", reference,
            "-lavfi", "psnr", "-f", "null", "-"
        ]
        
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode != 0:
            return 0.0
        
        # 解析PSNR值
        for line in result.stderr.split('\n'):
            if "average:" in line and "psnr" in line:
                try:
                    return float(line.split("average:")[1].split()[0])
                except:
                    pass
        return 0.0
    
    def calculate_ssim(self, reference: str, distorted: str) -> float:
        """计算SSIM"""
        cmd = [
            "ffmpeg", "-i", distorted, "-i", reference,
            "-lavfi", "ssim", "-f", "null", "-"
        ]
        
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode != 0:
            return 0.0
        
        # 解析SSIM值
        for line in result.stderr.split('\n'):
            if "All:" in line:
                try:
                    return float(line.split("All:")[1].split()[0])
                except:
                    pass
        return 0.0
    
    def calculate_vmaf(self, reference: str, distorted: str, model: str = "vmaf_v0.6.1") -> float:
        """计算VMAF"""
        if model not in self.vmaf_models:
            model = "vmaf_v0.6.1"
        
        cmd = [
            "ffmpeg", "-i", distorted, "-i", reference,
            "-lavfi", f"libvmaf=model_path={self.vmaf_models[model]}:log_fmt=json",
            "-f", "null", "-"
        ]
        
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode != 0:
            return 0.0
        
        # 解析VMAF值
        for line in result.stderr.split('\n'):
            if '"VMAF score"' in line:
                try:
                    return float(line.split(':')[1].strip().replace(',', ''))
                except:
                    pass
        return 0.0
    
    def calculate_no_reference_metrics(self, video_path: str) -> Dict:
        """计算无参考指标"""
        # 这里简化实现，实际应使用专门的库
        metrics = {"niqe": 0.0, "brisque": 0.0}
        return metrics
    
    def assess_video_quality(self, 
                           reference: str, 
                           distorted: str,
                           metrics: List[str] = None) -> QualityMetrics:
        """全面评估视频质量"""
        
        if metrics is None:
            metrics = self.supported_metrics
        
        start_time = time.time()
        
        results = {}
        
        # 并行计算指标
        with concurrent.futures.ThreadPoolExecutor() as executor:
            futures = {}
            
            if "psnr" in metrics:
                futures["psnr"] = executor.submit(self.calculate_psnr, reference, distorted)
            if "ssim" in metrics:
                futures["ssim"] = executor.submit(self.calculate_ssim, reference, distorted)
            if "vmaf" in metrics:
                futures["vmaf"] = executor.submit(self.calculate_vmaf, reference, distorted)
            
            # 获取结果
            for metric, future in futures.items():
                try:
                    results[metric] = future.result(timeout=300)
                except:
                    results[metric] = 0.0
        
        # 无参考指标
        if any(metric in metrics for metric in ["niqe", "brisque"]):
            nr_metrics = self.calculate_no_reference_metrics(distorted)
            results.update(nr_metrics)
        
        # 获取文件信息
        file_size_mb = Path(distorted).stat().st_size / (1024 * 1024)
        bitrate_kbps = self._get_bitrate(distorted)
        
        processing_time = time.time() - start_time
        
        return QualityMetrics(
            psnr=results.get("psnr", 0.0),
            ssim=results.get("ssim", 0.0),
            vmaf=results.get("vmaf", 0.0),
            ms_ssim=results.get("ms_ssim", 0.0),
            niqe=results.get("niqe", 0.0),
            brisque=results.get("brisque", 0.0),
            processing_time=processing_time,
            file_size_mb=file_size_mb,
            bitrate_kbps=bitrate_kbps
        )
    
    def _get_bitrate(self, video_path: str) -> float:
        """获取视频码率"""
        try:
            cmd = ["ffprobe", "-v", "quiet", "-print_format", "json", 
                   "-show_streams", video_path]
            result = subprocess.run(cmd, capture_output=True, text=True)
            
            if result.returncode == 0:
                data = json.loads(result.stdout)
                for stream in data.get("streams", []):
                    if stream.get("codec_type") == "video":
                        bitrate = stream.get("bit_rate")
                        if bitrate:
                            return float(bitrate) / 1000  # kbps
            
            # 计算平均码率
            file_size = Path(video_path).stat().st_size
            duration = self._get_duration(video_path)
            if duration > 0:
                return (file_size * 8) / (duration * 1000)  # kbps
            
        except:
            pass
        return 0.0
    
    def _get_duration(self, video_path: str) -> float:
        """获取视频时长"""
        try:
            cmd = ["ffprobe", "-v", "error", "-show_entries", 
                   "format=duration", "-of", "default=noprint_wrappers=1:nokey=1", 
                   video_path]
            result = subprocess.run(cmd, capture_output=True, text=True)
            return float(result.stdout.strip())
        except:
            return 0.0
    
    def batch_assess_quality(self, 
                           reference_dir: str,
                           distorted_dir: str,
                           output_file: str) -> Dict:
        """批量评估质量"""
        
        reference_path = Path(reference_dir)
        distorted_path = Path(distorted_dir)
        
        results = []
        
        for ref_file in reference_path.glob("*.mp4"):
            dist_file = distorted_path / ref_file.name
            
            if dist_file.exists():
                try:
                    metrics = self.assess_video_quality(str(ref_file), str(dist_file))
                    
                    results.append({
                        "filename": ref_file.name,
                        "psnr": metrics.psnr,
                        "ssim": metrics.ssim,
                        "vmaf": metrics.vmaf,
                        "file_size_mb": metrics.file_size_mb,
                        "bitrate_kbps": metrics.bitrate_kbps,
                        "processing_time": metrics.processing_time
                    })
                    
                except Exception as e:
                    logging.error(f"评估失败 {ref_file.name}: {e}")
        
        # 保存结果
        with open(output_file, 'w') as f:
            json.dump(results, f, indent=2)
        
        # 计算统计信息
        if results:
            psnr_values = [r["psnr"] for r in results if r["psnr"] > 0]
            ssim_values = [r["ssim"] for r in results if r["ssim"] > 0]
            vmaf_values = [r["vmaf"] for r in results if r["vmaf"] > 0]
            
            stats = {
                "total_files": len(results),
                "average_psnr": np.mean(psnr_values) if psnr_values else 0,
                "average_ssim": np.mean(ssim_values) if ssim_values else 0,
                "average_vmaf": np.mean(vmaf_values) if vmaf_values else 0,
                "min_file_size": min(r["file_size_mb"] for r in results),
                "max_file_size": max(r["file_size_mb"] for r in results)
            }
            
            return {"results": results, "statistics": stats}
        
        return {"results": results, "statistics": {}}

class AudioQualityAssessor:
    """音频质量评估器"""
    
    def __init__(self):
        self.supported_metrics = ["pesq", "stoi", "visqol"]
    
    def calculate_pesq(self, reference: str, degraded: str) -> float:
        """计算PESQ"""
        # 需要额外安装PESQ工具
        try:
            cmd = ["ffmpeg", "-i", degraded, "-i", reference,
                   "-lavfi", "pesq", "-f", "null", "-"]
            result = subprocess.run(cmd, capture_output=True, text=True)
            return 0.0  # 简化实现
        except:
            return 0.0
    
    def assess_audio_quality(self, 
                           reference: str, 
                           degraded: str) -> Dict:
        """评估音频质量"""
        return {
            "pesq": self.calculate_pesq(reference, degraded),
            "processing_time": time.time()
        }

class QualityReportGenerator:
    """质量报告生成器"""
    
    def __init__(self):
        self.video_assessor = VideoQualityAssessor()
        self.audio_assessor = AudioQualityAssessor()
    
    def generate_report(self, 
                       results: List[Dict],
                       output_file: str) -> None:
        """生成质量报告"""
        
        report = {
            "timestamp": str(datetime.now()),
            "summary": {
                "total_files": len(results),
                "metrics": ["PSNR", "SSIM", "VMAF"]
            },
            "detailed_results": results
        }
        
        # 添加图表数据
        if results:
            report["charts"] = {
                "bitrate_vs_vmaf": [(r["bitrate_kbps"], r["vmaf"]) for r in results],
                "filesize_vs_quality": [(r["file_size_mb"], r["psnr"]) for r in results]
            }
        
        with open(output_file, 'w') as f:
            json.dump(report, f, indent=2)

# 使用示例
if __name__ == "__main__":
    assessor = VideoQualityAssessor()
    
    # 单个文件评估
    metrics = assessor.assess_video_quality(
        "reference.mp4",
        "encoded.mp4"
    )
    
    print("质量评估结果:")
    print(f"PSNR: {metrics.psnr:.2f} dB")
    print(f"SSIM: {metrics.ssim:.4f}")
    print(f"VMAF: {metrics.vmaf:.2f}")
    print(f"文件大小: {metrics.file_size_mb:.2f} MB")
    print(f"处理时间: {metrics.processing_time:.2f} 秒")