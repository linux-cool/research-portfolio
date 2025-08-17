#include "sws_converter.h"
#include "avframe_manager.h"
#include <iostream>

int main(int argc, char* argv[]) {
    LOG_INFO("Starting format converter test");
    
    AVFrameManager manager(5);
    
    // 创建测试帧
    AVFrame* yuv_frame = manager.AllocFrame(640, 480, AV_PIX_FMT_YUV420P);
    AVFrame* rgb_frame = manager.AllocFrame(640, 480, AV_PIX_FMT_RGB24);
    AVFrame* rgba_frame = manager.AllocFrame(640, 480, AV_PIX_FMT_RGBA);
    
    if (!yuv_frame || !rgb_frame || !rgba_frame) {
        LOG_ERROR("Failed to allocate frames");
        return -1;
    }
    
    // 填充YUV测试数据
    memset(yuv_frame->data[0], 128, yuv_frame->linesize[0] * 480);     // Y
    memset(yuv_frame->data[1], 64,  yuv_frame->linesize[1] * 240);     // U  
    memset(yuv_frame->data[2], 192, yuv_frame->linesize[2] * 240);     // V
    
    LOG_INFO("Testing YUV420P to RGB24 conversion...");
    if (FormatConverter::YUV420P_to_RGB24(yuv_frame, rgb_frame)) {
        LOG_INFO("YUV420P to RGB24: SUCCESS");
        
        // 检查结果
        uint8_t r = rgb_frame->data[0][0];
        uint8_t g = rgb_frame->data[0][1]; 
        uint8_t b = rgb_frame->data[0][2];
        LOG_INFO("First pixel RGB: (%d, %d, %d)", r, g, b);
    } else {
        LOG_ERROR("YUV420P to RGB24: FAILED");
    }
    
    LOG_INFO("Testing YUV420P to RGBA conversion...");
    if (FormatConverter::YUV420P_to_RGBA(yuv_frame, rgba_frame)) {
        LOG_INFO("YUV420P to RGBA: SUCCESS");
    } else {
        LOG_ERROR("YUV420P to RGBA: FAILED");
    }
    
    LOG_INFO("Testing RGB24 to YUV420P conversion...");
    AVFrame* yuv_back = manager.AllocFrame(640, 480, AV_PIX_FMT_YUV420P);
    if (yuv_back && FormatConverter::RGB24_to_YUV420P(rgb_frame, yuv_back)) {
        LOG_INFO("RGB24 to YUV420P: SUCCESS");
        manager.ReleaseFrame(yuv_back);
    } else {
        LOG_ERROR("RGB24 to YUV420P: FAILED");
    }
    
    // 测试帧大小计算
    size_t yuv_size = FormatConverter::CalculateFrameSize(640, 480, AV_PIX_FMT_YUV420P);
    size_t rgb_size = FormatConverter::CalculateFrameSize(640, 480, AV_PIX_FMT_RGB24);
    
    LOG_INFO("Frame sizes: YUV420P=%zu bytes, RGB24=%zu bytes", yuv_size, rgb_size);
    
    // 清理
    manager.ReleaseFrame(yuv_frame);
    manager.ReleaseFrame(rgb_frame);
    manager.ReleaseFrame(rgba_frame);
    
    LOG_INFO("Format converter test completed");
    return 0;
}
