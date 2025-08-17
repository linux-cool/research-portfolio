#include "avframe_manager.h"
#include <iostream>
#include <vector>
#include <chrono>

/**
 * @brief AVFrame管理器测试程序
 */

void testBasicAllocation() {
    LOG_INFO("Testing basic allocation...");
    
    AVFrameManager manager(5);
    
    // 分配几个frame
    std::vector<AVFrame*> frames;
    for (int i = 0; i < 3; ++i) {
        AVFrame* frame = manager.AllocFrame(640, 480, AV_PIX_FMT_YUV420P);
        if (frame) {
            frames.push_back(frame);
            LOG_INFO("Allocated frame %d: %dx%d", i, frame->width, frame->height);
        } else {
            LOG_ERROR("Failed to allocate frame %d", i);
        }
    }
    
    // 检查统计信息
    auto stats = manager.GetStats();
    LOG_INFO("Pool stats: total=%zu, allocated=%zu, available=%zu", 
             stats.total_frames, stats.allocated_frames, stats.available_frames);
    
    // 释放frame
    for (AVFrame* frame : frames) {
        manager.ReleaseFrame(frame);
    }
    
    stats = manager.GetStats();
    LOG_INFO("After release: total=%zu, allocated=%zu, available=%zu", 
             stats.total_frames, stats.allocated_frames, stats.available_frames);
}

void testFrameReuse() {
    LOG_INFO("Testing frame reuse...");
    
    AVFrameManager manager(3);
    
    // 分配和释放同样规格的frame
    for (int cycle = 0; cycle < 3; ++cycle) {
        LOG_INFO("Cycle %d:", cycle);
        
        std::vector<AVFrame*> frames;
        for (int i = 0; i < 2; ++i) {
            AVFrame* frame = manager.AllocFrame(320, 240, AV_PIX_FMT_YUV420P);
            if (frame) {
                frames.push_back(frame);
            }
        }
        
        auto stats = manager.GetStats();
        LOG_INFO("  Allocated: %zu frames", stats.allocated_frames);
        
        for (AVFrame* frame : frames) {
            manager.ReleaseFrame(frame);
        }
        
        stats = manager.GetStats();
        LOG_INFO("  Available: %zu frames", stats.available_frames);
    }
}

void testCloneFrame() {
    LOG_INFO("Testing frame cloning...");
    
    AVFrameManager manager(5);
    
    // 创建源frame
    AVFrame* src_frame = manager.AllocFrame(160, 120, AV_PIX_FMT_YUV420P);
    if (!src_frame) {
        LOG_ERROR("Failed to allocate source frame");
        return;
    }
    
    // 填充一些测试数据
    memset(src_frame->data[0], 128, src_frame->linesize[0] * src_frame->height);
    memset(src_frame->data[1], 64, src_frame->linesize[1] * src_frame->height / 2);
    memset(src_frame->data[2], 192, src_frame->linesize[2] * src_frame->height / 2);
    
    src_frame->pts = 12345;
    
    // 克隆frame
    AVFrame* cloned_frame = manager.CloneFrame(src_frame);
    if (cloned_frame) {
        LOG_INFO("Frame cloned successfully");
        LOG_INFO("Original PTS: %lld, Cloned PTS: %lld", src_frame->pts, cloned_frame->pts);
        
        // 验证数据
        bool data_match = (memcmp(src_frame->data[0], cloned_frame->data[0], 
                                 src_frame->linesize[0] * src_frame->height) == 0);
        LOG_INFO("Data match: %s", data_match ? "Yes" : "No");
        
        manager.ReleaseFrame(cloned_frame);
    } else {
        LOG_ERROR("Failed to clone frame");
    }
    
    manager.ReleaseFrame(src_frame);
}

void testPerformance() {
    LOG_INFO("Testing performance...");
    
    const int NUM_FRAMES = 1000;
    const int POOL_SIZE = 20;
    
    AVFrameManager manager(POOL_SIZE);
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // 分配和释放大量frame
    for (int i = 0; i < NUM_FRAMES; ++i) {
        AVFrame* frame = manager.AllocFrame(640, 480, AV_PIX_FMT_YUV420P);
        if (frame) {
            // 模拟一些处理
            frame->pts = i;
            
            // 立即释放
            manager.ReleaseFrame(frame);
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    double avg_time = static_cast<double>(duration.count()) / NUM_FRAMES;
    LOG_INFO("Performance test: %d frames in %lld us (%.2f us/frame)", 
             NUM_FRAMES, duration.count(), avg_time);
    
    auto stats = manager.GetStats();
    LOG_INFO("Final stats: total=%zu, peak=%zu", stats.total_frames, stats.peak_usage);
}

void testRAIIWrapper() {
    LOG_INFO("Testing RAII wrapper...");
    
    auto manager = std::make_shared<AVFrameManager>(5);
    
    {
        // 创建RAII包装器
        AVFrame* raw_frame = manager->AllocFrame(320, 240, AV_PIX_FMT_RGB24);
        AVFrameWrapper wrapper(manager.get(), raw_frame);
        
        if (wrapper) {
            LOG_INFO("RAII wrapper created successfully");
            wrapper->pts = 9999;
            LOG_INFO("Frame PTS set to: %lld", wrapper->pts);
        }
        
        // wrapper会在作用域结束时自动释放frame
    }
    
    auto stats = manager->GetStats();
    LOG_INFO("After RAII cleanup: allocated=%zu, available=%zu", 
             stats.allocated_frames, stats.available_frames);
}

int main(int argc, char* argv[]) {
    LOG_INFO("Starting AVFrame manager tests");
    
    try {
        testBasicAllocation();
        testFrameReuse();
        testCloneFrame();
        testPerformance();
        testRAIIWrapper();
        
        LOG_INFO("All tests completed successfully!");
        
    } catch (const std::exception& e) {
        LOG_ERROR("Test failed with exception: %s", e.what());
        return -1;
    }
    
    return 0;
}
