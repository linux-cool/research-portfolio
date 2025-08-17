#include "xencode.h"
#include "avframe_manager.h"
#include <iostream>
#include <fstream>

void generateColorFrame(AVFrame* frame, int frame_number) {
    if (!frame || !frame->data[0]) return;
    
    int width = frame->width;
    int height = frame->height;
    
    // 生成彩色测试图案
    uint8_t colors[3] = {
        static_cast<uint8_t>(128 + 64 * sin(frame_number * 0.1)),      // Y
        static_cast<uint8_t>(128 + 32 * cos(frame_number * 0.15)),     // U
        static_cast<uint8_t>(128 + 32 * sin(frame_number * 0.2))       // V
    };
    
    // 填充Y平面
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            frame->data[0][y * frame->linesize[0] + x] = colors[0];
        }
    }
    
    // 填充UV平面
    for (int y = 0; y < height / 2; ++y) {
        for (int x = 0; x < width / 2; ++x) {
            frame->data[1][y * frame->linesize[1] + x] = colors[1];
            frame->data[2][y * frame->linesize[2] + x] = colors[2];
        }
    }
    
    frame->pts = frame_number;
}

int main(int argc, char* argv[]) {
    LOG_INFO("Starting H.264 encoder test");
    
    // 创建H.264编码器
    auto encoder = std::make_unique<H264Encoder>();
    if (!encoder) {
        LOG_ERROR("Failed to create H.264 encoder");
        return -1;
    }
    
    // 配置编码器
    EncodeConfig config;
    config.width = 1280;
    config.height = 720;
    config.pixel_format = AV_PIX_FMT_YUV420P;
    config.frame_rate = {30, 1};
    config.time_base = {1, 30};
    config.bit_rate = 2000000;  // 2Mbps
    config.gop_size = 30;
    config.preset = QualityPreset::MEDIUM;
    config.crf = 23;  // 使用CRF模式
    config.profile = "high";
    config.level = "4.0";
    
    // 输出文件
    std::ofstream output_file("test_h264.h264", std::ios::binary);
    size_t total_bytes = 0;
    
    config.packet_callback = [&output_file, &total_bytes](const AVPacket* packet) {
        if (output_file.is_open()) {
            output_file.write(reinterpret_cast<const char*>(packet->data), packet->size);
            total_bytes += packet->size;
        }
        
        const char* frame_type = (packet->flags & AV_PKT_FLAG_KEY) ? "I" : "P/B";
        LOG_INFO("H.264 packet: size=%d, type=%s, pts=%ld", 
                 packet->size, frame_type, packet->pts);
    };
    
    if (!encoder->Init(config)) {
        LOG_ERROR("Failed to initialize H.264 encoder");
        return -1;
    }
    
    LOG_INFO("H.264 encoder initialized: %s", encoder->GetEncoderInfo().c_str());
    
    // 创建帧管理器
    AVFrameManager frame_manager(5);
    
    // 编码测试帧
    const int NUM_FRAMES = 90;  // 3秒@30fps
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < NUM_FRAMES; ++i) {
        AVFrame* frame = frame_manager.AllocFrame(1280, 720, AV_PIX_FMT_YUV420P);
        if (!frame) {
            LOG_ERROR("Failed to allocate frame %d", i);
            break;
        }
        
        generateColorFrame(frame, i);
        
        if (!encoder->Encode(frame)) {
            LOG_ERROR("Failed to encode frame %d", i);
            frame_manager.ReleaseFrame(frame);
            break;
        }
        
        frame_manager.ReleaseFrame(frame);
        
        if (i % 30 == 0) {
            LOG_INFO("Encoded frame %d/%d", i + 1, NUM_FRAMES);
        }
    }
    
    // 刷新编码器
    encoder->Flush();
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    // 获取统计信息
    auto stats = encoder->GetStats();
    
    LOG_INFO("H.264 encoding completed!");
    LOG_INFO("Statistics:");
    LOG_INFO("  Frames encoded: %lu", stats.frames_encoded);
    LOG_INFO("  Total bytes: %zu", total_bytes);
    LOG_INFO("  Average FPS: %.2f", stats.avg_fps);
    LOG_INFO("  Average bitrate: %.2f kbps", stats.avg_bitrate / 1000.0);
    LOG_INFO("  Encoding time: %ld ms", duration.count());
    LOG_INFO("  Real-time factor: %.2fx", (NUM_FRAMES * 1000.0 / 30.0) / duration.count());
    
    output_file.close();
    LOG_INFO("H.264 stream saved to test_h264.h264");
    
    return 0;
}
