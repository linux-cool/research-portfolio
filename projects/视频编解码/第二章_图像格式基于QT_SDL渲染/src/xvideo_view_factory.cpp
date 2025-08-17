#include "xvideo_view.h"
#include <algorithm>

std::unique_ptr<XVideoView> XVideoViewFactory::Create(RendererType type, void* parent) {
    if (type == RendererType::AUTO) {
        // 自动选择可用的渲染器
        auto available = GetAvailableTypes();
        if (available.empty()) {
            LOG_ERROR("No available video renderers found");
            return nullptr;
        }
        type = available[0];
    }
    
    switch (type) {
#ifdef ENABLE_QT
        case RendererType::QT:
            return std::make_unique<QtVideoView>(static_cast<QWidget*>(parent));
#endif
            
#ifdef ENABLE_SDL
        case RendererType::SDL:
            return std::make_unique<SDLVideoView>();
#endif
            
        default:
            LOG_ERROR("Unsupported renderer type: %s", TypeToString(type).c_str());
            return nullptr;
    }
}

std::vector<XVideoViewFactory::RendererType> XVideoViewFactory::GetAvailableTypes() {
    std::vector<RendererType> types;
    
#ifdef ENABLE_QT
    types.push_back(RendererType::QT);
#endif
    
#ifdef ENABLE_SDL
    types.push_back(RendererType::SDL);
#endif
    
    return types;
}

std::string XVideoViewFactory::TypeToString(RendererType type) {
    switch (type) {
        case RendererType::AUTO:   return "Auto";
        case RendererType::QT:     return "Qt";
        case RendererType::SDL:    return "SDL";
        case RendererType::OPENGL: return "OpenGL";
        default: return "Unknown";
    }
}

// Qt实现已移动到qt_video_view.cpp

#ifdef ENABLE_SDL

SDLVideoView::SDLVideoView()
    : window_(nullptr)
    , renderer_(nullptr)
    , texture_(nullptr)
    , sws_context_(nullptr, sws_freeContext)
    , window_title_("SDL Video Renderer") {

    // 初始化SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        LOG_ERROR("SDL_Init failed: %s", SDL_GetError());
    }
}

SDLVideoView::~SDLVideoView() {
    Close();
    SDL_Quit();
}

bool SDLVideoView::Init(int width, int height, PixelFormat format) {
    if (width <= 0 || height <= 0) {
        LOG_ERROR("Invalid video dimensions: %dx%d", width, height);
        return false;
    }

    std::lock_guard<std::mutex> lock(render_mutex_);

    width_ = width;
    height_ = height;
    format_ = format;

    // 创建窗口
    if (!CreateWindow()) {
        return false;
    }

    // 创建渲染器
    if (!CreateRenderer()) {
        return false;
    }

    // 创建纹理
    if (!CreateTexture()) {
        return false;
    }

    // 创建像素格式转换上下文
    AVPixelFormat src_format = Utils::ToAVPixelFormat(format);
    if (src_format == AV_PIX_FMT_NONE) {
        LOG_ERROR("Unsupported pixel format");
        return false;
    }

    SwsContext* sws_ctx = sws_getContext(
        width, height, src_format,
        width, height, AV_PIX_FMT_YUV420P,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );

    if (!sws_ctx) {
        LOG_ERROR("Failed to create sws context");
        return false;
    }

    sws_context_.reset(sws_ctx);

    // 分配YUV缓冲区
    int yuv_size = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, width, height, 1);
    yuv_buffer_.resize(yuv_size);

    initialized_ = true;

    LOG_INFO("SDL renderer initialized: %dx%d, format=%d", width, height, static_cast<int>(format));
    return true;
}

bool SDLVideoView::Render(uint8_t* data[], int linesize[]) {
    if (!initialized_ || !data || !linesize) {
        return false;
    }

    std::lock_guard<std::mutex> lock(render_mutex_);

    // 转换为YUV420P
    uint8_t* dst_data[4];
    int dst_linesize[4];

    av_image_fill_arrays(dst_data, dst_linesize, yuv_buffer_.data(),
                        AV_PIX_FMT_YUV420P, width_, height_, 1);

    int ret = sws_scale(sws_context_.get(),
                       data, linesize,
                       0, height_,
                       dst_data, dst_linesize);

    if (ret != height_) {
        LOG_ERROR("sws_scale failed: %d", ret);
        return false;
    }

    // 更新纹理
    SDL_UpdateYUVTexture(texture_, nullptr,
                        dst_data[0], dst_linesize[0],
                        dst_data[1], dst_linesize[1],
                        dst_data[2], dst_linesize[2]);

    // 清屏
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);

    // 渲染纹理
    SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);

    // 显示
    SDL_RenderPresent(renderer_);

    UpdateFPS();
    return true;
}

bool SDLVideoView::Render(AVFrame* frame) {
    if (!frame) {
        return false;
    }

    return Render(frame->data, frame->linesize);
}

void SDLVideoView::Close() {
    std::lock_guard<std::mutex> lock(render_mutex_);

    DestroyResources();
    sws_context_.reset();
    yuv_buffer_.clear();
    initialized_ = false;

    LOG_INFO("SDL renderer closed");
}

bool SDLVideoView::Resize(int width, int height) {
    if (width <= 0 || height <= 0) {
        return false;
    }

    width_ = width;
    height_ = height;

    if (window_) {
        SDL_SetWindowSize(window_, width, height);
    }

    return true;
}

void SDLVideoView::SetAntiAliasing(bool enable) {
    anti_aliasing_ = enable;
    // SDL的抗锯齿需要在创建渲染器时设置
}

double SDLVideoView::GetFPS() const {
    std::lock_guard<std::mutex> lock(fps_mutex_);
    return current_fps_;
}

void SDLVideoView::SetTargetFPS(double fps) {
    target_fps_ = fps;
}

bool SDLVideoView::HandleEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                return false;
            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    width_ = event.window.data1;
                    height_ = event.window.data2;
                }
                break;
        }
    }
    return true;
}

void SDLVideoView::SetWindowTitle(const std::string& title) {
    window_title_ = title;
    if (window_) {
        SDL_SetWindowTitle(window_, title.c_str());
    }
}

bool SDLVideoView::CreateWindow() {
    window_ = SDL_CreateWindow(
        window_title_.c_str(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width_, height_,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );

    if (!window_) {
        LOG_ERROR("SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }

    return true;
}

bool SDLVideoView::CreateRenderer() {
    Uint32 flags = SDL_RENDERER_ACCELERATED;
    if (anti_aliasing_) {
        flags |= SDL_RENDERER_PRESENTVSYNC;
    }

    renderer_ = SDL_CreateRenderer(window_, -1, flags);
    if (!renderer_) {
        LOG_ERROR("SDL_CreateRenderer failed: %s", SDL_GetError());
        return false;
    }

    return true;
}

bool SDLVideoView::CreateTexture() {
    texture_ = SDL_CreateTexture(
        renderer_,
        SDL_PIXELFORMAT_YV12,
        SDL_TEXTUREACCESS_STREAMING,
        width_, height_
    );

    if (!texture_) {
        LOG_ERROR("SDL_CreateTexture failed: %s", SDL_GetError());
        return false;
    }

    return true;
}

void SDLVideoView::DestroyResources() {
    if (texture_) {
        SDL_DestroyTexture(texture_);
        texture_ = nullptr;
    }

    if (renderer_) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }

    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
}

#endif // ENABLE_SDL
