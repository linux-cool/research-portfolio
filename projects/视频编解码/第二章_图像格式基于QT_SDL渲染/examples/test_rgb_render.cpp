#include "xvideo_view.h"
#include <iostream>
#include <vector>
#include <cmath>

/**
 * @brief RGB渲染测试程序
 * 
 * 测试RGB24格式的图像渲染，生成彩色渐变图案
 */

void generateRGBGradient(std::vector<uint8_t>& buffer, int width, int height, int frame) {
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int index = (y * width + x) * 3;
            
            // 生成彩色渐变
            float fx = static_cast<float>(x) / width;
            float fy = static_cast<float>(y) / height;
            float ft = static_cast<float>(frame) * 0.01f;
            
            // RGB渐变公式
            uint8_t r = static_cast<uint8_t>(255 * (0.5 + 0.5 * sin(fx * 6.28 + ft)));
            uint8_t g = static_cast<uint8_t>(255 * (0.5 + 0.5 * sin(fy * 6.28 + ft)));
            uint8_t b = static_cast<uint8_t>(255 * (0.5 + 0.5 * sin((fx + fy) * 3.14 + ft)));
            
            buffer[index + 0] = r;
            buffer[index + 1] = g;
            buffer[index + 2] = b;
        }
    }
}

void generateColorBars(std::vector<uint8_t>& buffer, int width, int height) {
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
    
    int bar_width = width / 8;
    
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int bar_index = x / bar_width;
            if (bar_index >= 8) bar_index = 7;
            
            int index = (y * width + x) * 3;
            buffer[index + 0] = colors[bar_index][0];
            buffer[index + 1] = colors[bar_index][1];
            buffer[index + 2] = colors[bar_index][2];
        }
    }
}

void generateChessboard(std::vector<uint8_t>& buffer, int width, int height, int square_size = 32) {
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int index = (y * width + x) * 3;
            
            bool is_white = ((x / square_size) + (y / square_size)) % 2 == 0;
            uint8_t color = is_white ? 255 : 0;
            
            buffer[index + 0] = color;
            buffer[index + 1] = color;
            buffer[index + 2] = color;
        }
    }
}

int main(int argc, char* argv[]) {
    LOG_INFO("Starting RGB render test");
    
    // 解析命令行参数
    std::string renderer_type = "auto";
    int width = 640;
    int height = 480;
    int duration = 10; // 秒
    std::string pattern = "gradient"; // gradient, bars, chess
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--renderer" && i + 1 < argc) {
            renderer_type = argv[++i];
        } else if (arg == "--width" && i + 1 < argc) {
            width = std::atoi(argv[++i]);
        } else if (arg == "--height" && i + 1 < argc) {
            height = std::atoi(argv[++i]);
        } else if (arg == "--duration" && i + 1 < argc) {
            duration = std::atoi(argv[++i]);
        } else if (arg == "--pattern" && i + 1 < argc) {
            pattern = argv[++i];
        } else if (arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "Options:\n"
                      << "  --renderer <type>   Renderer type (auto, qt, sdl)\n"
                      << "  --width <width>     Video width (default: 640)\n"
                      << "  --height <height>   Video height (default: 480)\n"
                      << "  --duration <sec>    Test duration in seconds (default: 10)\n"
                      << "  --pattern <type>    Pattern type (gradient, bars, chess)\n"
                      << "  --help              Show this help\n";
            return 0;
        }
    }
    
    // 创建渲染器
    XVideoViewFactory::RendererType type = XVideoViewFactory::RendererType::AUTO;
    if (renderer_type == "qt") {
        type = XVideoViewFactory::RendererType::QT;
    } else if (renderer_type == "sdl") {
        type = XVideoViewFactory::RendererType::SDL;
    }
    
    auto renderer = XVideoViewFactory::Create(type);
    if (!renderer) {
        LOG_ERROR("Failed to create renderer");
        return -1;
    }
    
    LOG_INFO("Created %s renderer", renderer->GetType().c_str());
    
    // 初始化渲染器
    if (!renderer->Init(width, height, PixelFormat::RGB24)) {
        LOG_ERROR("Failed to initialize renderer");
        return -1;
    }
    
    LOG_INFO("Renderer initialized: %dx%d", width, height);
    
    // 分配帧缓冲区
    std::vector<uint8_t> frame_buffer(width * height * 3);
    
    // 设置渲染参数
    renderer->SetTargetFPS(25.0);
    renderer->SetAntiAliasing(true);
    
    // 渲染循环
    int total_frames = duration * 25; // 25 FPS
    int64_t start_time = Utils::GetCurrentTimeMs();
    
    LOG_INFO("Starting render loop: %d frames, pattern: %s", total_frames, pattern.c_str());
    
    for (int frame = 0; frame < total_frames; ++frame) {
        int64_t frame_start = Utils::GetCurrentTimeMs();
        
        // 生成测试图案
        if (pattern == "gradient") {
            generateRGBGradient(frame_buffer, width, height, frame);
        } else if (pattern == "bars") {
            generateColorBars(frame_buffer, width, height);
        } else if (pattern == "chess") {
            generateChessboard(frame_buffer, width, height);
        }
        
        // 渲染帧
        uint8_t* data[4] = {frame_buffer.data(), nullptr, nullptr, nullptr};
        int linesize[4] = {width * 3, 0, 0, 0};
        
        if (!renderer->Render(data, linesize)) {
            LOG_ERROR("Render failed at frame %d", frame);
            break;
        }
        
        // 处理SDL事件（如果是SDL渲染器）
        if (renderer->GetType() == "SDL") {
            auto sdl_renderer = static_cast<SDLVideoView*>(renderer.get());
            if (!sdl_renderer->HandleEvents()) {
                LOG_INFO("User requested exit");
                break;
            }
        }
        
        // 帧率控制
        int64_t frame_time = Utils::GetCurrentTimeMs() - frame_start;
        int64_t target_frame_time = 1000 / 25; // 25 FPS
        if (frame_time < target_frame_time) {
            Utils::SleepMs(target_frame_time - frame_time);
        }
        
        // 进度报告
        if (frame % 25 == 0) {
            double fps = renderer->GetFPS();
            double progress = static_cast<double>(frame) / total_frames * 100;
            LOG_INFO("Progress: %.1f%%, FPS: %.1f", progress, fps);
        }
    }
    
    int64_t total_time = Utils::GetCurrentTimeMs() - start_time;
    double avg_fps = (total_frames * 1000.0) / total_time;
    
    LOG_INFO("Test completed:");
    LOG_INFO("  Total time: %.2f seconds", total_time / 1000.0);
    LOG_INFO("  Average FPS: %.2f", avg_fps);
    LOG_INFO("  Final FPS: %.2f", renderer->GetFPS());
    
    // 清理
    renderer->Close();
    
    return 0;
}
