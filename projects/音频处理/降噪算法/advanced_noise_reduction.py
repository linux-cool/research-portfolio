#!/usr/bin/env python3
"""
高级降噪算法实现
基于第一性原理：频谱分析 + 自适应滤波 = 噪声抑制
"""

import numpy as np
import scipy.signal as signal
import soundfile as sf
import matplotlib.pyplot as plt
from typing import List, Tuple, Optional, Dict
from dataclasses import dataclass
import logging
import json

@dataclass
class NoiseProfile:
    """噪声频谱特征"""
    noise_floor: np.ndarray
    spectral_centroid: float
    spectral_rolloff: float
    zero_crossing_rate: float
    spectral_bandwidth: float

class SpectralSubtraction:
    """频谱减法降噪"""
    
    def __init__(self, sample_rate: int = 44100, frame_size: int = 2048, overlap: int = 0.75):
        self.sample_rate = sample_rate
        self.frame_size = frame_size
        self.hop_size = int(frame_size * (1 - overlap))
        self.overlap = overlap
        
        # 噪声估计参数
        self.noise_profile = None
        self.noise_frames = 0
        self.alpha = 1.0  # 过减因子
        self.beta = 0.002  # 频谱下限
        
    def estimate_noise(self, noise_sample: np.ndarray) -> NoiseProfile:
        """估计噪声频谱特征"""
        # 计算STFT
        f, t, Zxx = signal.stft(
            noise_sample, 
            fs=self.sample_rate, 
            nperseg=self.frame_size,
            noverlap=int(self.frame_size * self.overlap)
        )
        
        # 计算噪声功率谱密度
        noise_power = np.mean(np.abs(Zxx)**2, axis=1)
        
        # 计算频谱特征
        freqs = np.linspace(0, self.sample_rate/2, len(noise_power))
        
        # 频谱质心
        spectral_centroid = np.sum(freqs * noise_power) / np.sum(noise_power)
        
        # 频谱滚降
        cumsum_power = np.cumsum(noise_power)
        rolloff_idx = np.where(cumsum_power >= 0.85 * cumsum_power[-1])[0][0]
        spectral_rolloff = freqs[rolloff_idx]
        
        # 频谱带宽
        spectral_bandwidth = np.sqrt(
            np.sum(((freqs - spectral_centroid) ** 2) * noise_power) / np.sum(noise_power)
        )
        
        # 过零率
        zero_crossing_rate = np.sum(np.diff(np.signbit(noise_sample))) / len(noise_sample)
        
        noise_profile = NoiseProfile(
            noise_floor=noise_power,
            spectral_centroid=spectral_centroid,
            spectral_rolloff=spectral_rolloff,
            zero_crossing_rate=zero_crossing_rate,
            spectral_bandwidth=spectral_bandwidth
        )
        
        self.noise_profile = noise_profile
        return noise_profile
    
    def process(self, audio_data: np.ndarray) -> np.ndarray:
        """应用频谱减法降噪"""
        if self.noise_profile is None:
            logging.warning("未设置噪声配置文件，跳过降噪")
            return audio_data
        
        # 计算STFT
        f, t, Zxx = signal.stft(
            audio_data,
            fs=self.sample_rate,
            nperseg=self.frame_size,
            noverlap=int(self.frame_size * self.overlap)
        )
        
        # 获取幅度和相位
        magnitude = np.abs(Zxx)
        phase = np.angle(Zxx)
        
        # 频谱减法
        noise_floor = np.sqrt(self.noise_profile.noise_floor[:, np.newaxis])
        subtracted = magnitude - self.alpha * noise_floor
        
        # 应用频谱下限
        subtracted = np.maximum(subtracted, self.beta * noise_floor)
        
        # 重构信号
        enhanced_Zxx = subtracted * np.exp(1j * phase)
        _, processed = signal.istft(enhanced_Zxx, fs=self.sample_rate)
        
        return processed

class WienerFilter:
    """维纳滤波器降噪"""
    
    def __init__(self, sample_rate: int = 44100, frame_size: int = 2048):
        self.sample_rate = sample_rate
        self.frame_size = frame_size
        
    def estimate_psd(self, signal_data: np.ndarray) -> Tuple[np.ndarray, np.ndarray]:
        """估计功率谱密度"""
        freqs, psd = signal.welch(
            signal_data,
            fs=self.sample_rate,
            nperseg=self.frame_size,
            return_onesided=True
        )
        return freqs, psd
    
    def compute_wiener_filter(self, clean_psd: np.ndarray, noise_psd: np.ndarray) -> np.ndarray:
        """计算维纳滤波器"""
        # 维纳滤波器传递函数
        h_wiener = clean_psd / (clean_psd + noise_psd)
        return h_wiener
    
    def apply_filter(self, audio_data: np.ndarray, wiener_filter: np.ndarray) -> np.ndarray:
        """应用维纳滤波器"""
        # 计算FFT
        fft_data = np.fft.rfft(audio_data)
        
        # 应用滤波器
        filtered_fft = fft_data * wiener_filter
        
        # 反变换回时域
        filtered = np.fft.irfft(filtered_fft, n=len(audio_data))
        
        return filtered

class AdaptiveNoiseReduction:
    """自适应降噪算法"""
    
    def __init__(self, sample_rate: int = 44100, filter_order: int = 64):
        self.sample_rate = sample_rate
        self.filter_order = filter_order
        self.mu = 0.01  # 步长参数
        
        # LMS滤波器权重
        self.weights = np.zeros(filter_order)
        
    def lms_update(self, input_signal: np.ndarray, desired: np.ndarray) -> np.ndarray:
        """LMS算法更新"""
        output = np.zeros(len(desired))
        
        for n in range(self.filter_order, len(desired)):
            # 获取输入向量
            x = input_signal[n-self.filter_order:n]
            
            # 计算输出
            y = np.dot(self.weights, x)
            
            # 计算误差
            e = desired[n] - y
            
            # 更新权重
            self.weights += self.mu * e * x
            
            output[n] = y
        
        return output
    
    def nlms_update(self, input_signal: np.ndarray, desired: np.ndarray) -> np.ndarray:
        """归一化LMS算法更新"""
        output = np.zeros(len(desired))
        
        for n in range(self.filter_order, len(desired)):
            x = input_signal[n-self.filter_order:n]
            
            # 归一化因子
            norm = np.dot(x, x) + 1e-10
            
            y = np.dot(self.weights, x)
            e = desired[n] - y
            
            # 归一化更新
            self.weights += (self.mu / norm) * e * x
            
            output[n] = y
        
        return output

class DeepLearningNoiseReduction:
    """深度学习降噪算法（简化实现）"""
    
    def __init__(self, model_path: Optional[str] = None):
        self.model_path = model_path
        self.model = None
        self.load_model()
    
    def load_model(self):
        """加载预训练模型"""
        # 这里简化实现，实际应加载真实的深度学习模型
        logging.info("加载深度学习降噪模型...")
    
    def preprocess(self, audio_data: np.ndarray) -> np.ndarray:
        """预处理音频数据"""
        # 归一化
        audio_data = audio_data / np.max(np.abs(audio_data))
        
        # 分帧
        frame_size = 1024
        hop_size = 512
        
        frames = []
        for i in range(0, len(audio_data) - frame_size + 1, hop_size):
            frame = audio_data[i:i+frame_size]
            frames.append(frame)
        
        return np.array(frames)
    
    def postprocess(self, frames: np.ndarray) -> np.ndarray:
        """后处理重构音频"""
        # 重叠相加法重构信号
        frame_size = frames.shape[1]
        hop_size = frame_size // 2
        
        signal_length = (len(frames) - 1) * hop_size + frame_size
        reconstructed = np.zeros(signal_length)
        
        for i, frame in enumerate(frames):
            start = i * hop_size
            end = start + frame_size
            reconstructed[start:end] += frame
        
        return reconstructed
    
    def denoise(self, audio_data: np.ndarray) -> np.ndarray:
        """深度学习降噪"""
        # 简化实现：使用频谱减法作为替代
        spectral_subtraction = SpectralSubtraction()
        
        # 估计噪声（使用前1秒作为噪声样本）
        noise_sample = audio_data[:min(len(audio_data), self.sample_rate)]
        spectral_subtraction.estimate_noise(noise_sample)
        
        return spectral_subtraction.process(audio_data)

class NoiseReductionPipeline:
    """降噪算法管道"""
    
    def __init__(self, sample_rate: int = 44100):
        self.sample_rate = sample_rate
        
        # 初始化各种降噪算法
        self.spectral_subtraction = SpectralSubtraction(sample_rate)
        self.wiener_filter = WienerFilter(sample_rate)
        self.adaptive_filter = AdaptiveNoiseReduction(sample_rate)
        self.dl_reduction = DeepLearningNoiseReduction()
        
        # 结果存储
        self.results = {}
    
    def compare_algorithms(self, audio_file: str, noise_file: str) -> Dict:
        """比较不同降噪算法"""
        
        # 加载音频
        audio_data, sr = sf.read(audio_file)
        noise_data, _ = sf.read(noise_file)
        
        if sr != self.sample_rate:
            from scipy.signal import resample
            audio_data = resample(audio_data, int(len(audio_data) * self.sample_rate / sr))
            noise_data = resample(noise_data, int(len(noise_data) * self.sample_rate / sr))
        
        # 估计噪声
        self.spectral_subtraction.estimate_noise(noise_data)
        
        # 应用各种算法
        algorithms = {
            'spectral_subtraction': self.spectral_subtraction.process,
            'wiener_filter': lambda x: self.wiener_filter.apply_filter(x, self.wiener_filter.compute_wiener_filter(
                np.ones(len(x)), np.ones(len(noise_data))
            )),
            'adaptive_filter': lambda x: self.adaptive_filter.nlms_update(x, x),
            'deep_learning': self.dl_reduction.denoise
        }
        
        results = {}
        
        for name, algorithm in algorithms.items():
            try:
                start_time = time.time()
                processed = algorithm(audio_data)
                processing_time = time.time() - start_time
                
                # 计算SNR
                snr_before = self._calculate_snr(audio_data, noise_data)
                snr_after = self._calculate_snr(processed, noise_data)
                
                results[name] = {
                    'processing_time': processing_time,
                    'snr_before': snr_before,
                    'snr_after': snr_after,
                    'snr_improvement': snr_after - snr_before
                }
                
                # 保存处理结果
                sf.write(f'denoised_{name}.wav', processed, self.sample_rate)
                
            except Exception as e:
                logging.error(f"算法 {name} 失败: {e}")
                results[name] = {'error': str(e)}
        
        self.results = results
        return results
    
    def _calculate_snr(self, signal: np.ndarray, noise: np.ndarray) -> float:
        """计算信噪比"""
        signal_power = np.mean(signal**2)
        noise_power = np.mean(noise**2)
        
        if noise_power > 0:
            return 10 * np.log10(signal_power / noise_power)
        else:
            return float('inf')
    
    def visualize_results(self, audio_file: str, noise_file: str):
        """可视化降噪结果"""
        
        audio_data, _ = sf.read(audio_file)
        
        # 创建子图
        fig, axes = plt.subplots(3, 2, figsize=(15, 12))
        
        # 原始信号
        axes[0, 0].plot(audio_data[:1000])
        axes[0, 0].set_title('原始信号')
        axes[0, 0].set_ylabel('幅度')
        
        # 频谱
        freqs, psd = signal.welch(audio_data, fs=self.sample_rate)
        axes[0, 1].semilogy(freqs, psd)
        axes[0, 1].set_title('原始频谱')
        axes[0, 1].set_ylabel('功率谱密度')
        
        # 处理后的信号
        processed = self.spectral_subtraction.process(audio_data)
        axes[1, 0].plot(processed[:1000])
        axes[1, 0].set_title('降噪后信号')
        axes[1, 0].set_ylabel('幅度')
        
        # 降噪后频谱
        freqs_proc, psd_proc = signal.welch(processed, fs=self.sample_rate)
        axes[1, 1].semilogy(freqs_proc, psd_proc)
        axes[1, 1].set_title('降噪后频谱')
        axes[1, 1].set_ylabel('功率谱密度')
        
        # 频谱差异
        axes[2, 0].semilogy(freqs, psd - psd_proc)
        axes[2, 0].set_title('频谱差异')
        axes[2, 0].set_ylabel('功率差异')
        axes[2, 0].set_xlabel('频率 (Hz)')
        
        # SNR对比
        snr_values = [result['snr_after'] for result in self.results.values() 
                     if 'snr_after' in result]
        algorithm_names = [name for name in self.results.keys() 
                          if 'snr_after' in self.results[name]]
        
        if snr_values:
            axes[2, 1].bar(algorithm_names, snr_values)
            axes[2, 1].set_title('SNR对比')
            axes[2, 1].set_ylabel('SNR (dB)')
            axes[2, 1].tick_params(axis='x', rotation=45)
        
        plt.tight_layout()
        plt.savefig('noise_reduction_comparison.png')
        plt.close()

# 使用示例
if __name__ == "__main__":
    # 创建降噪管道
    pipeline = NoiseReductionPipeline(sample_rate=44100)
    
    # 比较算法
    results = pipeline.compare_algorithms('input.wav', 'noise.wav')
    
    # 打印结果
    print("降噪算法对比结果:")
    for algorithm, result in results.items():
        if 'error' not in result:
            print(f"{algorithm}:")
            print(f"  SNR提升: {result['snr_improvement']:.2f} dB")
            print(f"  处理时间: {result['processing_time']:.3f} 秒")
        else:
            print(f"{algorithm}: 错误 - {result['error']}")
    
    # 可视化结果
    pipeline.visualize_results('input.wav', 'noise.wav')