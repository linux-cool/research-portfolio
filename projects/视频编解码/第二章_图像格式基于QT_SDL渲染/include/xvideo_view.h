#pragma once

#include "common.h"
#include <memory>

/**
 * @brief 视频渲染接口基类
 * 
 * 定义了统一的视频渲染接口，支持多种渲染后端（Qt、SDL等）
 */
class XVideoView {
public:
    virtual ~XVideoView() = default;
    
    /**
     * @brief 初始化渲染器
     * @param width 视频宽度
     * @param height 视频高度
     * @param format 像素格式
     * @return 成功返回true，失败返回false
     */
    virtual bool Init(int width, int height, PixelFormat format) = 0;
    
    /**
     * @brief 渲染一帧数据
     * @param data 图像数据指针数组
     * @param linesize 每行字节数数组
     * @return 成功返回true，失败返回false
     */
    virtual bool Render(uint8_t* data[], int linesize[]) = 0;
    
    /**
     * @brief 渲染AVFrame
     * @param frame AVFrame指针
     * @return 成功返回true，失败返回false
     */
    virtual bool Render(AVFrame* frame) = 0;
    
    /**
     * @brief 清理资源
     */
    virtual void Close() = 0;
    
    /**
     * @brief 调整窗口大小
     * @param width 新宽度
     * @param height 新高度
     * @return 成功返回true，失败返回false
     */
    virtual bool Resize(int width, int height) = 0;
    
    /**
     * @brief 设置抗锯齿
     * @param enable 是否启用抗锯齿
     */
    virtual void SetAntiAliasing(bool enable) = 0;
    
    /**
     * @brief 获取渲染器类型
     * @return 渲染器类型字符串
     */
    virtual std::string GetType() const = 0;
    
    /**
     * @brief 检查是否已初始化
     * @return 已初始化返回true，否则返回false
     */
    virtual bool IsInitialized() const = 0;
    
    /**
     * @brief 获取当前帧率
     * @return 当前帧率
     */
    virtual double GetFPS() const = 0;
    
    /**
     * @brief 设置目标帧率
     * @param fps 目标帧率
     */
    virtual void SetTargetFPS(double fps) = 0;

protected:
    int width_ = 0;
    int height_ = 0;
    PixelFormat format_ = PixelFormat::UNKNOWN;
    bool initialized_ = false;
    bool anti_aliasing_ = true;
    double target_fps_ = 25.0;
    
    // 帧率统计
    mutable std::mutex fps_mutex_;
    mutable int64_t last_render_time_ = 0;
    mutable double current_fps_ = 0.0;
    mutable int frame_count_ = 0;
    mutable int64_t fps_start_time_ = 0;
    
    /**
     * @brief 更新帧率统计
     */
    void UpdateFPS() const {
        std::lock_guard<std::mutex> lock(fps_mutex_);
        int64_t current_time = Utils::GetCurrentTimeMs();
        
        if (fps_start_time_ == 0) {
            fps_start_time_ = current_time;
            frame_count_ = 0;
        }
        
        frame_count_++;
        
        // 每秒更新一次帧率
        if (current_time - fps_start_time_ >= 1000) {
            current_fps_ = frame_count_ * 1000.0 / (current_time - fps_start_time_);
            fps_start_time_ = current_time;
            frame_count_ = 0;
        }
        
        last_render_time_ = current_time;
    }
};

/**
 * @brief 渲染器工厂类
 */
class XVideoViewFactory {
public:
    enum class RendererType {
        AUTO,    // 自动选择
        QT,      // Qt渲染器
        SDL,     // SDL渲染器
        OPENGL   // OpenGL渲染器
    };
    
    /**
     * @brief 创建渲染器实例
     * @param type 渲染器类型
     * @param parent 父窗口指针（Qt专用）
     * @return 渲染器实例，失败返回nullptr
     */
    static std::unique_ptr<XVideoView> Create(RendererType type = RendererType::AUTO, void* parent = nullptr);
    
    /**
     * @brief 获取可用的渲染器类型列表
     * @return 可用渲染器类型列表
     */
    static std::vector<RendererType> GetAvailableTypes();
    
    /**
     * @brief 渲染器类型转字符串
     * @param type 渲染器类型
     * @return 类型字符串
     */
    static std::string TypeToString(RendererType type);
};

#ifdef ENABLE_QT
#include <QWidget>
#include <QLabel>
#include <QTimer>
#include <QPixmap>
#include <QPainter>

/**
 * @brief Qt渲染器实现
 */
class QtVideoView : public QWidget, public XVideoView {
    Q_OBJECT
    
public:
    explicit QtVideoView(QWidget* parent = nullptr);
    virtual ~QtVideoView();
    
    // XVideoView接口实现
    bool Init(int width, int height, PixelFormat format) override;
    bool Render(uint8_t* data[], int linesize[]) override;
    bool Render(AVFrame* frame) override;
    void Close() override;
    bool Resize(int width, int height) override;
    void SetAntiAliasing(bool enable) override;
    std::string GetType() const override { return "Qt"; }
    bool IsInitialized() const override { return initialized_; }
    double GetFPS() const override;
    void SetTargetFPS(double fps) override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void OnRefreshTimer();

private:
    bool ConvertToRGB(uint8_t* data[], int linesize[]);
    void UpdateDisplay();
    
    QLabel* display_label_;
    QTimer* refresh_timer_;
    QPixmap current_pixmap_;
    SwsContextPtr sws_context_;
    std::vector<uint8_t> rgb_buffer_;
    std::mutex render_mutex_;
};
#endif // ENABLE_QT

#ifdef ENABLE_SDL
#include <SDL2/SDL.h>

/**
 * @brief SDL渲染器实现
 */
class SDLVideoView : public XVideoView {
public:
    SDLVideoView();
    virtual ~SDLVideoView();
    
    // XVideoView接口实现
    bool Init(int width, int height, PixelFormat format) override;
    bool Render(uint8_t* data[], int linesize[]) override;
    bool Render(AVFrame* frame) override;
    void Close() override;
    bool Resize(int width, int height) override;
    void SetAntiAliasing(bool enable) override;
    std::string GetType() const override { return "SDL"; }
    bool IsInitialized() const override { return initialized_; }
    double GetFPS() const override;
    void SetTargetFPS(double fps) override;
    
    /**
     * @brief 处理SDL事件
     * @return 是否应该继续运行
     */
    bool HandleEvents();
    
    /**
     * @brief 设置窗口标题
     * @param title 窗口标题
     */
    void SetWindowTitle(const std::string& title);

private:
    bool CreateWindow();
    bool CreateRenderer();
    bool CreateTexture();
    void DestroyResources();
    
    SDL_Window* window_;
    SDL_Renderer* renderer_;
    SDL_Texture* texture_;
    SwsContextPtr sws_context_;
    std::vector<uint8_t> yuv_buffer_;
    std::mutex render_mutex_;
    std::string window_title_;
};
#endif // ENABLE_SDL
