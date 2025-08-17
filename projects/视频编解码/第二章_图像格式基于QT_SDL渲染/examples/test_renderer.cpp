#include "xvideo_view.h"
#include <iostream>
#include <vector>
#include <cmath>

#ifdef ENABLE_QT
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QTimer>
#include <QMainWindow>
#include <QWidget>
#include <QStatusBar>

class RendererTestWindow : public QMainWindow {
    Q_OBJECT
    
public:
    RendererTestWindow(QWidget* parent = nullptr) : QMainWindow(parent) {
        setupUI();
        setupConnections();
        
        // 创建测试数据生成定时器
        test_timer_ = new QTimer(this);
        connect(test_timer_, &QTimer::timeout, this, &RendererTestWindow::generateTestFrame);
        
        frame_count_ = 0;
    }
    
private slots:
    void onRendererTypeChanged() {
        QString type = renderer_combo_->currentText();
        LOG_INFO("Switching to renderer: %s", type.toStdString().c_str());
        
        // 停止当前渲染
        if (test_timer_->isActive()) {
            test_timer_->stop();
        }
        
        // 创建新的渲染器
        createRenderer();
    }
    
    void onStartStop() {
        if (test_timer_->isActive()) {
            test_timer_->stop();
            start_button_->setText("Start");
            statusBar()->showMessage("Stopped");
        } else {
            if (!renderer_ || !renderer_->IsInitialized()) {
                createRenderer();
            }
            
            int fps = fps_spinbox_->value();
            test_timer_->start(1000 / fps);
            start_button_->setText("Stop");
            statusBar()->showMessage("Running");
        }
    }
    
    void onFpsChanged() {
        int fps = fps_spinbox_->value();
        if (renderer_) {
            renderer_->SetTargetFPS(fps);
        }
        
        if (test_timer_->isActive()) {
            test_timer_->setInterval(1000 / fps);
        }
    }
    
    void onAntiAliasingChanged() {
        if (renderer_) {
            renderer_->SetAntiAliasing(antialias_checkbox_->isChecked());
        }
    }
    
    void generateTestFrame() {
        if (!renderer_ || !renderer_->IsInitialized()) {
            return;
        }
        
        // 生成彩色测试图案
        generateColorBars();
        
        // 渲染帧
        uint8_t* data[4] = {test_frame_.data(), nullptr, nullptr, nullptr};
        int linesize[4] = {width_ * 3, 0, 0, 0};
        
        renderer_->Render(data, linesize);
        
        // 更新状态
        frame_count_++;
        double fps = renderer_->GetFPS();
        statusBar()->showMessage(QString("Frame: %1, FPS: %2").arg(frame_count_).arg(fps, 0, 'f', 1));
    }
    
private:
    void setupUI() {
        auto* central_widget = new QWidget;
        setCentralWidget(central_widget);
        
        auto* main_layout = new QVBoxLayout(central_widget);
        
        // 控制面板
        auto* control_layout = new QHBoxLayout;
        
        // 渲染器选择
        control_layout->addWidget(new QLabel("Renderer:"));
        renderer_combo_ = new QComboBox;
        auto available_types = XVideoViewFactory::GetAvailableTypes();
        for (auto type : available_types) {
            renderer_combo_->addItem(QString::fromStdString(XVideoViewFactory::TypeToString(type)));
        }
        control_layout->addWidget(renderer_combo_);
        
        // FPS控制
        control_layout->addWidget(new QLabel("FPS:"));
        fps_spinbox_ = new QSpinBox;
        fps_spinbox_->setRange(1, 120);
        fps_spinbox_->setValue(25);
        control_layout->addWidget(fps_spinbox_);
        
        // 抗锯齿
        antialias_checkbox_ = new QCheckBox("Anti-aliasing");
        antialias_checkbox_->setChecked(true);
        control_layout->addWidget(antialias_checkbox_);
        
        // 开始/停止按钮
        start_button_ = new QPushButton("Start");
        control_layout->addWidget(start_button_);
        
        control_layout->addStretch();
        main_layout->addLayout(control_layout);
        
        // 渲染区域容器
        renderer_container_ = new QWidget;
        renderer_container_->setMinimumSize(640, 480);
        renderer_container_->setStyleSheet("background-color: black;");
        main_layout->addWidget(renderer_container_);
        
        // 状态栏
        statusBar()->showMessage("Ready");
        
        setWindowTitle("Video Renderer Test");
        resize(800, 600);
    }
    
    void setupConnections() {
        connect(renderer_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &RendererTestWindow::onRendererTypeChanged);
        connect(start_button_, &QPushButton::clicked, this, &RendererTestWindow::onStartStop);
        connect(fps_spinbox_, QOverload<int>::of(&QSpinBox::valueChanged),
                this, &RendererTestWindow::onFpsChanged);
        connect(antialias_checkbox_, &QCheckBox::toggled, this, &RendererTestWindow::onAntiAliasingChanged);
    }
    
    void createRenderer() {
        // 清理旧的渲染器
        if (renderer_) {
            renderer_->Close();
            renderer_.reset();
        }
        
        // 创建新的渲染器
        QString type_str = renderer_combo_->currentText();
        XVideoViewFactory::RendererType type = XVideoViewFactory::RendererType::AUTO;
        
        if (type_str == "Qt") {
            type = XVideoViewFactory::RendererType::QT;
        } else if (type_str == "SDL") {
            type = XVideoViewFactory::RendererType::SDL;
        }
        
        renderer_ = XVideoViewFactory::Create(type, renderer_container_);
        if (!renderer_) {
            statusBar()->showMessage("Failed to create renderer");
            return;
        }
        
        // 初始化渲染器
        width_ = 640;
        height_ = 480;
        
        if (!renderer_->Init(width_, height_, PixelFormat::RGB24)) {
            statusBar()->showMessage("Failed to initialize renderer");
            renderer_.reset();
            return;
        }
        
        // 设置参数
        renderer_->SetTargetFPS(fps_spinbox_->value());
        renderer_->SetAntiAliasing(antialias_checkbox_->isChecked());
        
        // 分配测试帧缓冲区
        test_frame_.resize(width_ * height_ * 3);
        
        statusBar()->showMessage("Renderer created successfully");
    }
    
    void generateColorBars() {
        // 生成彩色条纹测试图案
        const int bar_width = width_ / 8;
        const uint8_t colors[8][3] = {
            {255, 255, 255}, // 白色
            {255, 255, 0},   // 黄色
            {0, 255, 255},   // 青色
            {0, 255, 0},     // 绿色
            {255, 0, 255},   // 品红
            {255, 0, 0},     // 红色
            {0, 0, 255},     // 蓝色
            {0, 0, 0}        // 黑色
        };
        
        for (int y = 0; y < height_; ++y) {
            for (int x = 0; x < width_; ++x) {
                int bar_index = x / bar_width;
                if (bar_index >= 8) bar_index = 7;
                
                int pixel_index = (y * width_ + x) * 3;
                test_frame_[pixel_index + 0] = colors[bar_index][0];     // R
                test_frame_[pixel_index + 1] = colors[bar_index][1];     // G
                test_frame_[pixel_index + 2] = colors[bar_index][2];     // B
                
                // 添加动态效果
                if (frame_count_ % 50 < 25) {
                    // 每25帧闪烁一次
                    int brightness = static_cast<int>(128 + 127 * sin(frame_count_ * 0.1));
                    test_frame_[pixel_index + 0] = (test_frame_[pixel_index + 0] * brightness) / 255;
                    test_frame_[pixel_index + 1] = (test_frame_[pixel_index + 1] * brightness) / 255;
                    test_frame_[pixel_index + 2] = (test_frame_[pixel_index + 2] * brightness) / 255;
                }
            }
        }
    }
    
    QComboBox* renderer_combo_;
    QSpinBox* fps_spinbox_;
    QCheckBox* antialias_checkbox_;
    QPushButton* start_button_;
    QWidget* renderer_container_;
    QTimer* test_timer_;
    
    std::unique_ptr<XVideoView> renderer_;
    std::vector<uint8_t> test_frame_;
    int width_, height_;
    int frame_count_;
};

#include "test_renderer.moc"
#endif // ENABLE_QT

int main(int argc, char* argv[]) {
    LOG_INFO("Starting renderer test application");
    
#ifdef ENABLE_QT
    QApplication app(argc, argv);
    
    RendererTestWindow window;
    window.show();
    
    return app.exec();
#else
    LOG_ERROR("Qt support not enabled, cannot run GUI test");
    
    // 简单的SDL测试
#ifdef ENABLE_SDL
    auto renderer = XVideoViewFactory::Create(XVideoViewFactory::RendererType::SDL);
    if (!renderer) {
        LOG_ERROR("Failed to create SDL renderer");
        return -1;
    }
    
    const int width = 640, height = 480;
    if (!renderer->Init(width, height, PixelFormat::RGB24)) {
        LOG_ERROR("Failed to initialize renderer");
        return -1;
    }
    
    // 生成测试数据
    std::vector<uint8_t> test_frame(width * height * 3);
    for (int i = 0; i < width * height; ++i) {
        test_frame[i * 3 + 0] = (i % 256);      // R
        test_frame[i * 3 + 1] = ((i / 256) % 256); // G
        test_frame[i * 3 + 2] = 128;            // B
    }
    
    uint8_t* data[4] = {test_frame.data(), nullptr, nullptr, nullptr};
    int linesize[4] = {width * 3, 0, 0, 0};
    
    // 渲染循环
    auto sdl_renderer = static_cast<SDLVideoView*>(renderer.get());
    for (int frame = 0; frame < 300; ++frame) {
        if (!sdl_renderer->HandleEvents()) {
            break;
        }
        
        renderer->Render(data, linesize);
        Utils::SleepMs(40); // 25 FPS
    }
    
    renderer->Close();
    LOG_INFO("SDL test completed");
#else
    LOG_ERROR("No renderer support available");
    return -1;
#endif
    
    return 0;
#endif
}
