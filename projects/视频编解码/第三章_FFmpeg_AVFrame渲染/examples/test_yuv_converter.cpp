#include "avframe_manager.h"
#include <iostream>

int main(int argc, char* argv[]) {
    LOG_INFO("Starting YUV converter test");
    
    // 创建转换器
    YUVConverter converter;
    
    // 初始化转换器：YUV420P -> RGB24
    if (!converter.Init(640, 480, AV_PIX_FMT_YUV420P, 
                       640, 480, AV_PIX_FMT_RGB24)) {
        LOG_ERROR("Failed to initialize converter");
        return -1;
    }
    
    // 创建frame管理器
    AVFrameManager manager(5);
    
    // 分配源和目标frame
    AVFrame* src_frame = manager.AllocFrame(640, 480, AV_PIX_FMT_YUV420P);
    AVFrame* dst_frame = manager.AllocFrame(640, 480, AV_PIX_FMT_RGB24);
    
    if (!src_frame || !dst_frame) {
        LOG_ERROR("Failed to allocate frames");
        return -1;
    }
    
    // 填充测试数据
    memset(src_frame->data[0], 128, src_frame->linesize[0] * 480);     // Y
    memset(src_frame->data[1], 64,  src_frame->linesize[1] * 240);     // U
    memset(src_frame->data[2], 192, src_frame->linesize[2] * 240);     // V
    
    LOG_INFO("Converting YUV420P to RGB24...");
    
    // 执行转换
    if (converter.Convert(src_frame, dst_frame)) {
        LOG_INFO("Conversion successful");
        
        // 检查结果
        uint8_t r = dst_frame->data[0][0];
        uint8_t g = dst_frame->data[0][1];
        uint8_t b = dst_frame->data[0][2];
        
        LOG_INFO("First pixel RGB: (%d, %d, %d)", r, g, b);
    } else {
        LOG_ERROR("Conversion failed");
    }
    
    // 清理
    manager.ReleaseFrame(src_frame);
    manager.ReleaseFrame(dst_frame);
    
    LOG_INFO("YUV converter test completed");
    return 0;
}
