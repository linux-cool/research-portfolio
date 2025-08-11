#!/usr/bin/env python3
"""
实时音频处理器
基于第一性原理：信号采样 + 数字滤波 = 音频增强
"""

import numpy as np
import scipy.signal as signal
import soundfile as sf
import logging
from typing import List, Tuple, Optional, Dict
from dataclasses import dataclass
import threading
import queue
import time
import json

@dataclass
class AudioConfig:
    """音频配置参数"""
    sample_rate: int = 44100
    chunk_size: int = 1024
    channels: int = 2
    dtype: str = 'float32'
    buffer_size: int = 10

class RealTimeAudioProcessor:
    """实时音频处理器"""
    
    def __init__(self, config: AudioConfig):
        self.config = config
        self.is_running = False
        self.input_queue = queue.Queue(maxsize=config.buffer_size)
        self.output_queue = queue.Queue(maxsize=config.buffer_size)
        self.processing_thread = None
        
        # 滤波器状态
        self.filter_state = {}
        
        # 性能统计
        self.stats = {
            'frames_processed': 0,
            'total_processing_time': 0.0,
            'average_latency': 0.0,
            'peak_cpu_usage': 0.0
        }
    
    def start_processing(self):
        """开始处理"""
        if not self.is_running:
            self.is_running = True
            self.processing_thread = threading.Thread(target=self._processing_loop)
            self.processing_thread.start()
    
    def stop_processing(self):
        """停止处理"""
        self.is_running = False
        if self.processing_thread:
            self.processing_thread.join()
    
    def _processing_loop(self):
        """处理循环"""
        while self.is_running:
            try:
                # 获取输入数据
                audio_data = self.input_queue.get(timeout=0.1)
                
                start_time = time.time()
                
                # 处理音频
                processed_data = self.process_audio(audio_data)
                
                # 计算延迟
                processing_time = time.time() - start_time
                self.stats['total_processing_time'] += processing_time
                self.stats['frames_processed'] += 1
                
                # 更新平均延迟
                if self.stats['frames_processed'] > 0:
                    self.stats['average_latency'] = (
                        self.stats['total_processing_time'] / self.stats['frames_processed']
                    )
                
                # 输出结果
                self.output_queue.put(processed_data)
                
            except queue.Empty:
                continue
            except Exception as e:
                logging.error(f"处理错误: {e}")
    
    def process_audio(self, audio_data: np.ndarray) -> np.ndarray:
        """处理音频数据 - 可被子类重写"""
        return audio_data
    
    def get_stats(self) -> Dict:
        """获取处理统计"""
        return self.stats.copy()

class NoiseReductionProcessor(RealTimeAudioProcessor):
    """降噪处理器"""
    
    def __init__(self, config: AudioConfig, noise_threshold: float = 0.01):
        super().__init__(config)
        self.noise_threshold = noise_threshold
        
        # 设计降噪滤波器
        self.noise_gate = self._design_noise_gate()
        
        # 噪声频谱估计
        self.noise_profile = None
        self.noise_frames = 0
    
    def _design_noise_gate(self) -> Dict:
        """设计噪声门"""
        # 低通滤波器用于噪声检测
        nyquist = self.config.sample_rate / 2
        low_cutoff = 1000 / nyquist
        b, a = signal.butter(4, low_cutoff, btype='low')
        
        return {'b': b, 'a': a, 'zi': signal.lfilter_zi(b, a)}
    
    def estimate_noise_profile(self, noise_sample: np.ndarray):
        """估计噪声频谱"""
        # 使用STFT分析噪声频谱
        f, t, Zxx = signal.stft(
            noise_sample, 
            fs=self.config.sample_rate, 
            nperseg=256
        )
        
        # 计算平均噪声频谱
        self.noise_profile = np.mean(np.abs(Zxx), axis=1)
        self.noise_frames = len(t)
    
    def process_audio(self, audio_data: np.ndarray) -> np.ndarray:
        """降噪处理"""
        if self.noise_profile is None:
            return audio_data
        
        # 应用频谱减法
        f, t, Zxx = signal.stft(
            audio_data, 
            fs=self.config.sample_rate, 
            nperseg=256
        )
        
        # 计算增益因子
        magnitude = np.abs(Zxx)
        phase = np.angle(Zxx)
        
        # 频谱减法
        gain = np.maximum(0, magnitude - self.noise_profile[:, np.newaxis])
        
        # 重构信号
        enhanced_Zxx = gain * np.exp(1j * phase)
        _, processed = signal.istft(enhanced_Zxx, fs=self.config.sample_rate)
        
        return processed

class EchoCancellationProcessor(RealTimeAudioProcessor):
    """回声消除处理器"""
    
    def __init__(self, config: AudioConfig, filter_length: int = 256):
        super().__init__(config)
        self.filter_length = filter_length
        self.adaptive_filter = np.zeros(filter_length)
        self.step_size = 0.01
        
        # 参考信号缓冲区
        self.reference_buffer = np.zeros(filter_length)
        self.buffer_index = 0
    
    def process_audio(self, audio_data: np.ndarray) -> np.ndarray:
        """回声消除处理"""
        processed = np.zeros_like(audio_data)
        
        for i, sample in enumerate(audio_data):
            # 更新参考信号缓冲区
            self.reference_buffer = np.roll(self.reference_buffer, 1)
            self.reference_buffer[0] = sample
            
            # 计算预测的回声
            predicted_echo = np.dot(self.reference_buffer, self.adaptive_filter)
            
            # 消除回声
            error = sample - predicted_echo
            processed[i] = error
            
            # 更新自适应滤波器 (NLMS算法)
            norm = np.dot(self.reference_buffer, self.reference_buffer)
            if norm > 0:
                self.adaptive_filter += (
                    self.step_size * error * self.reference_buffer / norm
                )
        
        return processed

class AudioEnhancementProcessor(RealTimeAudioProcessor):
    """音频增强处理器"""
    
    def __init__(self, config: AudioConfig):
        super().__init__(config)
        
        # 均衡器频段
        self.eq_bands = [
            {'freq': 60, 'gain': 0.0},    # 低频
            {'freq': 250, 'gain': 0.0},   # 中低频
            {'freq': 1000, 'gain': 0.0},  # 中频
            {'freq': 4000, 'gain': 0.0},  # 中高频
            {'freq': 8000, 'gain': 0.0}   # 高频
        ]
        
        # 压缩器参数
        self.compressor = {
            'threshold': -20.0,  # dB
            'ratio': 4.0,        # 压缩比
            'attack': 10.0,      # ms
            'release': 100.0,    # ms
            'makeup_gain': 0.0   # dB
        }
        
        # 设计均衡器滤波器
        self.eq_filters = self._design_eq_filters()
    
    def _design_eq_filters(self) -> List[Dict]:
        """设计均衡器滤波器"""
        filters = []
        nyquist = self.config.sample_rate / 2
        
        for band in self.eq_bands:
            # 使用二阶峰值滤波器
            w0 = 2 * np.pi * band['freq'] / self.config.sample_rate
            alpha = np.sin(w0) / (2 * 1.0)  # Q=1
            
            # 计算滤波器系数
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
                'zi': signal.lfilter_zi(b, a)
            })
        
        return filters
    
    def set_eq_gain(self, band_index: int, gain: float):
        """设置EQ增益"""
        if 0 <= band_index < len(self.eq_bands):
            self.eq_bands[band_index]['gain'] = gain
            self.eq_filters = self._design_eq_filters()
    
    def apply_compression(self, audio_data: np.ndarray) -> np.ndarray:
        """应用动态压缩"""
        # 计算RMS电平
        rms_level = np.sqrt(np.mean(audio_data**2))
        rms_db = 20 * np.log10(rms_level + 1e-10)
        
        # 计算增益衰减
        if rms_db > self.compressor['threshold']:
            excess = rms_db - self.compressor['threshold']
            gain_reduction = excess * (1 - 1/self.compressor['ratio'])
            
            # 应用平滑的增益控制
            gain = 10**((-gain_reduction + self.compressor['makeup_gain']) / 20)
            audio_data *= gain
        
        return np.clip(audio_data, -1.0, 1.0)
    
    def process_audio(self, audio_data: np.ndarray) -> np.ndarray:
        """音频增强处理"""
        processed = audio_data.copy()
        
        # 应用均衡器
        for filter_info in self.eq_filters:
            processed, _ = signal.lfilter(
                filter_info['b'], 
                filter_info['a'], 
                processed, 
                zi=filter_info['zi'] * processed[0]
            )
        
        # 应用压缩器
        processed = self.apply_compression(processed)
        
        return processed

class AudioProcessorPipeline:
    """音频处理管道"""
    
    def __init__(self, config: AudioConfig):
        self.config = config
        self.processors = []
        
        # 初始化处理器
        self.noise_reduction = NoiseReductionProcessor(config)
        self.echo_cancellation = EchoCancellationProcessor(config)
        self.enhancement = AudioEnhancementProcessor(config)
        
        # 构建处理链
        self.processors = [
            self.noise_reduction,
            self.echo_cancellation,
            self.enhancement
        ]
    
    def process_file(self, input_file: str, output_file: str) -> Dict:
        """处理音频文件"""
        try:
            # 读取音频文件
            audio_data, sample_rate = sf.read(input_file)
            
            # 确保采样率匹配
            if sample_rate != self.config.sample_rate:
                from scipy.signal import resample
                new_length = int(len(audio_data) * self.config.sample_rate / sample_rate)
                audio_data = resample(audio_data, new_length)
            
            # 处理音频
            processed = audio_data
            for processor in self.processors:
                processed = processor.process_audio(processed)
            
            # 保存处理后的音频
            sf.write(output_file, processed, self.config.sample_rate)
            
            return {
                'success': True,
                'input_file': input_file,
                'output_file': output_file,
                'original_length': len(audio_data),
                'processed_length': len(processed)
            }
            
        except Exception as e:
            return {
                'success': False,
                'error': str(e)
            }
    
    def process_real_time(self, input_device: int = None, output_device: int = None):
        """实时处理"""
        import sounddevice as sd
        
        def audio_callback(indata, outdata, frames, time, status):
            if status:
                print(status)
            
            # 处理音频
            processed = indata[:, 0]  # 单声道处理
            for processor in self.processors:
                processed = processor.process_audio(processed)
            
            # 输出处理后的音频
            outdata[:, 0] = processed
            if self.config.channels > 1:
                outdata[:, 1] = processed
        
        with sd.Stream(
            samplerate=self.config.sample_rate,
            blocksize=self.config.chunk_size,
            channels=self.config.channels,
            dtype=self.config.dtype,
            callback=audio_callback
        ):
            print("实时音频处理已启动，按Ctrl+C停止...")
            try:
                while True:
                    time.sleep(1)
            except KeyboardInterrupt:
                print("\n实时处理已停止")

# 使用示例
if __name__ == "__main__":
    config = AudioConfig(
        sample_rate=44100,
        chunk_size=1024,
        channels=2
    )
    
    # 创建处理管道
    pipeline = AudioProcessorPipeline(config)
    
    # 处理文件
    result = pipeline.process_file('input.wav', 'output_processed.wav')
    print(f"处理结果: {result}")
    
    # 实时处理
    # pipeline.process_real_time()