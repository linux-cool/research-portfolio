#include "xvideo_view.h"
#include <iostream>
#include <vector>
#include <cmath>

/**
 * @brief YUV渲染测试程序
 * 
 * 测试YUV420P格式的图像渲染，生成各种YUV测试图案
 */

void generateYUVGradient(std::vector<uint8_t>& y_plane, 
                        std::vector<uint8_t>& u_plane, 
                        std::vector<uint8_t>& v_plane,
                        int width, int height, int frame) {
    
    // Y平面 (亮度)
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float fx = static_cast<float>(x) / width;
            float fy = static_cast<float>(y) / height;
            float ft = static_cast<float>(frame) * 0.02f;
            
            // 亮度渐变
            uint8_t luma = static_cast<uint8_t>(128 + 127 * sin(fx * 3.14 + ft));
            y_plane[y * width + x] = luma;
        }
    }
    
    // U和V平面 (色度，4:2:0采样)
    int uv_width = width / 2;
    int uv_height = height / 2;
    
    for (int y = 0; y < uv_height; ++y) {
        for (int x = 0; x < uv_width; ++x) {
            float fx = static_cast<float>(x) / uv_width;
            float fy = static_cast<float>(y) / uv_height;
            float ft = static_cast<float>(frame) * 0.01f;
            
            // U分量 (蓝色色度)
            uint8_t u = static_cast<uint8_t>(128 + 64 * sin(fx * 6.28 + ft));
            u_plane[y * uv_width + x] = u;
            
            // V分量 (红色色度)
            uint8_t v = static_cast<uint8_t>(128 + 64 * cos(fy * 6.28 + ft));
            v_plane[y * uv_width + x] = v;
        }
    }
}

void generateYUVColorBars(std::vector<uint8_t>& y_plane, 
                         std::vector<uint8_t>& u_plane, 
                         std::vector<uint8_t>& v_plane,
                         int width, int height) {
    
    // 标准彩条的YUV值
    const uint8_t colors[8][3] = {
        {235, 128, 128}, // 白色
        {210, 16,  146}, // 黄色
        {170, 166, 16},  // 青色
        {145, 54,  34},  // 绿色
        {106, 202, 222}, // 品红
        {81,  90,  240}, // 红色
        {41,  240, 110}, // 蓝色
        {16,  128, 128}  // 黑色
    };
    
    int bar_width = width / 8;
    
    // Y平面
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int bar_index = x / bar_width;
            if (bar_index >= 8) bar_index = 7;
            
            y_plane[y * width + x] = colors[bar_index][0];
        }
    }
    
    // U和V平面
    int uv_width = width / 2;
    int uv_height = height / 2;
    
    for (int y = 0; y < uv_height; ++y) {
        for (int x = 0; x < uv_width; ++x) {
            int bar_index = (x * 2) / bar_width;
            if (bar_index >= 8) bar_index = 7;
            
            u_plane[y * uv_width + x] = colors[bar_index][1];
            v_plane[y * uv_width + x] = colors[bar_index][2];
        }
    }
}

void generateYUVChessboard(std::vector<uint8_t>& y_plane, 
                          std::vector<uint8_t>& u_plane, 
                          std::vector<uint8_t>& v_plane,
                          int width, int height, int square_size = 32) {
    
    // Y平面 - 棋盘图案
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            bool is_white = ((x / square_size) + (y / square_size)) % 2 == 0;
            y_plane[y * width + x] = is_white ? 235 : 16; // 白色或黑色
        }
    }
    
    // U和V平面 - 中性色度
    int uv_width = width / 2;
    int uv_height = height / 2;
    
    for (int y = 0; y < uv_height; ++y) {
        for (int x = 0; x < uv_width; ++x) {
            u_plane[y * uv_width + x] = 128; // 中性U
            v_plane[y * uv_width + x] = 128; // 中性V
        }
    }
}

int main(int argc, char* argv[]) {
    LOG_INFO("Starting YUV render test");
    
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
    
    // 确保宽高是偶数（YUV420P要求）
    width = (width + 1) & ~1;
    height = (height + 1) & ~1;
    
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
    if (!renderer->Init(width, height, PixelFormat::YUV420P)) {
        LOG_ERROR("Failed to initialize renderer");
        return -1;
    }
    
    LOG_INFO("Renderer initialized: %dx%d (YUV420P)", width, height);
    
    // 分配YUV平面缓冲区
    std::vector<uint8_t> y_plane(width * height);
    std::vector<uint8_t> u_plane(width * height / 4);
    std::vector<uint8_t> v_plane(width * height / 4);
    
    // 设置渲染参数
    renderer->SetTargetFPS(25.0);
    renderer->SetAntiAliasing(true);
    
    // 渲染循环
    int total_frames = duration * 25; // 25 FPS
    int64_t start_time = Utils::GetCurrentTimeMs();
    
    LOG_INFO("Starting render loop: %d frames, pattern: %s", total_frames, pattern.c_str());
    
    for (int frame = 0; frame < total_frames; ++frame) {
        int64_t frame_start = Utils::GetCurrentTimeMs();
        
        // 生成YUV测试图案
        if (pattern == "gradient") {
            generateYUVGradient(y_plane, u_plane, v_plane, width, height, frame);
        } else if (pattern == "bars") {
            generateYUVColorBars(y_plane, u_plane, v_plane, width, height);
        } else if (pattern == "chess") {
            generateYUVChessboard(y_plane, u_plane, v_plane, width, height);
        }
        
        // 设置YUV数据指针和行步长
        uint8_t* data[4] = {
            y_plane.data(),
            u_plane.data(),
            v_plane.data(),
            nullptr
        };
        
        int linesize[4] = {
            width,          // Y平面行步长
            width / 2,      // U平面行步长
            width / 2,      // V平面行步长
            0
        };
        
        // 渲染帧
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
