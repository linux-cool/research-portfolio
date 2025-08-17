#include "xvideo_view.h"

#ifdef ENABLE_QT
#include <QApplication>
#include <QPaintEvent>
#include <QResizeEvent>

QtVideoView::QtVideoView(QWidget* parent)
    : QWidget(parent)
    , display_label_(nullptr)
    , refresh_timer_(nullptr)
    , sws_context_(nullptr, sws_freeContext) {
    
    setMinimumSize(320, 240);
    setWindowTitle("Qt Video Renderer");
    
    // 创建显示标签
    display_label_ = new QLabel(this);
    display_label_->setAlignment(Qt::AlignCenter);
    display_label_->setStyleSheet("background-color: black;");
    display_label_->setText("No Video");
    
    // 创建刷新定时器
    refresh_timer_ = new QTimer(this);
    connect(refresh_timer_, &QTimer::timeout, this, &QtVideoView::OnRefreshTimer);
}

QtVideoView::~QtVideoView() {
    Close();
}

bool QtVideoView::Init(int width, int height, PixelFormat format) {
    if (width <= 0 || height <= 0) {
        LOG_ERROR("Invalid video dimensions: %dx%d", width, height);
        return false;
    }
    
    std::lock_guard<std::mutex> lock(render_mutex_);
    
    width_ = width;
    height_ = height;
    format_ = format;
    
    // 创建像素格式转换上下文
    AVPixelFormat src_format = Utils::ToAVPixelFormat(format);
    if (src_format == AV_PIX_FMT_NONE) {
        LOG_ERROR("Unsupported pixel format");
        return false;
    }
    
    SwsContext* sws_ctx = sws_getContext(
        width, height, src_format,
        width, height, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );
    
    if (!sws_ctx) {
        LOG_ERROR("Failed to create sws context");
        return false;
    }
    
    sws_context_.reset(sws_ctx);
    
    // 分配RGB缓冲区
    int rgb_size = av_image_get_buffer_size(AV_PIX_FMT_RGB24, width, height, 1);
    rgb_buffer_.resize(rgb_size);
    
    // 调整窗口大小
    resize(width, height);
    display_label_->resize(width, height);
    
    initialized_ = true;
    
    // 启动刷新定时器
    int interval = static_cast<int>(1000.0 / target_fps_);
    refresh_timer_->start(interval);
    
    LOG_INFO("Qt renderer initialized: %dx%d, format=%d", width, height, static_cast<int>(format));
    return true;
}

bool QtVideoView::Render(uint8_t* data[], int linesize[]) {
    if (!initialized_ || !data || !linesize) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(render_mutex_);
    
    if (!ConvertToRGB(data, linesize)) {
        return false;
    }
    
    UpdateFPS();
    return true;
}

bool QtVideoView::Render(AVFrame* frame) {
    if (!frame) {
        return false;
    }
    
    return Render(frame->data, frame->linesize);
}

void QtVideoView::Close() {
    if (refresh_timer_) {
        refresh_timer_->stop();
    }
    
    std::lock_guard<std::mutex> lock(render_mutex_);
    sws_context_.reset();
    rgb_buffer_.clear();
    initialized_ = false;
    
    LOG_INFO("Qt renderer closed");
}

bool QtVideoView::Resize(int width, int height) {
    if (width <= 0 || height <= 0) {
        return false;
    }
    
    width_ = width;
    height_ = height;
    
    resize(width, height);
    if (display_label_) {
        display_label_->resize(width, height);
    }
    
    return true;
}

void QtVideoView::SetAntiAliasing(bool enable) {
    anti_aliasing_ = enable;
}

double QtVideoView::GetFPS() const {
    std::lock_guard<std::mutex> lock(fps_mutex_);
    return current_fps_;
}

void QtVideoView::SetTargetFPS(double fps) {
    target_fps_ = fps;
    if (refresh_timer_ && refresh_timer_->isActive()) {
        int interval = static_cast<int>(1000.0 / fps);
        refresh_timer_->setInterval(interval);
    }
}

void QtVideoView::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    
    QPainter painter(this);
    if (anti_aliasing_) {
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
    }
    
    if (!current_pixmap_.isNull()) {
        QRect target_rect = rect();
        QRect source_rect = current_pixmap_.rect();
        
        // 保持宽高比
        QSize scaled_size = source_rect.size().scaled(target_rect.size(), Qt::KeepAspectRatio);
        QRect scaled_rect((target_rect.width() - scaled_size.width()) / 2,
                         (target_rect.height() - scaled_size.height()) / 2,
                         scaled_size.width(), scaled_size.height());
        
        painter.drawPixmap(scaled_rect, current_pixmap_, source_rect);
    } else {
        painter.fillRect(rect(), Qt::black);
        painter.setPen(Qt::white);
        painter.drawText(rect(), Qt::AlignCenter, "No Video");
    }
}

void QtVideoView::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (display_label_) {
        display_label_->resize(event->size());
    }
}

void QtVideoView::OnRefreshTimer() {
    UpdateDisplay();
}

bool QtVideoView::ConvertToRGB(uint8_t* data[], int linesize[]) {
    if (!sws_context_ || rgb_buffer_.empty()) {
        return false;
    }
    
    uint8_t* dst_data[4] = {rgb_buffer_.data(), nullptr, nullptr, nullptr};
    int dst_linesize[4] = {width_ * 3, 0, 0, 0};
    
    int ret = sws_scale(sws_context_.get(),
                       data, linesize,
                       0, height_,
                       dst_data, dst_linesize);
    
    if (ret != height_) {
        LOG_ERROR("sws_scale failed: %d", ret);
        return false;
    }
    
    return true;
}

void QtVideoView::UpdateDisplay() {
    std::lock_guard<std::mutex> lock(render_mutex_);
    
    if (rgb_buffer_.empty()) {
        return;
    }
    
    QImage image(rgb_buffer_.data(), width_, height_, width_ * 3, QImage::Format_RGB888);
    current_pixmap_ = QPixmap::fromImage(image);
    
    update(); // 触发重绘
}

#endif // ENABLE_QT
