#!/usr/bin/env python3
"""
实时目标检测器
基于第一性原理：特征提取 + 分类器 = 目标识别
"""

import cv2
import numpy as np
import logging
import time
from typing import List, Dict, Tuple, Optional
from dataclasses import dataclass
import json
import threading
from queue import Queue
import torch
import torchvision
from torchvision.models.detection import fasterrcnn_resnet50_fpn, ssdlite320_mobilenet_v3_large

@dataclass
class DetectionResult:
    """检测结果"""
    bbox: Tuple[int, int, int, int]  # x1, y1, x2, y2
    confidence: float
    class_id: int
    class_name: str
    timestamp: float

class FrameBuffer:
    """帧缓冲区管理"""
    
    def __init__(self, max_size: int = 30):
        self.max_size = max_size
        self.buffer = []
        self.lock = threading.Lock()
    
    def add_frame(self, frame: np.ndarray, timestamp: float):
        """添加帧到缓冲区"""
        with self.lock:
            self.buffer.append({"frame": frame, "timestamp": timestamp})
            if len(self.buffer) > self.max_size:
                self.buffer.pop(0)
    
    def get_latest_frame(self) -> Optional[Dict]:
        """获取最新帧"""
        with self.lock:
            return self.buffer[-1] if self.buffer else None
    
    def get_frame_count(self) -> int:
        """获取帧数量"""
        with self.lock:
            return len(self.buffer)

class RealTimeDetector:
    """实时目标检测器"""
    
    def __init__(self, 
                 model_name: str = "faster_rcnn",
                 confidence_threshold: float = 0.5,
                 device: str = "auto"):
        
        self.model_name = model_name
        self.confidence_threshold = confidence_threshold
        self.device = self._setup_device(device)
        
        # 初始化模型
        self.model = self._load_model()
        self.model.eval()
        
        # COCO类别名称
        self.class_names = [
            'background', 'person', 'bicycle', 'car', 'motorcycle', 'airplane',
            'bus', 'train', 'truck', 'boat', 'traffic light', 'fire hydrant',
            'stop sign', 'parking meter', 'bench', 'bird', 'cat', 'dog', 'horse',
            'sheep', 'cow', 'elephant', 'bear', 'zebra', 'giraffe', 'backpack',
            'umbrella', 'handbag', 'tie', 'suitcase', 'frisbee', 'skis',
            'snowboard', 'sports ball', 'kite', 'baseball bat', 'baseball glove',
            'skateboard', 'surfboard', 'tennis racket', 'bottle', 'wine glass',
            'cup', 'fork', 'knife', 'spoon', 'bowl', 'banana', 'apple',
            'sandwich', 'orange', 'broccoli', 'carrot', 'hot dog', 'pizza',
            'donut', 'cake', 'chair', 'couch', 'potted plant', 'bed',
            'dining table', 'toilet', 'tv', 'laptop', 'mouse', 'remote',
            'keyboard', 'cell phone', 'microwave', 'oven', 'toaster', 'sink',
            'refrigerator', 'book', 'clock', 'vase', 'scissors', 'teddy bear',
            'hair drier', 'toothbrush'
        ]
        
        # 帧缓冲区
        self.frame_buffer = FrameBuffer()
        
        # 性能统计
        self.fps_counter = 0
        self.total_frames = 0
        self.total_processing_time = 0
        self.detections = []
        
        # 线程安全
        self.lock = threading.Lock()
        self.running = False
        
    def _setup_device(self, device: str) -> torch.device:
        """设置计算设备"""
        if device == "auto":
            if torch.cuda.is_available():
                return torch.device("cuda")
            elif hasattr(torch.backends, 'mps') and torch.backends.mps.is_available():
                return torch.device("mps")
            else:
                return torch.device("cpu")
        return torch.device(device)
    
    def _load_model(self):
        """加载模型"""
        if self.model_name == "faster_rcnn":
            model = fasterrcnn_resnet50_fpn(pretrained=True)
        elif self.model_name == "ssd":
            model = ssdlite320_mobilenet_v3_large(pretrained=True)
        else:
            raise ValueError(f"不支持的模型: {self.model_name}")
        
        return model.to(self.device)
    
    def predict(self, frame: np.ndarray) -> List[DetectionResult]:
        """执行目标检测"""
        start_time = time.time()
        
        # 预处理
        input_tensor = self._preprocess_frame(frame)
        
        # 推理
        with torch.no_grad():
            predictions = self.model([input_tensor])[0]
        
        # 后处理
        results = self._postprocess_predictions(predictions, frame.shape)
        
        # 更新统计
        processing_time = time.time() - start_time
        with self.lock:
            self.total_frames += 1
            self.total_processing_time += processing_time
            self.detections.extend(results)
        
        return results
    
    def _preprocess_frame(self, frame: np.ndarray) -> torch.Tensor:
        """预处理帧"""
        # 转换颜色空间
        frame_rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        
        # 转换为tensor
        frame_tensor = torch.from_numpy(frame_rgb).permute(2, 0, 1).float() / 255.0
        
        return frame_tensor.to(self.device)
    
    def _postprocess_predictions(self, 
                               predictions: Dict, 
                               frame_shape: Tuple) -> List[DetectionResult]:
        """后处理预测结果"""
        boxes = predictions['boxes'].cpu().numpy()
        scores = predictions['scores'].cpu().numpy()
        labels = predictions['labels'].cpu().numpy()
        
        results = []
        height, width = frame_shape[:2]
        
        for box, score, label in zip(boxes, scores, labels):
            if score > self.confidence_threshold:
                x1, y1, x2, y2 = box.astype(int)
                
                # 确保边界在图像范围内
                x1 = max(0, min(x1, width))
                y1 = max(0, min(y1, height))
                x2 = max(0, min(x2, width))
                y2 = max(0, min(y2, height))
                
                class_name = self.class_names[label] if label < len(self.class_names) else f"class_{label}"
                
                results.append(DetectionResult(
                    bbox=(x1, y1, x2, y2),
                    confidence=float(score),
                    class_id=int(label),
                    class_name=class_name,
                    timestamp=time.time()
                ))
        
        return results
    
    def draw_detections(self, 
                       frame: np.ndarray, 
                       detections: List[DetectionResult]) -> np.ndarray:
        """绘制检测结果"""
        result_frame = frame.copy()
        
        for detection in detections:
            x1, y1, x2, y2 = detection.bbox
            
            # 绘制边界框
            cv2.rectangle(result_frame, (x1, y1), (x2, y2), (0, 255, 0), 2)
            
            # 绘制标签
            label = f"{detection.class_name}: {detection.confidence:.2f}"
            label_size = cv2.getTextSize(label, cv2.FONT_HERSHEY_SIMPLEX, 0.5, 2)[0]
            
            cv2.rectangle(result_frame, 
                        (x1, y1 - label_size[1] - 10), 
                        (x1 + label_size[0], y1), 
                        (0, 255, 0), 
                        -1)
            
            cv2.putText(result_frame, 
                       label, 
                       (x1, y1 - 5), 
                       cv2.FONT_HERSHEY_SIMPLEX, 
                       0.5, 
                       (0, 0, 0), 
                       2)
        
        return result_frame
    
    def process_video_file(self, 
                          video_path: str, 
                          output_path: str,
                          skip_frames: int = 1) -> Dict:
        """处理视频文件"""
        cap = cv2.VideoCapture(video_path)
        
        if not cap.isOpened():
            return {"error": "无法打开视频文件"}
        
        # 获取视频信息
        fps = int(cap.get(cv2.CAP_PROP_FPS))
        width = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
        height = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
        total_frames = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
        
        # 视频写入器
        fourcc = cv2.VideoWriter_fourcc(*'mp4v')
        out = cv2.VideoWriter(output_path, fourcc, fps, (width, height))
        
        frame_count = 0
        all_detections = []
        
        while True:
            ret, frame = cap.read()
            if not ret:
                break
            
            if frame_count % skip_frames == 0:
                detections = self.predict(frame)
                result_frame = self.draw_detections(frame, detections)
                
                # 收集检测信息
                frame_detections = {
                    "frame": frame_count,
                    "detections": [
                        {
                            "bbox": det.bbox,
                            "confidence": det.confidence,
                            "class_id": det.class_id,
                            "class_name": det.class_name
                        }
                        for det in detections
                    ]
                }
                all_detections.append(frame_detections)
            
            out.write(result_frame)
            frame_count += 1
            
            if frame_count % 100 == 0:
                print(f"处理进度: {frame_count}/{total_frames}")
        
        cap.release()
        out.release()
        
        return {
            "total_frames": total_frames,
            "processed_frames": frame_count,
            "detections": all_detections,
            "average_fps": self.get_average_fps()
        }
    
    def start_real_time_detection(self, 
                                 video_source: int = 0,
                                 display: bool = True) -> None:
        """启动实时检测"""
        cap = cv2.VideoCapture(video_source)
        
        if not cap.isOpened():
            print("无法打开摄像头")
            return
        
        self.running = True
        
        def detect_loop():
            while self.running:
                ret, frame = cap.read()
                if not ret:
                    break
                
                detections = self.predict(frame)
                
                if display:
                    result_frame = self.draw_detections(frame, detections)
                    
                    # 显示FPS
                    fps = self.get_average_fps()
                    cv2.putText(result_frame, 
                               f"FPS: {fps:.1f}", 
                               (10, 30), 
                               cv2.FONT_HERSHEY_SIMPLEX, 
                               1, 
                               (0, 255, 0), 
                               2)
                    
                    cv2.imshow("Real-time Detection", result_frame)
                    
                    if cv2.waitKey(1) & 0xFF == ord('q'):
                        break
        
        detect_loop()
        
        cap.release()
        cv2.destroyAllWindows()
    
    def stop_real_time_detection(self):
        """停止实时检测"""
        self.running = False
    
    def get_average_fps(self) -> float:
        """获取平均FPS"""
        with self.lock:
            if self.total_frames == 0:
                return 0.0
            return 1.0 / (self.total_processing_time / self.total_frames)
    
    def get_detection_statistics(self) -> Dict:
        """获取检测统计"""
        with self.lock:
            if not self.detections:
                return {"message": "暂无检测数据"}
            
            # 按类别统计
            class_counts = {}
            confidence_scores = []
            
            for detection in self.detections:
                class_name = detection.class_name
                class_counts[class_name] = class_counts.get(class_name, 0) + 1
                confidence_scores.append(detection.confidence)
            
            return {
                "total_detections": len(self.detections),
                "unique_classes": len(class_counts),
                "class_distribution": class_counts,
                "average_confidence": np.mean(confidence_scores),
                "min_confidence": np.min(confidence_scores),
                "max_confidence": np.max(confidence_scores),
                "average_fps": self.get_average_fps()
            }

class DetectionTracker:
    """目标跟踪器"""
    
    def __init__(self, max_distance: int = 50, max_frames_to_skip: int = 10):
        self.max_distance = max_distance
        self.max_frames_to_skip = max_frames_to_skip
        self.tracks = []
        self.track_id = 0
    
    def update(self, detections: List[DetectionResult]) -> List[Dict]:
        """更新跟踪"""
        if not self.tracks:
            # 初始化跟踪
            for detection in detections:
                self.tracks.append({
                    "id": self.track_id,
                    "bbox": detection.bbox,
                    "class_name": detection.class_name,
                    "confidence": detection.confidence,
                    "frames_since_update": 0,
                    "history": [detection.bbox]
                })
                self.track_id += 1
            return self.tracks
        
        # 计算距离矩阵
        tracks_centers = [self._get_center(track["bbox"]) for track in self.tracks]
        detections_centers = [self._get_center(det.bbox) for det in detections]
        
        # 匈牙利算法匹配
        # 这里简化实现，实际应使用scipy的linear_sum_assignment
        matched_indices = self._simple_matching(tracks_centers, detections_centers)
        
        # 更新跟踪
        updated_tracks = []
        for track_idx, det_idx in matched_indices:
            if det_idx is not None:
                self.tracks[track_idx]["bbox"] = detections[det_idx].bbox
                self.tracks[track_idx]["confidence"] = detections[det_idx].confidence
                self.tracks[track_idx]["frames_since_update"] = 0
                self.tracks[track_idx]["history"].append(detections[det_idx].bbox)
                updated_tracks.append(self.tracks[track_idx])
            else:
                self.tracks[track_idx]["frames_since_update"] += 1
                if self.tracks[track_idx]["frames_since_update"] <= self.max_frames_to_skip:
                    updated_tracks.append(self.tracks[track_idx])
        
        # 添加新检测
        unmatched_detections = [i for i in range(len(detections)) 
                              if i not in [idx for _, idx in matched_indices]]
        for det_idx in unmatched_detections:
            updated_tracks.append({
                "id": self.track_id,
                "bbox": detections[det_idx].bbox,
                "class_name": detections[det_idx].class_name,
                "confidence": detections[det_idx].confidence,
                "frames_since_update": 0,
                "history": [detections[det_idx].bbox]
            })
            self.track_id += 1
        
        self.tracks = updated_tracks
        return self.tracks
    
    def _get_center(self, bbox: Tuple[int, int, int, int]) -> Tuple[int, int]:
        """获取边界框中心"""
        x1, y1, x2, y2 = bbox
        return ((x1 + x2) // 2, (y1 + y2) // 2)
    
    def _simple_matching(self, 
                        tracks_centers: List[Tuple[int, int]], 
                        detections_centers: List[Tuple[int, int]]) -> List[Tuple[int, Optional[int]]]:
        """简单匹配算法"""
        matched = []
        used_detections = set()
        
        for i, track_center in enumerate(tracks_centers):
            best_match = None
            best_distance = float('inf')
            
            for j, det_center in enumerate(detections_centers):
                if j in used_detections:
                    continue
                
                distance = np.sqrt((track_center[0] - det_center[0]) ** 2 + 
                                 (track_center[1] - det_center[1]) ** 2)
                
                if distance < self.max_distance and distance < best_distance:
                    best_distance = distance
                    best_match = j
            
            if best_match is not None:
                used_detections.add(best_match)
                matched.append((i, best_match))
            else:
                matched.append((i, None))
        
        return matched

# 使用示例
if __name__ == "__main__":
    # 创建检测器
    detector = RealTimeDetector(
        model_name="faster_rcnn",
        confidence_threshold=0.7
    )
    
    # 处理视频文件
    result = detector.process_video_file(
        "input.mp4",
        "output_detected.mp4",
        skip_frames=1
    )
    
    print("检测完成:")
    print(json.dumps(result, indent=2))