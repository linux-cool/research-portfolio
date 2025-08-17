#include "xencode.h"
#include "avframe_manager.h"
#include <iostream>

int main(int argc, char* argv[]) {
    LOG_INFO("Starting hardware encode test");
    
    // 检查可用的硬件设备
    auto hw_devices = EncodeUtils::GetHardwareDevices();
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
    
    // 检查H.264硬件编码支持
    if (!EncodeUtils::IsHardwareAccelAvailable(test_device, CodecType::H264)) {
        LOG_WARN("H.264 hardware encoding not supported on %s", test_device.c_str());
        return 0;
    }
    
    auto encoder = XEncodeFactory::Create(CodecType::H264);
    if (!encoder) {
        LOG_ERROR("Failed to create H.264 encoder");
        return -1;
    }
    
    // 配置硬件编码
    EncodeConfig config;
    config.width = 1920;
    config.height = 1080;
    config.pixel_format = AV_PIX_FMT_YUV420P;
    config.frame_rate = {30, 1};
    config.time_base = {1, 30};
    config.bit_rate = 5000000;  // 5Mbps
    config.enable_hw_accel = true;
    config.hw_device = test_device;
    config.preset = QualityPreset::FAST;
    
    size_t total_bytes = 0;
    config.packet_callback = [&total_bytes](const AVPacket* packet) {
        total_bytes += packet->size;
        if (packet->flags & AV_PKT_FLAG_KEY) {
            LOG_INFO("Hardware encoded I-frame: size=%d", packet->size);
        }
    };
    
    if (!encoder->Init(config)) {
        LOG_ERROR("Failed to initialize hardware encoder");
        return -1;
    }
    
    LOG_INFO("Hardware encoder initialized: %s", encoder->GetEncoderInfo().c_str());
    
    AVFrameManager frame_manager(5);
    const int NUM_FRAMES = 60;  // 2秒@30fps
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < NUM_FRAMES; ++i) {
        AVFrame* frame = frame_manager.AllocFrame(1920, 1080, AV_PIX_FMT_YUV420P);
        if (!frame) {
            LOG_ERROR("Failed to allocate frame %d", i);
            break;
        }
        
        // 生成测试图案
        for (int y = 0; y < 1080; ++y) {
            for (int x = 0; x < 1920; ++x) {
                frame->data[0][y * frame->linesize[0] + x] = (x + y + i * 4) % 256;
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
        
        if (i % 15 == 0) {
            LOG_INFO("Hardware encoded frame %d/%d", i + 1, NUM_FRAMES);
        }
    }
    
    encoder->Flush();
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    auto stats = encoder->GetStats();
    double fps = (NUM_FRAMES * 1000.0) / duration.count();
    
    LOG_INFO("Hardware encoding completed!");
    LOG_INFO("Statistics:");
    LOG_INFO("  Device: %s", test_device.c_str());
    LOG_INFO("  Frames encoded: %lu", stats.frames_encoded);
    LOG_INFO("  Total bytes: %zu", total_bytes);
    LOG_INFO("  Encoding time: %ld ms", duration.count());
    LOG_INFO("  Average FPS: %.2f", fps);
    LOG_INFO("  Real-time factor: %.2fx", fps / 30.0);
    
    return 0;
}
