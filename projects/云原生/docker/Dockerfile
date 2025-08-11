FROM ubuntu:22.04

# 设置时区
ENV TZ=UTC
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

# 安装依赖
RUN apt-get update && apt-get install -y \
    ffmpeg \
    python3 \
    python3-pip \
    curl \
    wget \
    supervisor \
    nginx \
    && rm -rf /var/lib/apt/lists/*

# 创建应用目录
WORKDIR /app

# 复制应用文件
COPY requirements.txt .
COPY src/ ./src/
COPY config/ ./config/
COPY scripts/ ./scripts/

# 安装Python依赖
RUN pip3 install --no-cache-dir -r requirements.txt

# 创建数据目录
RUN mkdir -p /data/input /data/output /data/logs

# 设置权限
RUN chmod +x /app/scripts/*.sh

# 暴露端口
EXPOSE 8080

# 健康检查
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD curl -f http://localhost:8080/health || exit 1

# 启动命令
CMD ["/app/scripts/start.sh"]