#include "xencode.h"
#include "avframe_manager.h"
#include <iostream>
#include <fstream>

int main(int argc, char* argv[]) {
    LOG_INFO("Starting H.265 encoder test");
    
    // 检查H.265编码器是否可用
    if (!XEncodeFactory::IsCodecSupported(CodecType::H265)) {
        LOG_WARN("H.265 encoder not available, skipping test");
        return 0;
    }
    
    // 创建H.265编码器
    auto encoder = std::make_unique<H265Encoder>();
    if (!encoder) {
        LOG_ERROR("Failed to create H.265 encoder");
        return -1;
    }
    
    // 配置编码器
    EncodeConfig config;
    config.width = 1920;
    config.height = 1080;
    config.pixel_format = AV_PIX_FMT_YUV420P;
    config.frame_rate = {25, 1};
    config.time_base = {1, 25};
    config.bit_rate = 3000000;  // 3Mbps
    config.gop_size = 25;
    config.preset = QualityPreset::MEDIUM;
    config.crf = 28;  // H.265通常使用更高的CRF值
    
    // 输出文件
    std::ofstream output_file("test_h265.h265", std::ios::binary);
    size_t total_bytes = 0;
    
    config.packet_callback = [&output_file, &total_bytes](const AVPacket* packet) {
        if (output_file.is_open()) {
            output_file.write(reinterpret_cast<const char*>(packet->data), packet->size);
            total_bytes += packet->size;
        }
        
        const char* frame_type = (packet->flags & AV_PKT_FLAG_KEY) ? "I" : "P/B";
        LOG_INFO("H.265 packet: size=%d, type=%s, pts=%ld", 
                 packet->size, frame_type, packet->pts);
    };
    
    if (!encoder->Init(config)) {
        LOG_ERROR("Failed to initialize H.265 encoder");
        return -1;
    }
    
    LOG_INFO("H.265 encoder initialized: %s", encoder->GetEncoderInfo().c_str());
    
    // 创建帧管理器
    AVFrameManager frame_manager(5);
    
    // 编码测试帧（简单的渐变图案）
    const int NUM_FRAMES = 50;  // 2秒@25fps
    
    for (int i = 0; i < NUM_FRAMES; ++i) {
        AVFrame* frame = frame_manager.AllocFrame(1920, 1080, AV_PIX_FMT_YUV420P);
        if (!frame) {
            LOG_ERROR("Failed to allocate frame %d", i);
            break;
        }
        
        // 生成简单的测试图案
        for (int y = 0; y < 1080; ++y) {
            for (int x = 0; x < 1920; ++x) {
                frame->data[0][y * frame->linesize[0] + x] = (x + y + i) % 256;
            }
        }
        
        // UV分量
        for (int y = 0; y < 540; ++y) {
            for (int x = 0; x < 960; ++x) {
                frame->data[1][y * frame->linesize[1] + x] = 128;
                frame->data[2][y * frame->linesize[2] + x] = 128;
            }
        }
        
        frame->pts = i;
        
        if (!encoder->Encode(frame)) {
            LOG_ERROR("Failed to encode frame %d", i);
            frame_manager.ReleaseFrame(frame);
            break;
        }
        
        frame_manager.ReleaseFrame(frame);
        
        if (i % 10 == 0) {
            LOG_INFO("Encoded frame %d/%d", i + 1, NUM_FRAMES);
        }
    }
    
    // 刷新编码器
    encoder->Flush();
    
    // 获取统计信息
    auto stats = encoder->GetStats();
    
    LOG_INFO("H.265 encoding completed!");
    LOG_INFO("Statistics:");
    LOG_INFO("  Frames encoded: %lu", stats.frames_encoded);
    LOG_INFO("  Total bytes: %zu", total_bytes);
    LOG_INFO("  Average FPS: %.2f", stats.avg_fps);
    LOG_INFO("  Average bitrate: %.2f kbps", stats.avg_bitrate / 1000.0);
    
    output_file.close();
    LOG_INFO("H.265 stream saved to test_h265.h265");
    
    return 0;
}
