#include "sws_converter.h"
#include "avframe_manager.h"
#include <iostream>
#include <chrono>

void generateTestYUV(AVFrame* frame) {
    if (!frame || !frame->data[0]) return;
    
    int width = frame->width;
    int height = frame->height;
    
    // 生成测试YUV数据
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // Y分量 - 渐变
            uint8_t Y = static_cast<uint8_t>((x + y) % 256);
            frame->data[0][y * frame->linesize[0] + x] = Y;
            
            // UV分量 - 每2x2像素一个
            if (x % 2 == 0 && y % 2 == 0) {
                int uv_x = x / 2;
                int uv_y = y / 2;
                frame->data[1][uv_y * frame->linesize[1] + uv_x] = 128; // U
                frame->data[2][uv_y * frame->linesize[2] + uv_x] = 128; // V
            }
        }
    }
}

void testBasicConversion() {
    LOG_INFO("Testing basic YUV420P to RGB24 conversion...");
    
    AVFrameManager manager(5);
    SwsConverter converter;
    
    // 创建源和目标frame
    AVFrame* yuv_frame = manager.AllocFrame(640, 480, AV_PIX_FMT_YUV420P);
    AVFrame* rgb_frame = manager.AllocFrame(640, 480, AV_PIX_FMT_RGB24);
    
    if (!yuv_frame || !rgb_frame) {
        LOG_ERROR("Failed to allocate frames");
        return;
    }
    
    // 生成测试数据
    generateTestYUV(yuv_frame);
    
    // 配置转换器
    SwsConverter::Config config;
    config.src_width = 640;
    config.src_height = 480;
    config.src_format = AV_PIX_FMT_YUV420P;
    config.dst_width = 640;
    config.dst_height = 480;
    config.dst_format = AV_PIX_FMT_RGB24;
    config.quality = SwsConverter::Quality::BILINEAR;
    
    if (!converter.Init(config)) {
        LOG_ERROR("Failed to initialize converter");
        return;
    }
    
    // 执行转换
    auto start = std::chrono::high_resolution_clock::now();
    bool success = converter.Convert(yuv_frame, rgb_frame);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    if (success) {
        LOG_INFO("Conversion successful in %ld us", duration.count());
        
        // 检查结果
        uint8_t r = rgb_frame->data[0][0];
        uint8_t g = rgb_frame->data[0][1];
        uint8_t b = rgb_frame->data[0][2];
        LOG_INFO("First pixel RGB: (%d, %d, %d)", r, g, b);
    } else {
        LOG_ERROR("Conversion failed");
    }
    
    manager.ReleaseFrame(yuv_frame);
    manager.ReleaseFrame(rgb_frame);
}

void testScaling() {
    LOG_INFO("Testing scaling conversion...");
    
    AVFrameManager manager(5);
    SwsConverter converter;
    
    // 创建不同尺寸的frame
    AVFrame* src_frame = manager.AllocFrame(640, 480, AV_PIX_FMT_YUV420P);
    AVFrame* dst_frame = manager.AllocFrame(320, 240, AV_PIX_FMT_RGB24);
    
    if (!src_frame || !dst_frame) {
        LOG_ERROR("Failed to allocate frames");
        return;
    }
    
    generateTestYUV(src_frame);
    
    // 配置缩放转换
    SwsConverter::Config config;
    config.src_width = 640;
    config.src_height = 480;
    config.src_format = AV_PIX_FMT_YUV420P;
    config.dst_width = 320;
    config.dst_height = 240;
    config.dst_format = AV_PIX_FMT_RGB24;
    config.quality = SwsConverter::Quality::BICUBIC;
    
    if (converter.Init(config) && converter.Convert(src_frame, dst_frame)) {
        LOG_INFO("Scaling conversion successful: 640x480 -> 320x240");
    } else {
        LOG_ERROR("Scaling conversion failed");
    }
    
    manager.ReleaseFrame(src_frame);
    manager.ReleaseFrame(dst_frame);
}

void testQualityComparison() {
    LOG_INFO("Testing quality comparison...");
    
    AVFrameManager manager(10);
    
    AVFrame* src_frame = manager.AllocFrame(640, 480, AV_PIX_FMT_YUV420P);
    generateTestYUV(src_frame);
    
    std::vector<SwsConverter::Quality> qualities = {
        SwsConverter::Quality::FAST_BILINEAR,
        SwsConverter::Quality::BILINEAR,
        SwsConverter::Quality::BICUBIC,
        SwsConverter::Quality::LANCZOS
    };
    
    for (auto quality : qualities) {
        SwsConverter converter;
        AVFrame* dst_frame = manager.AllocFrame(640, 480, AV_PIX_FMT_RGB24);
        
        SwsConverter::Config config;
        config.src_width = 640;
        config.src_height = 480;
        config.src_format = AV_PIX_FMT_YUV420P;
        config.dst_width = 640;
        config.dst_height = 480;
        config.dst_format = AV_PIX_FMT_RGB24;
        config.quality = quality;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        if (converter.Init(config) && converter.Convert(src_frame, dst_frame)) {
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            
            LOG_INFO("Quality %d: %ld us", static_cast<int>(quality), duration.count());
        } else {
            LOG_ERROR("Quality %d: failed", static_cast<int>(quality));
        }
        
        manager.ReleaseFrame(dst_frame);
    }
    
    manager.ReleaseFrame(src_frame);
}

void testFormatConverter() {
    LOG_INFO("Testing FormatConverter utilities...");
    
    AVFrameManager manager(5);
    
    // 测试YUV420P to RGB24
    AVFrame* yuv_frame = manager.AllocFrame(320, 240, AV_PIX_FMT_YUV420P);
    AVFrame* rgb_frame = manager.AllocFrame(320, 240, AV_PIX_FMT_RGB24);
    
    generateTestYUV(yuv_frame);
    
    if (FormatConverter::YUV420P_to_RGB24(yuv_frame, rgb_frame)) {
        LOG_INFO("YUV420P to RGB24 conversion successful");
        
        // 保存为PPM文件
        if (FormatConverter::SaveRGBFrame(rgb_frame, "test_output.ppm")) {
            LOG_INFO("RGB frame saved to test_output.ppm");
        }
    } else {
        LOG_ERROR("YUV420P to RGB24 conversion failed");
    }
    
    // 测试RGB24 to YUV420P
    AVFrame* yuv_back = manager.AllocFrame(320, 240, AV_PIX_FMT_YUV420P);
    if (FormatConverter::RGB24_to_YUV420P(rgb_frame, yuv_back)) {
        LOG_INFO("RGB24 to YUV420P conversion successful");
    } else {
        LOG_ERROR("RGB24 to YUV420P conversion failed");
    }
    
    manager.ReleaseFrame(yuv_frame);
    manager.ReleaseFrame(rgb_frame);
    manager.ReleaseFrame(yuv_back);
}

void testPerformance() {
    LOG_INFO("Testing conversion performance...");
    
    const int NUM_CONVERSIONS = 100;
    AVFrameManager manager(5);
    SwsConverter converter;
    
    AVFrame* src_frame = manager.AllocFrame(1920, 1080, AV_PIX_FMT_YUV420P);
    AVFrame* dst_frame = manager.AllocFrame(1920, 1080, AV_PIX_FMT_RGB24);
    
    generateTestYUV(src_frame);
    
    SwsConverter::Config config;
    config.src_width = 1920;
    config.src_height = 1080;
    config.src_format = AV_PIX_FMT_YUV420P;
    config.dst_width = 1920;
    config.dst_height = 1080;
    config.dst_format = AV_PIX_FMT_RGB24;
    config.quality = SwsConverter::Quality::BILINEAR;
    
    if (!converter.Init(config)) {
        LOG_ERROR("Failed to initialize converter for performance test");
        return;
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < NUM_CONVERSIONS; ++i) {
        if (!converter.Convert(src_frame, dst_frame)) {
            LOG_ERROR("Conversion failed at iteration %d", i);
            break;
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    double avg_time = static_cast<double>(total_duration.count()) / NUM_CONVERSIONS;
    double fps = 1000.0 / avg_time;
    
    LOG_INFO("Performance test results:");
    LOG_INFO("  %d conversions of 1920x1080", NUM_CONVERSIONS);
    LOG_INFO("  Total time: %ld ms", total_duration.count());
    LOG_INFO("  Average time: %.2f ms/frame", avg_time);
    LOG_INFO("  Equivalent FPS: %.2f", fps);
    
    manager.ReleaseFrame(src_frame);
    manager.ReleaseFrame(dst_frame);
}

int main(int argc, char* argv[]) {
    LOG_INFO("Starting sws converter tests");
    
    try {
        testBasicConversion();
        testScaling();
        testQualityComparison();
        testFormatConverter();
        testPerformance();
        
        LOG_INFO("All sws converter tests completed!");
        
    } catch (const std::exception& e) {
        LOG_ERROR("Test failed with exception: %s", e.what());
        return -1;
    }
    
    return 0;
}
