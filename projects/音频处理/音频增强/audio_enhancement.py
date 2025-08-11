#!/usr/bin/env python3
"""
音频增强算法实现
基于第一性原理：频谱均衡 + 动态处理 + 空间增强 = 音质提升
"""

import numpy as np
import scipy.signal as signal
import soundfile as sf
from typing import List, Tuple, Dict, Optional
from dataclasses import dataclass
import matplotlib.pyplot as plt
import logging
import json

@dataclass
class EnhancementParams:
    """增强参数配置"""
    eq_gains: List[float]  # 均衡器增益
    compression_ratio: float  # 压缩比
    compression_threshold: float  # 压缩阈值
    reverb_amount: float  # 混响量
    stereo_width: float  # 立体声宽度

class Equalizer:
    """参数均衡器"""
    
    def __init__(self, sample_rate: int = 44100, bands: List[Dict] = None):
        self.sample_rate = sample_rate
        
        # 默认频段
        if bands is None:
            self.bands = [
                {'freq': 32, 'q': 1.0, 'gain': 0.0},    # 超低频
                {'freq': 64, 'q': 1.0, 'gain': 0.0},    # 低频
                {'freq': 125, 'q': 1.0, 'gain': 0.0},   # 中低频
                {'freq': 250, 'q': 1.0, 'gain': 0.0},   # 低中频
                {'freq': 500, 'q': 1.0, 'gain': 0.0},   # 中低频
                {'freq': 1000, 'q': 1.0, 'gain': 0.0},  # 中频
                {'freq': 2000, 'q': 1.0, 'gain': 0.0},  # 中高频
                {'freq': 4000, 'q': 1.0, 'gain': 0.0},  # 高频
                {'freq': 8000, 'q': 1.0, 'gain': 0.0},  # 超高频
                {'freq': 16000, 'q': 1.0, 'gain': 0.0}  # 极高频
            ]
        else:
            self.bands = bands
            
        self.filters = self._design_filters()
    
    def _design_filters(self) -> List[Dict]:
        """设计带通滤波器"""
        filters = []
        nyquist = self.sample_rate / 2
        
        for band in self.bands:
            # 计算滤波器系数
            w0 = 2 * np.pi * band['freq'] / self.sample_rate
            alpha = np.sin(w0) / (2 * band['q'])
            
            # 峰值滤波器系数
            A = 10**(band['gain'] / 40)
            
            b0 = 1 + alpha * A
            b1 = -2 * np.cos(w0)
            b2 = 1 - alpha * A
            a0 = 1 + alpha / A
            a1 = -2 * np.cos(w0)
            a2 = 1 - alpha / A
            
            # 归一化
            b = [b0 / a0, b1 / a0, b2 / a0]
            a = [1, a1 / a0, a2 / a0]
            
            filters.append({
                'b': b,
                'a': a,
                'freq': band['freq'],
                'q': band['q'],
                'gain': band['gain']
            })
        
        return filters
    
    def set_gain(self, band_index: int, gain: float):
        """设置频段增益"""
        if 0 <= band_index < len(self.bands):
            self.bands[band_index]['gain'] = gain
            self.filters = self._design_filters()
    
    def process(self, audio_data: np.ndarray) -> np.ndarray:
        """应用均衡器"""
        processed = audio_data.copy()
        
        for filter_info in self.filters:
            processed = signal.lfilter(filter_info['b'], filter_info['a'], processed)
        
        return processed

class DynamicRangeCompressor:
    """动态范围压缩器"""
    
    def __init__(self, sample_rate: int = 44100):
        self.sample_rate = sample_rate
        self.parameters = {
            'threshold': -20.0,      # 阈值 (dB)
            'ratio': 4.0,            # 压缩比
            'attack_time': 10.0,     # 启动时间 (ms)
            'release_time': 100.0,   # 释放时间 (ms)
            'knee_width': 3.0,       # 拐点宽度 (dB)
            'makeup_gain': 0.0       # 补偿增益 (dB)
        }
        
        # 状态变量
        self.envelope = 0.0
        self.gain_reduction = 0.0
    
    def set_parameters(self, **kwargs):
        """设置压缩器参数"""
        for key, value in kwargs.items():
            if key in self.parameters:
                self.parameters[key] = value
    
    def _db_to_linear(self, db: float) -> float:
        """dB转线性"""
        return 10**(db / 20)
    
    def _linear_to_db(self, linear: float) -> float:
        """线性转dB"""
        return 20 * np.log10(np.abs(linear) + 1e-10)
    
    def _calculate_gain(self, input_level: float) -> float:
        """计算增益衰减"""
        # 转换到dB域
        input_db = self._linear_to_db(input_level)
        
        # 计算超过阈值的部分
        if input_db > self.parameters['threshold']:
            # 软拐点压缩
            excess = input_db - self.parameters['threshold']
            
            if excess <= self.parameters['knee_width'] / 2:
                # 软拐点区域
                gain_reduction = excess**2 / (2 * self.parameters['knee_width'])
            else:
                # 压缩区域
                gain_reduction = excess * (1 - 1/self.parameters['ratio']) + \
                               self.parameters['knee_width'] / 4
                
            return -gain_reduction
        
        return 0.0
    
    def process(self, audio_data: np.ndarray) -> np.ndarray:
        """应用动态压缩"""
        processed = np.zeros_like(audio_data)
        
        # 计算包络
        attack_coeff = np.exp(-1 / (self.parameters['attack_time'] * self.sample_rate / 1000))
        release_coeff = np.exp(-1 / (self.parameters['release_time'] * self.sample_rate / 1000))
        
        for i, sample in enumerate(audio_data):
            # 计算信号包络
            abs_sample = np.abs(sample)
            if abs_sample > self.envelope:
                self.envelope = attack_coeff * self.envelope + (1 - attack_coeff) * abs_sample
            else:
                self.envelope = release_coeff * self.envelope + (1 - release_coeff) * abs_sample
            
            # 计算增益
            gain_db = self._calculate_gain(self.envelope)
            gain_linear = self._db_to_linear(gain_db + self.parameters['makeup_gain'])
            
            processed[i] = sample * gain_linear
        
        return np.clip(processed, -1.0, 1.0)

class ReverbGenerator:
    """混响生成器"""
    
    def __init__(self, sample_rate: int = 44100):
        self.sample_rate = sample_rate
        self.parameters = {
            'room_size': 0.5,      # 房间大小 (0-1)
            'damping': 0.5,        # 阻尼 (0-1)
            'wet_level': 0.3,      # 湿信号电平
            'dry_level': 0.7,      # 干信号电平
            'width': 1.0           # 立体声宽度
        }
        
        # 延迟线
        self.delay_buffers = []
        self.delay_times = []
        
        # 初始化梳状滤波器
        self._init_comb_filters()
    
    def _init_comb_filters(self):
        """初始化梳状滤波器"""
        # Schroeder混响器参数
        delay_times_ms = [29.7, 37.1, 41.1, 43.7, 47.3, 53.3, 59.5, 67.1]
        
        self.delay_times = [int(ms * self.sample_rate / 1000) for ms in delay_times_ms]
        self.delay_buffers = [np.zeros(dt) for dt in self.delay_times]
        self.buffer_indices = [0] * len(self.delay_times)
    
    def set_parameters(self, **kwargs):
        """设置混响参数"""
        for key, value in kwargs.items():
            if key in self.parameters:
                self.parameters[key] = value
    
    def process(self, audio_data: np.ndarray) -> np.ndarray:
        """应用混响效果"""
        processed = np.zeros_like(audio_data)
        
        for i, sample in enumerate(audio_data):
            # 干信号
            dry = sample * self.parameters['dry_level']
            
            # 湿信号
            wet = 0.0
            
            # 梳状滤波器
            for j, (buffer, delay_time, buffer_idx) in enumerate(
                zip(self.delay_buffers, self.delay_times, self.buffer_indices)
            ):
                # 获取延迟样本
                delayed_sample = buffer[buffer_idx]
                
                # 更新延迟缓冲区
                feedback = delayed_sample * (1 - self.parameters['damping'])
                buffer[buffer_idx] = sample + feedback * self.parameters['room_size']
                
                # 更新索引
                self.buffer_indices[j] = (buffer_idx + 1) % delay_time
                
                # 累加湿信号
                wet += delayed_sample
            
            # 应用湿信号电平
            wet *= self.parameters['wet_level'] / len(self.delay_buffers)
            
            processed[i] = dry + wet
        
        return np.clip(processed, -1.0, 1.0)

class StereoEnhancer:
    """立体声增强器"""
    
    def __init__(self, sample_rate: int = 44100):
        self.sample_rate = sample_rate
        self.parameters = {
            'width': 1.0,          # 立体声宽度
            'delay_ms': 0.1,       # 延迟时间
            'mid_gain': 0.0,       # 中置增益
            'side_gain': 0.0       # 侧置增益
        }
        
    def set_parameters(self, **kwargs):
        """设置立体声参数"""
        for key, value in kwargs.items():
            if key in self.parameters:
                self.parameters[key] = value
    
    def process_stereo(self, left: np.ndarray, right: np.ndarray) -> Tuple[np.ndarray, np.ndarray]:
        """处理立体声信号"""
        # 转换为M-S格式
        mid = (left + right) / 2
        side = (left - right) / 2
        
        # 应用增益
        mid *= self._db_to_linear(self.parameters['mid_gain'])
        side *= self._db_to_linear(self.parameters['side_gain'])
        
        # 立体声宽度控制
        side *= self.parameters['width']
        
        # 添加延迟增强
        delay_samples = int(self.parameters['delay_ms'] * self.sample_rate / 1000)
        
        # 转换回L-R格式
        enhanced_left = mid + side
        enhanced_right = mid - side
        
        return enhanced_left, enhanced_right
    
    def _db_to_linear(self, db: float) -> float:
        """dB转线性"""
        return 10**(db / 20)

class AudioEnhancementPipeline:
    """音频增强管道"""
    
    def __init__(self, sample_rate: int = 44100):
        self.sample_rate = sample_rate
        
        # 初始化各个模块
        self.equalizer = Equalizer(sample_rate)
        self.compressor = DynamicRangeCompressor(sample_rate)
        self.reverb = ReverbGenerator(sample_rate)
        self.stereo_enhancer = StereoEnhancer(sample_rate)
        
        # 处理链
        self.chain = [
            self.equalizer,
            self.compressor,
            self.reverb,
            self.stereo_enhancer
        ]
    
    def set_enhancement_parameters(self, params: EnhancementParams):
        """设置增强参数"""
        # 设置均衡器
        for i, gain in enumerate(params.eq_gains):
            self.equalizer.set_gain(i, gain)
        
        # 设置压缩器
        self.compressor.set_parameters(
            threshold=params.compression_threshold,
            ratio=params.compression_ratio
        )
        
        # 设置混响
        self.reverb.set_parameters(wet_level=params.reverb_amount)
        
        # 设置立体声
        self.stereo_enhancer.set_parameters(width=params.stereo_width)
    
    def process_mono(self, audio_data: np.ndarray) -> np.ndarray:
        """处理单声道音频"""
        processed = audio_data.copy()
        
        # 应用均衡器
        processed = self.equalizer.process(processed)
        
        # 应用压缩器
        processed = self.compressor.process(processed)
        
        # 应用混响
        processed = self.reverb.process(processed)
        
        return processed
    
    def process_stereo(self, left: np.ndarray, right: np.ndarray) -> Tuple[np.ndarray, np.ndarray]:
        """处理立体声音频"""
        # 处理左右声道
        left_processed = self.process_mono(left)
        right_processed = self.process_mono(right)
        
        # 应用立体声增强
        left_final, right_final = self.stereo_enhancer.process_stereo(
            left_processed, right_processed
        )
        
        return left_final, right_final
    
    def process_file(self, input_file: str, output_file: str, params: EnhancementParams) -> Dict:
        """处理音频文件"""
        try:
            # 读取音频
            audio_data, sr = sf.read(input_file)
            
            # 重采样
            if sr != self.sample_rate:
                from scipy.signal import resample
                new_length = int(len(audio_data) * self.sample_rate / sr)
                audio_data = resample(audio_data, new_length)
            
            # 设置参数
            self.set_enhancement_parameters(params)
            
            # 处理音频
            if audio_data.ndim == 1:
                # 单声道
                processed = self.process_mono(audio_data)
            else:
                # 立体声
                left, right = audio_data[:, 0], audio_data[:, 1]
                left_processed, right_processed = self.process_stereo(left, right)
                processed = np.column_stack((left_processed, right_processed))
            
            # 保存结果
            sf.write(output_file, processed, self.sample_rate)
            
            return {
                'success': True,
                'input_file': input_file,
                'output_file': output_file,
                'sample_rate': self.sample_rate
            }
            
        except Exception as e:
            return {
                'success': False,
                'error': str(e)
            }
    
    def analyze_enhancement(self, original: np.ndarray, enhanced: np.ndarray) -> Dict:
        """分析增强效果"""
        
        # 计算频谱特征
        def spectral_features(signal):
            fft = np.fft.fft(signal)
            magnitude = np.abs(fft)
            
            # 频谱质心
            freqs = np.fft.fftfreq(len(signal), 1/self.sample_rate)
            centroid = np.sum(freqs * magnitude) / np.sum(magnitude)
            
            # 频谱带宽
            bandwidth = np.sqrt(np.sum(((freqs - centroid) ** 2) * magnitude) / np.sum(magnitude))
            
            # 频谱滚降
            cumsum_magnitude = np.cumsum(magnitude)
            rolloff_idx = np.where(cumsum_magnitude >= 0.85 * cumsum_magnitude[-1])[0][0]
            rolloff = freqs[rolloff_idx]
            
            return {
                'spectral_centroid': centroid,
                'spectral_bandwidth': bandwidth,
                'spectral_rolloff': rolloff
            }
        
        # 计算特征
        original_features = spectral_features(original)
        enhanced_features = spectral_features(enhanced)
        
        # 计算动态范围
        original_range = np.max(original) - np.min(original)
        enhanced_range = np.max(enhanced) - np.min(enhanced)
        
        return {
            'spectral_changes': {
                'centroid_shift': enhanced_features['spectral_centroid'] - original_features['spectral_centroid'],
                'bandwidth_change': enhanced_features['spectral_bandwidth'] - original_features['spectral_bandwidth'],
                'rolloff_shift': enhanced_features['spectral_rolloff'] - original_features['spectral_rolloff']
            },
            'dynamic_range': {
                'original': original_range,
                'enhanced': enhanced_range,
                'reduction': original_range - enhanced_range
            }
        }

# 使用示例
if __name__ == "__main__":
    # 创建增强管道
    enhancer = AudioEnhancementPipeline(sample_rate=44100)
    
    # 设置增强参数
    params = EnhancementParams(
        eq_gains=[2.0, 1.0, 0.0, -1.0, -2.0, 0.0, 1.0, 2.0, 1.0, 0.0],  # 10段均衡器
        compression_ratio=3.0,
        compression_threshold=-15.0,
        reverb_amount=0.2,
        stereo_width=1.2
    )
    
    # 处理音频
    result = enhancer.process_file('input.wav', 'enhanced.wav', params)
    print(f"增强结果: {result}")
    
    # 分析增强效果
    if result['success']:
        original, sr = sf.read('input.wav')
        enhanced, _ = sf.read('enhanced.wav')
        
        if len(original) > len(enhanced):
            original = original[:len(enhanced)]
        elif len(enhanced) > len(original):
            enhanced = enhanced[:len(original)]
        
        analysis = enhancer.analyze_enhancement(original, enhanced)
        print("增强效果分析:")
        print(json.dumps(analysis, indent=2))