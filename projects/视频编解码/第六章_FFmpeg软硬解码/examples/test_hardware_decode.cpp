#include "xdecode.h"
#include "avframe_manager.h"
#include <iostream>

int main(int argc, char* argv[]) {
    LOG_INFO("Starting hardware decode test");
    
    // 检查可用的硬件设备
    auto hw_devices = DecodeUtils::GetHardwareDevices();
    if (hw_devices.empty()) {
        LOG_WARN("No hardware acceleration devices available");
        return 0;
    }
    
    LOG_INFO("Available hardware devices:");
    for (const auto& device : hw_devices) {
        LOG_INFO("  - %s", device.c_str());
    }
    
    // 测试第一个可用的硬件设备
    std::string test_device = hw_devices[0];
    LOG_INFO("Testing hardware device: %s", test_device.c_str());
    
    // 检查H.264硬件解码支持
    if (!DecodeUtils::IsHardwareDecodeAvailable(test_device, CodecType::H264)) {
        LOG_WARN("H.264 hardware decoding not supported on %s", test_device.c_str());
        return 0;
    }
    
    auto decoder = XDecodeFactory::Create(CodecType::H264);
    if (!decoder) {
        LOG_ERROR("Failed to create H.264 decoder");
        return -1;
    }
    
    // 配置硬件解码
    DecodeConfig config;
    config.codec_type = CodecType::H264;
    config.pixel_format = AV_PIX_FMT_YUV420P;
    config.enable_hw_accel = true;
    config.hw_device = test_device;
    config.enable_multithreading = true;
    config.thread_count = 2;
    
    size_t frames_decoded = 0;
    config.frame_callback = [&frames_decoded](AVFrame* frame) {
        frames_decoded++;
        LOG_INFO("Hardware decoded frame %zu: %dx%d, format=%d",
                 frames_decoded, frame->width, frame->height, frame->format);
    };
    
    config.error_callback = [](const std::string& error) {
        LOG_ERROR("Hardware decoding error: %s", error.c_str());
    };
    
    if (!decoder->Init(config)) {
        LOG_ERROR("Failed to initialize hardware decoder");
        return -1;
    }
    
    LOG_INFO("Hardware decoder initialized: %s", decoder->GetDecoderInfo().c_str());
    
    // 模拟硬件解码过程
    LOG_INFO("Simulating hardware decoding process...");
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // 创建一些测试包
    for (int i = 0; i < 10; ++i) {
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
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    // 获取统计信息
    auto stats = decoder->GetStats();
    
    LOG_INFO("Hardware decoding completed!");
    LOG_INFO("Statistics:");
    LOG_INFO("  Device: %s", test_device.c_str());
    LOG_INFO("  Frames decoded: %lu", stats.frames_decoded);
    LOG_INFO("  Bytes decoded: %lu", stats.bytes_decoded);
    LOG_INFO("  Decoding time: %ld ms", duration.count());
    LOG_INFO("  Average FPS: %.2f", stats.avg_fps);
    LOG_INFO("  Average decode time: %.2f ms", stats.avg_decode_time_ms);
    LOG_INFO("  Errors: %lu", stats.errors_count);
    
    return 0;
}
