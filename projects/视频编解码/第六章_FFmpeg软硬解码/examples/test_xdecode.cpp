#include "xdecode.h"
#include "avframe_manager.h"
#include <iostream>
#include <fstream>

void testCodecSupport() {
    LOG_INFO("Testing codec support...");
    
    auto supported_codecs = XDecodeFactory::GetSupportedCodecs();
    LOG_INFO("Supported codecs (%zu):", supported_codecs.size());
    
    for (auto codec : supported_codecs) {
        std::string name = XDecodeFactory::GetCodecName(codec);
        LOG_INFO("  - %s", name.c_str());
    }
    
    // 测试硬件设备
    auto hw_devices = DecodeUtils::GetHardwareDevices();
    LOG_INFO("Available hardware devices (%zu):", hw_devices.size());
    
    for (const auto& device : hw_devices) {
        LOG_INFO("  - %s", device.c_str());
        
        // 检查H.264和H.265支持
        bool h264_support = DecodeUtils::IsHardwareDecodeAvailable(device, CodecType::H264);
        bool h265_support = DecodeUtils::IsHardwareDecodeAvailable(device, CodecType::H265);
        
        LOG_INFO("    H.264: %s, H.265: %s", 
                 h264_support ? "YES" : "NO",
                 h265_support ? "YES" : "NO");
    }
}

void testCodecDetection() {
    LOG_INFO("Testing codec detection...");
    
    // H.264 NAL单元测试数据
    uint8_t h264_data[] = {0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0x00, 0x1E};
    CodecType detected = DecodeUtils::DetectCodecType(h264_data, sizeof(h264_data));
    LOG_INFO("H.264 detection: %s", detected == CodecType::H264 ? "PASS" : "FAIL");
    
    // H.265 NAL单元测试数据
    uint8_t h265_data[] = {0x00, 0x00, 0x00, 0x01, 0x40, 0x01, 0x0C, 0x01};
    detected = DecodeUtils::DetectCodecType(h265_data, sizeof(h265_data));
    LOG_INFO("H.265 detection: %s", detected == CodecType::H265 ? "PASS" : "FAIL");
    
    // 无效数据
    uint8_t invalid_data[] = {0xFF, 0xFF, 0xFF, 0xFF};
    detected = DecodeUtils::DetectCodecType(invalid_data, sizeof(invalid_data));
    LOG_INFO("Invalid data detection: %s", detected == CodecType::UNKNOWN ? "PASS" : "FAIL");
}

void testConfigValidation() {
    LOG_INFO("Testing config validation...");
    
    // 有效配置
    DecodeConfig valid_config;
    valid_config.codec_type = CodecType::H264;
    valid_config.width = 1280;
    valid_config.height = 720;
    valid_config.thread_count = 4;
    
    bool is_valid = DecodeUtils::ValidateConfig(valid_config);
    LOG_INFO("Valid config test: %s", is_valid ? "PASS" : "FAIL");
    
    // 无效配置测试
    std::vector<std::pair<std::string, DecodeConfig>> invalid_configs;
    
    // 奇数尺寸
    DecodeConfig odd_size = valid_config;
    odd_size.width = 641;
    invalid_configs.push_back({"Odd width", odd_size});
    
    // 过大尺寸
    DecodeConfig large_size = valid_config;
    large_size.width = 10000;
    invalid_configs.push_back({"Too large width", large_size});
    
    // 无效线程数
    DecodeConfig invalid_threads = valid_config;
    invalid_threads.thread_count = -1;
    invalid_configs.push_back({"Invalid thread count", invalid_threads});
    
    for (const auto& [name, config] : invalid_configs) {
        bool is_invalid = !DecodeUtils::ValidateConfig(config);
        LOG_INFO("%s test: %s", name.c_str(), is_invalid ? "PASS" : "FAIL");
    }
}

void testBasicDecoding() {
    LOG_INFO("Testing basic H.264 decoding...");
    
    // 创建解码器
    auto decoder = XDecodeFactory::Create(CodecType::H264);
    if (!decoder) {
        LOG_ERROR("Failed to create H.264 decoder");
        return;
    }
    
    // 配置解码器
    DecodeConfig config;
    config.codec_type = CodecType::H264;
    config.pixel_format = AV_PIX_FMT_YUV420P;
    config.enable_multithreading = true;
    config.thread_count = 2;
    
    size_t frames_received = 0;
    config.frame_callback = [&frames_received](AVFrame* frame) {
        frames_received++;
        LOG_INFO("Decoded frame %zu: %dx%d, format=%d, pts=%ld",
                 frames_received, frame->width, frame->height, 
                 frame->format, frame->pts);
    };
    
    config.error_callback = [](const std::string& error) {
        LOG_ERROR("Decoding error: %s", error.c_str());
    };
    
    if (!decoder->Init(config)) {
        LOG_ERROR("Failed to initialize decoder");
        return;
    }
    
    LOG_INFO("Decoder info: %s", decoder->GetDecoderInfo().c_str());
    
    // 模拟解码一些空包（这里只是测试接口）
    AVPacket* packet = av_packet_alloc();
    if (packet) {
        // 创建一个简单的测试包
        packet->data = nullptr;
        packet->size = 0;
        packet->pts = 0;
        packet->dts = 0;
        
        // 尝试解码（会失败，但测试接口）
        decoder->Decode(packet);
        
        av_packet_free(&packet);
    }
    
    // 刷新解码器
    decoder->Flush();
    
    // 获取统计信息
    auto stats = decoder->GetStats();
    LOG_INFO("Decoding statistics:");
    LOG_INFO("  Frames decoded: %lu", stats.frames_decoded);
    LOG_INFO("  Bytes decoded: %lu", stats.bytes_decoded);
    LOG_INFO("  Average FPS: %.2f", stats.avg_fps);
    LOG_INFO("  Average decode time: %.2f ms", stats.avg_decode_time_ms);
    LOG_INFO("  Total time: %ld ms", stats.total_time_ms);
    LOG_INFO("  Errors: %lu", stats.errors_count);
}

void testThreadRecommendation() {
    LOG_INFO("Testing thread recommendation...");
    
    int recommended = DecodeUtils::GetRecommendedThreadCount();
    int cpu_count = std::thread::hardware_concurrency();
    
    LOG_INFO("CPU cores: %d", cpu_count);
    LOG_INFO("Recommended decode threads: %d", recommended);
    
    // 验证推荐值合理
    bool reasonable = (recommended >= 1 && recommended <= cpu_count && recommended <= 6);
    LOG_INFO("Thread recommendation test: %s", reasonable ? "PASS" : "FAIL");
}

void testDecoderCreation() {
    LOG_INFO("Testing decoder creation...");
    
    // 测试支持的编码器
    std::vector<CodecType> test_codecs = {
        CodecType::H264,
        CodecType::H265,
        CodecType::VP8,
        CodecType::VP9,
        CodecType::AV1
    };
    
    for (auto codec : test_codecs) {
        std::string name = XDecodeFactory::GetCodecName(codec);
        auto decoder = XDecodeFactory::Create(codec);
        
        if (decoder) {
            LOG_INFO("  %s decoder: CREATED", name.c_str());
        } else {
            LOG_INFO("  %s decoder: NOT AVAILABLE", name.c_str());
        }
    }
}

int main(int argc, char* argv[]) {
    LOG_INFO("Starting XDecode tests");
    
    try {
        testCodecSupport();
        testCodecDetection();
        testConfigValidation();
        testThreadRecommendation();
        testDecoderCreation();
        testBasicDecoding();
        
        LOG_INFO("All XDecode tests completed!");
        
    } catch (const std::exception& e) {
        LOG_ERROR("Test failed with exception: %s", e.what());
        return -1;
    }
    
    return 0;
}
