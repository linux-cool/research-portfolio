#include "avframe_manager.h"
#include <iostream>

int main(int argc, char* argv[]) {
    LOG_INFO("Starting FPS controller test");
    
    // 创建FPS控制器
    FPSController fps_controller(30.0);
    
    LOG_INFO("Testing 30 FPS for 3 seconds...");
    
    const int NUM_FRAMES = 90; // 3秒 @ 30fps
    int64_t start_time = Utils::GetCurrentTimeMs();
    
    for (int i = 0; i < NUM_FRAMES; ++i) {
        // 模拟帧处理
        Utils::SleepMs(5); // 模拟5ms处理时间
        
        // 等待下一帧
        int64_t wait_time = fps_controller.WaitForNextFrame();
        
        if (i % 30 == 0) {
            double current_fps = fps_controller.GetCurrentFPS();
            LOG_INFO("Frame %d: Current FPS=%.2f, Wait time=%lld ms", 
                     i, current_fps, wait_time);
        }
    }
    
    int64_t total_time = Utils::GetCurrentTimeMs() - start_time;
    double actual_fps = (NUM_FRAMES * 1000.0) / total_time;
    
    auto stats = fps_controller.GetStats();
    LOG_INFO("Test completed:");
    LOG_INFO("  Target FPS: %.2f", stats.target_fps);
    LOG_INFO("  Measured FPS: %.2f", stats.current_fps);
    LOG_INFO("  Actual FPS: %.2f", actual_fps);
    LOG_INFO("  Total frames: %lld", stats.total_frames);
    LOG_INFO("  Avg frame time: %.2f ms", stats.avg_frame_time);
    
    return 0;
}
