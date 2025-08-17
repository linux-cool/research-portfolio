#include "xencode.h"
#include "avframe_manager.h"
#include <iostream>
#include <fstream>

void generateTestFrame(AVFrame* frame, int frame_number) {
    if (!frame || !frame->data[0]) return;
    
    int width = frame->width;
    int height = frame->height;
    
    // 生成动态测试图案
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // Y分量 - 移动的渐变
            uint8_t Y = static_cast<uint8_t>((x + y + frame_number * 2) % 256);
            frame->data[0][y * frame->linesize[0] + x] = Y;
            
            // UV分量 - 每2x2像素一个
            if (x % 2 == 0 && y % 2 == 0) {
                int uv_x = x / 2;
                int uv_y = y / 2;
                frame->data[1][uv_y * frame->linesize[1] + uv_x] = 128 + (frame_number % 64); // U
                frame->data[2][uv_y * frame->linesize[2] + uv_x] = 128 - (frame_number % 64); // V
            }
        }
    }
    
    // 设置时间戳
    frame->pts = frame_number;
}

void testBasicEncoding() {
    LOG_INFO("Testing basic H.264 encoding...");
    
    // 创建编码器
    auto encoder = XEncodeFactory::Create(CodecType::H264);
    if (!encoder) {
        LOG_ERROR("Failed to create H.264 encoder");
        return;
    }
    
    // 配置编码器
    EncodeConfig config;
    config.width = 640;
    config.height = 480;
    config.pixel_format = AV_PIX_FMT_YUV420P;
    config.frame_rate = {25, 1};
    config.time_base = {1, 25};
    config.bit_rate = 1000000;  // 1Mbps
    config.gop_size = 25;
    config.preset = QualityPreset::FAST;
    
    // 输出文件
    std::ofstream output_file("test_output.h264", std::ios::binary);
    
    config.packet_callback = [&output_file](const AVPacket* packet) {
        if (output_file.is_open()) {
            output_file.write(reinterpret_cast<const char*>(packet->data), packet->size);
        }
        LOG_INFO("Encoded packet: size=%d, pts=%ld, dts=%ld, flags=0x%x",
                 packet->size, packet->pts, packet->dts, packet->flags);
    };
    
    config.error_callback = [](const std::string& error) {
        LOG_ERROR("Encoding error: %s", error.c_str());
    };
    
    if (!encoder->Init(config)) {
        LOG_ERROR("Failed to initialize encoder");
        return;
    }
    
    LOG_INFO("Encoder info: %s", encoder->GetEncoderInfo().c_str());
    
    // 创建帧管理器
    AVFrameManager frame_manager(5);
    
    // 编码测试帧
    const int NUM_FRAMES = 50;
    for (int i = 0; i < NUM_FRAMES; ++i) {
        AVFrame* frame = frame_manager.AllocFrame(640, 480, AV_PIX_FMT_YUV420P);
        if (!frame) {
            LOG_ERROR("Failed to allocate frame %d", i);
            break;
        }
        
        generateTestFrame(frame, i);
        
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
    LOG_INFO("Encoding statistics:");
    LOG_INFO("  Frames encoded: %lu", stats.frames_encoded);
    LOG_INFO("  Bytes encoded: %lu", stats.bytes_encoded);
    LOG_INFO("  Average FPS: %.2f", stats.avg_fps);
    LOG_INFO("  Average bitrate: %.2f bps", stats.avg_bitrate);
    LOG_INFO("  Total time: %ld ms", stats.encode_time_ms);
    
    output_file.close();
    LOG_INFO("Output saved to test_output.h264");
}

void testCodecSupport() {
    LOG_INFO("Testing codec support...");
    
    auto supported_codecs = XEncodeFactory::GetSupportedCodecs();
    LOG_INFO("Supported codecs (%zu):", supported_codecs.size());
    
    for (auto codec : supported_codecs) {
        std::string name = XEncodeFactory::GetCodecName(codec);
        LOG_INFO("  - %s", name.c_str());
    }
    
    // 测试硬件设备
    auto hw_devices = EncodeUtils::GetHardwareDevices();
    LOG_INFO("Available hardware devices (%zu):", hw_devices.size());
    
    for (const auto& device : hw_devices) {
        LOG_INFO("  - %s", device.c_str());
        
        // 检查H.264支持
        bool h264_support = EncodeUtils::IsHardwareAccelAvailable(device, CodecType::H264);
        bool h265_support = EncodeUtils::IsHardwareAccelAvailable(device, CodecType::H265);
        
        LOG_INFO("    H.264: %s, H.265: %s", 
                 h264_support ? "YES" : "NO",
                 h265_support ? "YES" : "NO");
    }
}

void testBitrateCalculation() {
    LOG_INFO("Testing bitrate calculation...");
    
    struct TestCase {
        int width, height;
        double fps;
        CodecType codec;
    };
    
    std::vector<TestCase> test_cases = {
        {640, 480, 30.0, CodecType::H264},
        {1280, 720, 30.0, CodecType::H264},
        {1920, 1080, 30.0, CodecType::H264},
        {1920, 1080, 60.0, CodecType::H264},
        {1920, 1080, 30.0, CodecType::H265},
        {3840, 2160, 30.0, CodecType::H265}
    };
    
    for (const auto& test_case : test_cases) {
        int64_t bitrate = EncodeUtils::CalculateRecommendedBitrate(
            test_case.width, test_case.height, test_case.fps, test_case.codec);
        
        std::string codec_name = XEncodeFactory::GetCodecName(test_case.codec);
        
        LOG_INFO("  %dx%d@%.1ffps (%s): %ld bps (%.2f Mbps)",
                 test_case.width, test_case.height, test_case.fps,
                 codec_name.c_str(), bitrate, bitrate / 1000000.0);
    }
}

void testConfigValidation() {
    LOG_INFO("Testing config validation...");
    
    // 有效配置
    EncodeConfig valid_config;
    valid_config.width = 1280;
    valid_config.height = 720;
    valid_config.frame_rate = {30, 1};
    valid_config.bit_rate = 2000000;
    valid_config.codec_type = CodecType::H264;
    
    bool is_valid = EncodeUtils::ValidateConfig(valid_config);
    LOG_INFO("Valid config test: %s", is_valid ? "PASS" : "FAIL");
    
    // 无效配置测试
    std::vector<std::pair<std::string, EncodeConfig>> invalid_configs;
    
    // 奇数尺寸
    EncodeConfig odd_size = valid_config;
    odd_size.width = 641;
    invalid_configs.push_back({"Odd width", odd_size});
    
    // 负码率
    EncodeConfig negative_bitrate = valid_config;
    negative_bitrate.bit_rate = -1000;
    invalid_configs.push_back({"Negative bitrate", negative_bitrate});
    
    // 无效CRF
    EncodeConfig invalid_crf = valid_config;
    invalid_crf.crf = 100;
    invalid_configs.push_back({"Invalid CRF", invalid_crf});
    
    for (const auto& [name, config] : invalid_configs) {
        bool is_invalid = !EncodeUtils::ValidateConfig(config);
        LOG_INFO("%s test: %s", name.c_str(), is_invalid ? "PASS" : "FAIL");
    }
}

int main(int argc, char* argv[]) {
    LOG_INFO("Starting XEncode tests");
    
    try {
        testCodecSupport();
        testBitrateCalculation();
        testConfigValidation();
        testBasicEncoding();
        
        LOG_INFO("All XEncode tests completed!");
        
    } catch (const std::exception& e) {
        LOG_ERROR("Test failed with exception: %s", e.what());
        return -1;
    }
    
    return 0;
}
