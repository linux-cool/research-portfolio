#!/usr/bin/env python3
"""
FFmpeg Cloud Native Service
基于第一性原理：无状态微服务架构 + 队列解耦 = 线性扩展能力
"""

import os
import asyncio
import logging
from typing import Optional, Dict, Any
from fastapi import FastAPI, HTTPException, BackgroundTasks, UploadFile, File
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
import redis
from celery import Celery
import ffmpeg
import uuid
import json
from datetime import datetime

# 配置日志
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# 初始化应用
app = FastAPI(
    title="FFmpeg Cloud Native Service",
    description="基于FFmpeg的云原生视频处理服务",
    version="1.0.0"
)

# CORS配置
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Redis配置
redis_client = redis.Redis(
    host=os.getenv("REDIS_HOST", "localhost"),
    port=int(os.getenv("REDIS_PORT", 6379)),
    db=0,
    decode_responses=True
)

# Celery配置
celery_app = Celery(
    "ffmpeg_service",
    broker=os.getenv("CELERY_BROKER_URL", "redis://localhost:6379/0"),
    backend=os.getenv("CELERY_RESULT_BACKEND", "redis://localhost:6379/0")
)

# 数据模型
class VideoProcessingRequest(BaseModel):
    input_url: str
    output_format: str = "mp4"
    quality: str = "medium"
    resolution: Optional[str] = None
    bitrate: Optional[str] = None
    metadata: Optional[Dict[str, Any]] = None

class VideoProcessingResponse(BaseModel):
    job_id: str
    status: str
    message: str
    created_at: datetime

class HealthResponse(BaseModel):
    status: str
    timestamp: datetime
    services: Dict[str, str]

# 健康检查端点
@app.get("/health", response_model=HealthResponse)
async def health_check():
    """健康检查端点"""
    services_status = {
        "redis": "ok" if redis_client.ping() else "error",
        "celery": "ok" if celery_app.control.inspect().ping() else "error"
    }
    
    return HealthResponse(
        status="healthy" if all(s == "ok" for s in services_status.values()) else "unhealthy",
        timestamp=datetime.now(),
        services=services_status
    )

@app.get("/ready")
async def readiness_check():
    """就绪检查端点"""
    try:
        # 检查Redis连接
        redis_client.ping()
        return {"status": "ready"}
    except Exception as e:
        logger.error(f"Readiness check failed: {e}")
        raise HTTPException(status_code=503, detail="Service not ready")

# 视频处理端点
@app.post("/process", response_model=VideoProcessingResponse)
async def process_video(
    request: VideoProcessingRequest,
    background_tasks: BackgroundTasks
):
    """提交视频处理任务"""
    job_id = str(uuid.uuid4())
    
    # 创建任务记录
    task_data = {
        "job_id": job_id,
        "request": request.dict(),
        "status": "pending",
        "created_at": datetime.now().isoformat(),
        "updated_at": datetime.now().isoformat()
    }
    
    # 保存到Redis
    redis_client.setex(f"job:{job_id}", 3600, json.dumps(task_data))
    
    # 提交到Celery队列
    process_video_task.delay(job_id, request.dict())
    
    return VideoProcessingResponse(
        job_id=job_id,
        status="accepted",
        message="任务已提交到处理队列",
        created_at=datetime.now()
    )

@app.get("/status/{job_id}")
async def get_job_status(job_id: str):
    """获取任务状态"""
    task_data = redis_client.get(f"job:{job_id}")
    if not task_data:
        raise HTTPException(status_code=404, detail="任务未找到")
    
    return json.loads(task_data)

@app.get("/download/{job_id}")
async def download_result(job_id: str):
    """下载处理结果"""
    task_data = redis_client.get(f"job:{job_id}")
    if not task_data:
        raise HTTPException(status_code=404, detail="任务未找到")
    
    task = json.loads(task_data)
    if task["status"] != "completed":
        raise HTTPException(status_code=400, detail="任务尚未完成")
    
    # 返回下载链接
    return {"download_url": task.get("output_url")}

# 批处理端点
@app.post("/batch/process")
async def process_batch_videos(requests: list[VideoProcessingRequest]):
    """批量处理视频"""
    job_ids = []
    
    for request in requests:
        job_id = str(uuid.uuid4())
        job_ids.append(job_id)
        
        task_data = {
            "job_id": job_id,
            "request": request.dict(),
            "status": "pending",
            "created_at": datetime.now().isoformat()
        }
        
        redis_client.setex(f"job:{job_id}", 3600, json.dumps(task_data))
        process_video_task.delay(job_id, request.dict())
    
    return {"job_ids": job_ids, "message": "批量任务已提交"}

# Celery任务
@celery_app.task(bind=True, max_retries=3)
def process_video_task(self, job_id: str, request_data: dict):
    """处理视频转换任务"""
    try:
        # 更新任务状态
        task_data = json.loads(redis_client.get(f"job:{job_id}"))
        task_data["status"] = "processing"
        task_data["updated_at"] = datetime.now().isoformat()
        redis_client.setex(f"job:{job_id}", 3600, json.dumps(task_data))
        
        # 处理视频
        output_path = f"/data/output/{job_id}.{request_data['output_format']}"
        
        # FFmpeg处理逻辑
        (
            ffmpeg
            .input(request_data["input_url"])
            .output(
                output_path,
                vcodec="libx264",
                acodec="aac",
                preset=request_data.get("quality", "medium"),
                **{k: v for k, v in request_data.items() if k in ["resolution", "bitrate"] and v}
            )
            .run(overwrite_output=True)
        )
        
        # 更新任务状态为完成
        task_data["status"] = "completed"
        task_data["output_url"] = f"/download/{job_id}"
        task_data["updated_at"] = datetime.now().isoformat()
        redis_client.setex(f"job:{job_id}", 3600, json.dumps(task_data))
        
        logger.info(f"Job {job_id} completed successfully")
        
    except Exception as exc:
        logger.error(f"Job {job_id} failed: {exc}")
        
        # 更新任务状态为失败
        task_data = json.loads(redis_client.get(f"job:{job_id}"))
        task_data["status"] = "failed"
        task_data["error"] = str(exc)
        task_data["updated_at"] = datetime.now().isoformat()
        redis_client.setex(f"job:{job_id}", 3600, json.dumps(task_data))
        
        # 重试机制
        raise self.retry(exc=exc, countdown=60)

# 指标端点
@app.get("/metrics")
async def get_metrics():
    """获取Prometheus指标"""
    from prometheus_client import generate_latest, CONTENT_TYPE_LATEST
    return Response(generate_latest(), media_type=CONTENT_TYPE_LATEST)

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8080)