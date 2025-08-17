#include "xdecode.h"
#include "avframe_manager.h"
#include <iostream>

int main(int argc, char* argv[]) {
    LOG_INFO("Starting H.264 decoder test");
    
    // 创建H.264解码器
    auto decoder = std::make_unique<H264Decoder>();
    if (!decoder) {
        LOG_ERROR("Failed to create H.264 decoder");
        return -1;
    }
    
    // 配置解码器
    DecodeConfig config;
    config.codec_type = CodecType::H264;
    config.pixel_format = AV_PIX_FMT_YUV420P;
    config.enable_multithreading = true;
    config.thread_count = 4;
    
    size_t frames_decoded = 0;
    config.frame_callback = [&frames_decoded](AVFrame* frame) {
        frames_decoded++;
        LOG_INFO("H.264 decoded frame %zu: %dx%d, pts=%ld",
                 frames_decoded, frame->width, frame->height, frame->pts);
    };
    
    config.error_callback = [](const std::string& error) {
        LOG_ERROR("H.264 decoding error: %s", error.c_str());
    };
    
    if (!decoder->Init(config)) {
        LOG_ERROR("Failed to initialize H.264 decoder");
        return -1;
    }
    
    LOG_INFO("H.264 decoder initialized: %s", decoder->GetDecoderInfo().c_str());
    
    // 模拟解码过程（实际应用中需要真实的H.264数据）
    LOG_INFO("Simulating H.264 decoding process...");
    
    // 创建一些测试包
    for (int i = 0; i < 5; ++i) {
        AVPacket* packet = av_packet_alloc();
        if (packet) {
            // 这里应该是真实的H.264数据
            packet->data = nullptr;
            packet->size = 0;
            packet->pts = i;
            packet->dts = i;
            
            // 尝试解码
            decoder->Decode(packet);
            
            av_packet_free(&packet);
        }
    }
    
    // 刷新解码器
    decoder->Flush();
    
    // 获取统计信息
    auto stats = decoder->GetStats();
    
    LOG_INFO("H.264 decoding completed!");
    LOG_INFO("Statistics:");
    LOG_INFO("  Frames decoded: %lu", stats.frames_decoded);
    LOG_INFO("  Bytes decoded: %lu", stats.bytes_decoded);
    LOG_INFO("  Average FPS: %.2f", stats.avg_fps);
    LOG_INFO("  Average decode time: %.2f ms", stats.avg_decode_time_ms);
    LOG_INFO("  Total time: %ld ms", stats.total_time_ms);
    LOG_INFO("  Errors: %lu", stats.errors_count);
    
    return 0;
}
