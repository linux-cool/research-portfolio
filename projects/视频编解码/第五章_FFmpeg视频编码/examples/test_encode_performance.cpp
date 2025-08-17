#include "xencode.h"
#include "avframe_manager.h"
#include <iostream>

int main(int argc, char* argv[]) {
    LOG_INFO("Starting encode performance test");
    
    AVFrameManager frame_manager(10);
    
    // 测试不同预设的性能
    std::vector<QualityPreset> presets = {
        QualityPreset::ULTRAFAST,
        QualityPreset::FAST,
        QualityPreset::MEDIUM,
        QualityPreset::SLOW
    };
    
    const int NUM_FRAMES = 30;
    const int WIDTH = 1280;
    const int HEIGHT = 720;
    
    for (auto preset : presets) {
        LOG_INFO("Testing preset: %s", XEncodeFactory::GetPresetName(preset).c_str());
        
        auto encoder = XEncodeFactory::Create(CodecType::H264);
        if (!encoder) {
            LOG_ERROR("Failed to create encoder");
            continue;
        }
        
        EncodeConfig config;
        config.width = WIDTH;
        config.height = HEIGHT;
        config.pixel_format = AV_PIX_FMT_YUV420P;
        config.frame_rate = {30, 1};
        config.time_base = {1, 30};
        config.bit_rate = 2000000;
        config.preset = preset;
        
        size_t total_bytes = 0;
        config.packet_callback = [&total_bytes](const AVPacket* packet) {
            total_bytes += packet->size;
        };
        
        if (!encoder->Init(config)) {
            LOG_ERROR("Failed to initialize encoder");
            continue;
        }
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // 编码测试帧
        for (int i = 0; i < NUM_FRAMES; ++i) {
            AVFrame* frame = frame_manager.AllocFrame(WIDTH, HEIGHT, AV_PIX_FMT_YUV420P);
            if (!frame) continue;
            
            // 简单填充
            memset(frame->data[0], 128, frame->linesize[0] * HEIGHT);
            memset(frame->data[1], 128, frame->linesize[1] * HEIGHT / 2);
            memset(frame->data[2], 128, frame->linesize[2] * HEIGHT / 2);
            frame->pts = i;
            
            encoder->Encode(frame);
            frame_manager.ReleaseFrame(frame);
        }
        
        encoder->Flush();
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        auto stats = encoder->GetStats();
        double fps = (NUM_FRAMES * 1000.0) / duration.count();
        
        LOG_INFO("  Encoding time: %ld ms", duration.count());
        LOG_INFO("  Average FPS: %.2f", fps);
        LOG_INFO("  Total bytes: %zu", total_bytes);
        LOG_INFO("  Compression ratio: %.2f:1", 
                 (WIDTH * HEIGHT * 1.5 * NUM_FRAMES) / (double)total_bytes);
    }
    
    LOG_INFO("Performance test completed");
    return 0;
}
