#include "sws_converter.h"
#include "avframe_manager.h"
#include <iostream>

int main(int argc, char* argv[]) {
    LOG_INFO("Starting batch converter test");
    
    // 创建批量转换器
    BatchConverter batch_converter(2); // 2个工作线程
    
    AVFrameManager manager(10);
    
    // 创建测试任务
    std::vector<AVFrame*> src_frames;
    std::vector<AVFrame*> dst_frames;
    
    const int NUM_TASKS = 5;
    
    for (int i = 0; i < NUM_TASKS; ++i) {
        AVFrame* src = manager.AllocFrame(320, 240, AV_PIX_FMT_YUV420P);
        AVFrame* dst = manager.AllocFrame(320, 240, AV_PIX_FMT_RGB24);
        
        if (src && dst) {
            // 填充测试数据
            memset(src->data[0], 128 + i * 20, src->linesize[0] * 240);
            memset(src->data[1], 64, src->linesize[1] * 120);
            memset(src->data[2], 192, src->linesize[2] * 120);
            
            src_frames.push_back(src);
            dst_frames.push_back(dst);
        }
    }
    
    LOG_INFO("Created %zu test frames", src_frames.size());
    
    // 添加转换任务
    for (size_t i = 0; i < src_frames.size(); ++i) {
        BatchConverter::ConvertTask task;
        task.src_frame = src_frames[i];
        task.dst_frame = dst_frames[i];
        
        task.config.src_width = 320;
        task.config.src_height = 240;
        task.config.src_format = AV_PIX_FMT_YUV420P;
        task.config.dst_width = 320;
        task.config.dst_height = 240;
        task.config.dst_format = AV_PIX_FMT_RGB24;
        task.config.quality = SwsConverter::Quality::BILINEAR;
        
        task.callback = [i](bool success) {
            LOG_INFO("Task %zu completed: %s", i, success ? "SUCCESS" : "FAILED");
        };
        
        uint64_t task_id = batch_converter.AddTask(task);
        LOG_INFO("Added task %zu with ID %lu", i, task_id);
    }
    
    LOG_INFO("Waiting for all tasks to complete...");
    batch_converter.WaitAll();
    
    // 获取统计信息
    auto stats = batch_converter.GetStats();
    LOG_INFO("Batch conversion statistics:");
    LOG_INFO("  Total tasks: %lu", stats.total_tasks);
    LOG_INFO("  Completed: %lu", stats.completed_tasks);
    LOG_INFO("  Failed: %lu", stats.failed_tasks);
    LOG_INFO("  Average time: %.2f ms", stats.avg_convert_time_ms);
    
    // 清理
    for (AVFrame* frame : src_frames) {
        manager.ReleaseFrame(frame);
    }
    for (AVFrame* frame : dst_frames) {
        manager.ReleaseFrame(frame);
    }
    
    LOG_INFO("Batch converter test completed");
    return 0;
}
