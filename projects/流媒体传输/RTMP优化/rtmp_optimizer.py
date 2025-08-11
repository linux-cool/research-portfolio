#!/usr/bin/env python3
"""
RTMP协议优化器
基于第一性原理：网络拥塞控制 + 自适应码率 = 最优传输效率
"""

import asyncio
import logging
import time
from typing import Dict, Optional, Tuple
from dataclasses import dataclass
import numpy as np
from collections import deque
import threading

@dataclass
class RTMPStats:
    """RTMP统计信息"""
    bytes_sent: int = 0
    bytes_received: int = 0
    packets_sent: int = 0
    packets_received: int = 0
    rtt_ms: float = 0.0
    throughput_bps: float = 0.0
    packet_loss_rate: float = 0.0
    buffer_level: int = 0
    
class RTMPAdaptiveBitrate:
    """RTMP自适应码率控制器"""
    
    def __init__(self, initial_bitrate: int = 3000000):
        self.current_bitrate = initial_bitrate
        self.target_bitrate = initial_bitrate
        self.min_bitrate = 500000
        self.max_bitrate = 8000000
        
        # 滑动窗口统计
        self.stats_window = deque(maxlen=100)
        self.throughput_history = deque(maxlen=50)
        
        # 控制参数
        self.alpha = 0.1  # 平滑因子
        self.beta = 0.2   # 调整幅度
        self.gamma = 0.8  # 保守因子
        
        # 网络状态
        self.network_state = "stable"
        self.congestion_detected = False
        
        self.lock = threading.Lock()
    
    def update_network_stats(self, stats: RTMPStats):
        """更新网络统计信息"""
        with self.lock:
            self.stats_window.append(stats)
            self.throughput_history.append(stats.throughput_bps)
            
            # 检测网络拥塞
            self._detect_congestion()
            
            # 调整码率
            self._adjust_bitrate()
    
    def _detect_congestion(self):
        """检测网络拥塞"""
        if len(self.stats_window) < 10:
            return
            
        # 计算最近10个样本的统计信息
        recent_stats = list(self.stats_window)[-10:]
        
        # 丢包率检测
        avg_loss = np.mean([s.packet_loss_rate for s in recent_stats])
        if avg_loss > 0.05:  # 5%丢包率
            self.congestion_detected = True
            self.network_state = "congested"
            return
        
        # RTT抖动检测
        rtt_values = [s.rtt_ms for s in recent_stats]
        rtt_variance = np.var(rtt_values)
        if rtt_variance > 100:  # 100ms抖动
            self.congestion_detected = True
            self.network_state = "unstable"
            return
        
        # 吞吐量下降检测
        if len(self.throughput_history) >= 20:
            recent_throughput = np.mean(list(self.throughput_history)[-10:])
            historical_throughput = np.mean(list(self.throughput_history)[-20:-10])
            
            if recent_throughput < historical_throughput * 0.8:
                self.congestion_detected = True
                self.network_state = "degraded"
                return
        
        self.congestion_detected = False
        self.network_state = "stable"
    
    def _adjust_bitrate(self):
        """调整码率"""
        if not self.throughput_history:
            return
            
        # 计算平滑吞吐量
        smoothed_throughput = np.mean(list(self.throughput_history)[-5:])
        
        if self.congestion_detected:
            # 拥塞时降低码率
            reduction_factor = 0.7
            if self.network_state == "congested":
                reduction_factor = 0.5
            elif self.network_state == "unstable":
                reduction_factor = 0.8
            
            self.target_bitrate = max(
                self.min_bitrate,
                int(self.current_bitrate * reduction_factor)
            )
        else:
            # 网络良好时尝试提升码率
            if smoothed_throughput > self.current_bitrate * 1.2:
                increase_factor = 1.1
                self.target_bitrate = min(
                    self.max_bitrate,
                    int(self.current_bitrate * increase_factor)
                )
        
        # 平滑过渡
        self.current_bitrate = int(
            self.gamma * self.current_bitrate + (1 - self.gamma) * self.target_bitrate
        )
    
    def get_optimal_bitrate(self) -> int:
        """获取最优码率"""
        with self.lock:
            return self.current_bitrate
    
    def get_network_state(self) -> str:
        """获取网络状态"""
        with self.lock:
            return self.network_state

class RTMPBufferManager:
    """RTMP缓冲区管理器"""
    
    def __init__(self, max_buffer_size: int = 1000):
        self.max_buffer_size = max_buffer_size
        self.current_buffer_size = 0
        self.buffer_threshold_low = int(max_buffer_size * 0.2)
        self.buffer_threshold_high = int(max_buffer_size * 0.8)
        
        # 缓冲区状态
        self.buffer_state = "normal"
        self.drop_count = 0
        
        # 统计信息
        self.buffer_utilization = []
        self.dropped_packets = []
    
    def update_buffer_level(self, level: int):
        """更新缓冲区水位"""
        self.current_buffer_size = level
        self.buffer_utilization.append(level)
        
        # 缓冲区状态管理
        if level <= self.buffer_threshold_low:
            self.buffer_state = "low"
        elif level >= self.buffer_threshold_high:
            self.buffer_state = "high"
        else:
            self.buffer_state = "normal"
    
    def should_drop_packets(self) -> bool:
        """判断是否应该丢包"""
        if self.current_buffer_size >= self.max_buffer_size:
            self.drop_count += 1
            self.dropped_packets.append(time.time())
            return True
        return False
    
    def get_buffer_stats(self) -> Dict:
        """获取缓冲区统计"""
        return {
            "current_size": self.current_buffer_size,
            "max_size": self.max_buffer_size,
            "state": self.buffer_state,
            "utilization": np.mean(self.buffer_utilization[-10:]) if self.buffer_utilization else 0,
            "drop_count": self.drop_count
        }

class RTMPCongestionController:
    """RTMP拥塞控制器"""
    
    def __init__(self):
        self.congestion_window = 1000  # 初始拥塞窗口
        self.slow_start_threshold = 10000
        self.duplicate_acks = 0
        self.state = "slow_start"  # slow_start, congestion_avoidance, fast_recovery
        
        # 统计信息
        self.rtt_samples = deque(maxlen=100)
        self.retransmissions = 0
        
    def on_packet_loss(self, is_timeout: bool = False):
        """处理丢包事件"""
        if is_timeout:
            # 超时丢包
            self.slow_start_threshold = max(self.congestion_window // 2, 2)
            self.congestion_window = 1
            self.state = "slow_start"
            self.retransmissions += 1
        else:
            # 快速重传
            self.slow_start_threshold = max(self.congestion_window // 2, 2)
            self.congestion_window = self.slow_start_threshold + 3
            self.state = "fast_recovery"
    
    def on_ack_received(self, bytes_acked: int):
        """处理ACK接收"""
        if self.state == "slow_start":
            self.congestion_window += 1
            if self.congestion_window >= self.slow_start_threshold:
                self.state = "congestion_avoidance"
        elif self.state == "congestion_avoidance":
            self.congestion_window += 1 / self.congestion_window
        elif self.state == "fast_recovery":
            self.congestion_window = self.slow_start_threshold
            self.state = "congestion_avoidance"
    
    def get_congestion_window(self) -> int:
        """获取当前拥塞窗口"""
        return int(self.congestion_window)
    
    def get_state(self) -> str:
        """获取控制器状态"""
        return self.state

class RTMPOptimizer:
    """RTMP优化器主类"""
    
    def __init__(self, initial_bitrate: int = 3000000):
        self.adaptive_bitrate = RTMPAdaptiveBitrate(initial_bitrate)
        self.buffer_manager = RTMPBufferManager()
        self.congestion_controller = RTMPCongestionController()
        
        # 统计信息
        self.start_time = time.time()
        self.total_bytes_sent = 0
        self.optimization_logs = []
    
    def optimize(self, stats: RTMPStats) -> Dict:
        """执行RTMP优化"""
        # 更新自适应码率
        self.adaptive_bitrate.update_network_stats(stats)
        
        # 更新缓冲区管理
        self.buffer_manager.update_buffer_level(stats.buffer_level)
        
        # 获取优化建议
        optimal_bitrate = self.adaptive_bitrate.get_optimal_bitrate()
        congestion_window = self.congestion_controller.get_congestion_window()
        buffer_stats = self.buffer_manager.get_buffer_stats()
        
        # 生成优化日志
        optimization_log = {
            "timestamp": time.time(),
            "optimal_bitrate": optimal_bitrate,
            "congestion_window": congestion_window,
            "buffer_stats": buffer_stats,
            "network_state": self.adaptive_bitrate.get_network_state(),
            "congestion_state": self.congestion_controller.get_state()
        }
        
        self.optimization_logs.append(optimization_log)
        
        return optimization_log
    
    def get_optimization_report(self) -> Dict:
        """获取优化报告"""
        runtime = time.time() - self.start_time
        
        return {
            "runtime_seconds": runtime,
            "total_bytes_sent": self.total_bytes_sent,
            "final_bitrate": self.adaptive_bitrate.get_optimal_bitrate(),
            "network_state": self.adaptive_bitrate.get_network_state(),
            "buffer_stats": self.buffer_manager.get_buffer_stats(),
            "optimization_logs_count": len(self.optimization_logs),
            "recent_logs": self.optimization_logs[-10:] if self.optimization_logs else []
        }

# 使用示例
if __name__ == "__main__":
    # 创建RTMP优化器
    optimizer = RTMPOptimizer(initial_bitrate=3000000)
    
    # 模拟网络状态更新
    for i in range(50):
        # 模拟网络变化
        stats = RTMPStats(
            bytes_sent=1000000 + i * 10000,
            bytes_received=500000 + i * 5000,
            packets_sent=1000 + i,
            packets_received=950 + i,
            rtt_ms=50 + 10 * np.sin(i * 0.1),
            throughput_bps=2500000 + 500000 * np.sin(i * 0.2),
            packet_loss_rate=max(0, 0.01 * np.sin(i * 0.3)),
            buffer_level=100 + 50 * np.sin(i * 0.1)
        )
        
        # 执行优化
        result = optimizer.optimize(stats)
        
        if i % 10 == 0:
            print(f"Step {i}: Bitrate={result['optimal_bitrate']}, "
                  f"State={result['network_state']}")
    
    # 获取优化报告
    report = optimizer.get_optimization_report()
    print("\nOptimization Report:")
    print(json.dumps(report, indent=2))