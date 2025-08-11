#!/usr/bin/env python3
"""
回声消除算法实现
基于第一性原理：延迟估计 + 自适应滤波 = 回声抑制
"""

import numpy as np
import scipy.signal as signal
import soundfile as sf
from typing import List, Tuple, Optional, Dict
from dataclasses import dataclass
import matplotlib.pyplot as plt
import logging
import json

@dataclass
class EchoParameters:
    """回声参数"""
    delay_samples: int
    attenuation: float
    delay_time_ms: float

class AdaptiveEchoCanceler:
    """自适应回声消除器"""
    
    def __init__(self, sample_rate: int = 16000, filter_length: int = 1024):
        self.sample_rate = sample_rate
        self.filter_length = filter_length
        self.mu = 0.001  # LMS步长
        
        # 自适应滤波器权重
        self.weights = np.zeros(filter_length)
        
        # 参考信号缓冲区
        self.reference_buffer = np.zeros(filter_length)
        
        # 状态变量
        self.error_history = []
        self.weight_history = []
        
    def lms_update(self, far_end: np.ndarray, near_end: np.ndarray) -> Tuple[np.ndarray, np.ndarray]:
        """LMS算法更新"""
        output = np.zeros(len(near_end))
        error = np.zeros(len(near_end))
        
        for n in range(len(near_end)):
            # 更新参考信号缓冲区
            self.reference_buffer = np.roll(self.reference_buffer, 1)
            self.reference_buffer[0] = far_end[n] if n < len(far_end) else 0
            
            # 计算预测的回声
            predicted_echo = np.dot(self.weights, self.reference_buffer)
            
            # 计算误差
            error[n] = near_end[n] - predicted_echo
            
            # LMS权重更新
            norm = np.dot(self.reference_buffer, self.reference_buffer) + 1e-10
            self.weights += self.mu * error[n] * self.reference_buffer / norm
            
            output[n] = error[n]
            
            # 记录历史
            self.error_history.append(error[n])
            self.weight_history.append(self.weights.copy())
        
        return output, error
    
    def nlms_update(self, far_end: np.ndarray, near_end: np.ndarray) -> Tuple[np.ndarray, np.ndarray]:
        """归一化LMS算法更新"""
        output = np.zeros(len(near_end))
        error = np.zeros(len(near_end))
        
        for n in range(len(near_end)):
            # 更新参考信号缓冲区
            self.reference_buffer = np.roll(self.reference_buffer, 1)
            self.reference_buffer[0] = far_end[n] if n < len(far_end) else 0
            
            # 计算预测的回声
            predicted_echo = np.dot(self.weights, self.reference_buffer)
            
            # 计算误差
            error[n] = near_end[n] - predicted_echo
            
            # NLMS权重更新
            norm = np.dot(self.reference_buffer, self.reference_buffer) + 1e-10
            self.weights += (self.mu / norm) * error[n] * self.reference_buffer
            
            output[n] = error[n]
        
        return output, error
    
    def get_convergence_metrics(self) -> Dict:
        """获取收敛指标"""
        if not self.error_history:
            return {}
        
        error_array = np.array(self.error_history)
        
        # 计算MSE
        mse = np.mean(error_array**2)
        
        # 计算收敛时间（简单估计）
        window_size = 100
        if len(error_array) > window_size:
            rolling_mse = np.convolve(error_array**2, np.ones(window_size)/window_size, mode='valid')
            
            # 找到稳定点（MSE变化小于阈值）
            threshold = 0.01 * np.max(rolling_mse)
            stable_points = np.where(rolling_mse < threshold)[0]
            
            if len(stable_points) > 0:
                convergence_time = stable_points[0] + window_size
            else:
                convergence_time = len(error_array)
        else:
            convergence_time = len(error_array)
        
        return {
            'final_mse': mse,
            'convergence_time': convergence_time,
            'convergence_ratio': convergence_time / len(error_array)
        }

class DelayEstimator:
    """延迟估计器"""
    
    def __init__(self, sample_rate: int = 16000, max_delay: int = 1000):
        self.sample_rate = sample_rate
        self.max_delay = max_delay
        
    def estimate_delay(self, far_end: np.ndarray, near_end: np.ndarray) -> EchoParameters:
        """估计回声延迟"""
        # 计算互相关
        correlation = signal.correlate(near_end, far_end, mode='full')
        
        # 找到最大相关峰
        max_index = np.argmax(np.abs(correlation))
        
        # 计算延迟
        delay_samples = max_index - len(far_end) + 1
        
        # 计算衰减
        if delay_samples > 0 and delay_samples < len(far_end):
            attenuation = np.abs(correlation[max_index]) / np.sqrt(
                np.sum(far_end**2) * np.sum(near_end**2)
            )
        else:
            attenuation = 0.0
        
        delay_time_ms = delay_samples * 1000 / self.sample_rate
        
        return EchoParameters(
            delay_samples=delay_samples,
            attenuation=attenuation,
            delay_time_ms=delay_time_ms
        )
    
    def visualize_delay_estimation(self, far_end: np.ndarray, near_end: np.ndarray):
        """可视化延迟估计"""
        correlation = signal.correlate(near_end, far_end, mode='full')
        delays = np.arange(-len(far_end) + 1, len(near_end))
        
        plt.figure(figsize=(12, 6))
        plt.subplot(2, 1, 1)
        plt.plot(delays, np.abs(correlation))
        plt.title('延迟估计 - 互相关函数')
        plt.xlabel('延迟 (样本)')
        plt.ylabel('相关值')
        
        max_index = np.argmax(np.abs(correlation))
        max_delay = delays[max_index]
        plt.axvline(x=max_delay, color='r', linestyle='--', label=f'估计延迟: {max_delay}')
        plt.legend()
        
        plt.tight_layout()
        plt.savefig('delay_estimation.png')
        plt.close()

class EchoCancellationPipeline:
    """回声消除管道"""
    
    def __init__(self, sample_rate: int = 16000):
        self.sample_rate = sample_rate
        self.canceler = AdaptiveEchoCanceler(sample_rate)
        self.delay_estimator = DelayEstimator(sample_rate)
        
        # 性能评估
        self.performance_metrics = {}
    
    def simulate_echo(self, far_end: np.ndarray, delay_ms: float = 100, attenuation: float = 0.3) -> np.ndarray:
        """模拟回声信号"""
        delay_samples = int(delay_ms * self.sample_rate / 1000)
        
        # 创建延迟信号
        delayed = np.zeros(len(far_end) + delay_samples)
        delayed[delay_samples:] = far_end * attenuation
        
        # 截取到原始长度
        echo = delayed[:len(far_end)]
        
        return echo
    
    def cancel_echo(self, far_end: np.ndarray, near_end: np.ndarray) -> Dict:
        """执行回声消除"""
        
        # 估计延迟
        echo_params = self.delay_estimator.estimate_delay(far_end, near_end)
        
        # 执行回声消除
        start_time = time.time()
        
        # 使用NLMS算法
        output, error = self.canceler.nlms_update(far_end, near_end)
        
        processing_time = time.time() - start_time
        
        # 获取性能指标
        metrics = self.canceler.get_convergence_metrics()
        
        # 计算SNR改进
        snr_before = self._calculate_snr(near_end, far_end)
        snr_after = self._calculate_snr(output, far_end)
        
        return {
            'echo_parameters': {
                'estimated_delay_ms': echo_params.delay_time_ms,
                'estimated_attenuation': echo_params.attenuation
            },
            'performance_metrics': metrics,
            'snr_improvement': snr_after - snr_before,
            'processing_time': processing_time,
            'output': output
        }
    
    def _calculate_snr(self, signal: np.ndarray, noise: np.ndarray) -> float:
        """计算信噪比"""
        signal_power = np.mean(signal**2)
        noise_power = np.mean(noise**2)
        
        if noise_power > 0:
            return 10 * np.log10(signal_power / noise_power)
        return float('inf')
    
    def evaluate_performance(self, far_end: np.ndarray, near_end: np.ndarray, 
                           ground_truth: Optional[np.ndarray] = None) -> Dict:
        """评估回声消除性能"""
        
        # 执行回声消除
        result = self.cancel_echo(far_end, near_end)
        
        # 计算额外指标
        output = result['output']
        
        # 计算ERLE (Echo Return Loss Enhancement)
        original_echo_power = np.mean(near_end**2)
        residual_echo_power = np.mean(output**2)
        
        if residual_echo_power > 0:
            erle = 10 * np.log10(original_echo_power / residual_echo_power)
        else:
            erle = float('inf')
        
        # 计算残余回声功率
        residual_power = np.mean(output**2)
        
        # 计算收敛速度
        error_history = np.array(self.canceler.error_history)
        if len(error_history) > 100:
            initial_error = np.mean(error_history[:100])
            final_error = np.mean(error_history[-100:])
            convergence_rate = (initial_error - final_error) / initial_error
        else:
            convergence_rate = 0.0
        
        evaluation = {
            'erle': erle,
            'residual_echo_power': residual_power,
            'convergence_rate': convergence_rate,
            'final_error': np.std(error_history[-100:]) if len(error_history) > 100 else np.std(error_history),
            'algorithm': 'NLMS',
            'filter_length': self.canceler.filter_length
        }
        
        result['evaluation'] = evaluation
        return result
    
    def visualize_cancellation(self, far_end: np.ndarray, near_end: np.ndarray, 
                             output: np.ndarray):
        """可视化回声消除结果"""
        
        time_axis = np.arange(len(far_end)) / self.sample_rate
        
        fig, axes = plt.subplots(3, 1, figsize=(15, 12))
        
        # 原始信号
        axes[0].plot(time_axis, far_end, label='远端信号', alpha=0.7)
        axes[0].plot(time_axis, near_end, label='近端信号(含回声)', alpha=0.7)
        axes[0].set_title('原始信号')
        axes[0].set_ylabel('幅度')
        axes[0].legend()
        axes[0].grid(True)
        
        # 消除后的信号
        axes[1].plot(time_axis, output, label='消除后信号', color='green')
        axes[1].set_title('回声消除后信号')
        axes[1].set_ylabel('幅度')
        axes[1].legend()
        axes[1].grid(True)
        
        # 误差收敛
        if self.canceler.error_history:
            error_history = np.array(self.canceler.error_history)
            axes[2].plot(error_history**2)
            axes[2].set_title('误差收敛曲线')
            axes[2].set_ylabel('误差平方')
            axes[2].set_xlabel('样本')
            axes[2].grid(True)
        
        plt.tight_layout()
        plt.savefig('echo_cancellation_results.png')
        plt.close()

# 使用示例
if __name__ == "__main__":
    # 创建回声消除管道
    pipeline = EchoCancellationPipeline(sample_rate=16000)
    
    # 生成测试信号
    t = np.linspace(0, 2, 32000)  # 2秒音频
    far_end = np.sin(2 * np.pi * 1000 * t) * 0.3  # 1kHz测试信号
    
    # 模拟回声
    echo = pipeline.simulate_echo(far_end, delay_ms=50, attenuation=0.5)
    
    # 添加噪声
    noise = np.random.normal(0, 0.05, len(echo))
    near_end = echo + noise
    
    # 执行回声消除
    result = pipeline.evaluate_performance(far_end, near_end)
    
    # 打印结果
    print("回声消除结果:")
    print(json.dumps(result, indent=2, default=lambda x: float(x) if isinstance(x, np.number) else str(x)))
    
    # 可视化结果
    pipeline.visualize_cancellation(far_end, near_end, result['output'])
    
    # 保存结果
    sf.write('original_with_echo.wav', near_end, 16000)
    sf.write('echo_cancelled.wav', result['output'], 16000)