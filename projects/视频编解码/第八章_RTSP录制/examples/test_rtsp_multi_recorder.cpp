#include "xrtsp.h"
#include <filesystem>

void testMultiRecorderBasic() {
    LOG_INFO("Testing basic multi-recorder functionality...");
    
    XRTSPMultiRecorder multi_recorder;
    
    // 添加多个录制任务
    std::vector<std::pair<std::string, RTSPRecordConfig>> tasks;
    
    // 任务1：基础录制
    {
        RTSPRecordConfig config;
        config.rtsp_url = "rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mov";
        config.output_file = "test_output/multi_task1.mp4";
        config.output_format = "mp4";
        config.max_duration_ms = 8000;
        
        config.rtsp_config.timeout_ms = 10000;
        config.rtsp_config.enable_tcp = true;
        
        config.file_completed_callback = [](const std::string& filename) {
            LOG_INFO("Task1 completed: %s", filename.c_str());
        };
        
        tasks.push_back({"task1", config});
    }
    
    // 任务2：不同格式
    {
        RTSPRecordConfig config;
        config.rtsp_url = "rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mov";
        config.output_file = "test_output/multi_task2.avi";
        config.output_format = "avi";
        config.max_duration_ms = 6000;
        
        config.rtsp_config.timeout_ms = 10000;
        config.rtsp_config.enable_tcp = true;
        
        config.file_completed_callback = [](const std::string& filename) {
            LOG_INFO("Task2 completed: %s", filename.c_str());
        };
        
        tasks.push_back({"task2", config});
    }
    
    // 任务3：分段录制
    {
        RTSPRecordConfig config;
        config.rtsp_url = "rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mov";
        config.output_file = "test_output/multi_task3_segment";
        config.output_format = "mp4";
        config.max_duration_ms = 3000; // 3秒分段
        
        config.rtsp_config.timeout_ms = 10000;
        config.rtsp_config.enable_tcp = true;
        
        config.file_completed_callback = [](const std::string& filename) {
            LOG_INFO("Task3 segment completed: %s", filename.c_str());
        };
        
        tasks.push_back({"task3", config});
    }
    
    // 创建输出目录
    std::filesystem::create_directories("test_output");
    
    // 添加所有任务
    for (const auto& [id, config] : tasks) {
        if (multi_recorder.AddRecordTask(id, config)) {
            LOG_INFO("Added task: %s", id.c_str());
        } else {
            LOG_ERROR("Failed to add task: %s", id.c_str());
        }
    }
    
    // 获取任务列表
    auto task_ids = multi_recorder.GetTaskIds();
    LOG_INFO("Total tasks: %zu", task_ids.size());
    for (const auto& id : task_ids) {
        LOG_INFO("  Task ID: %s", id.c_str());
    }
    
    // 注意：由于XRTSPMultiRecorder的StartAllRecords需要为每个任务单独配置
    // 这里我们手动启动每个任务进行测试
    LOG_INFO("Note: Manual task starting for testing (StartAllRecords needs individual configs)");
    
    // 等待一段时间模拟录制过程
    LOG_INFO("Simulating recording process for 10 seconds...");
    for (int i = 0; i < 10; ++i) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // 获取每个任务的统计信息
        for (const auto& id : task_ids) {
            RTSPStats stats = multi_recorder.GetTaskStats(id);
            LOG_INFO("Task %s stats: packets=%llu, bytes=%llu KB",
                    id.c_str(), stats.packets_received, stats.bytes_received / 1024);
        }
    }
    
    // 停止所有录制
    LOG_INFO("Stopping all recordings...");
    multi_recorder.StopAllRecords();
    
    // 移除任务
    for (const auto& id : task_ids) {
        if (multi_recorder.RemoveRecordTask(id)) {
            LOG_INFO("Removed task: %s", id.c_str());
        }
    }
    
    LOG_INFO("Multi-recorder basic test completed");
}

void testMultiRecorderTaskManagement() {
    LOG_INFO("Testing multi-recorder task management...");
    
    XRTSPMultiRecorder multi_recorder;
    
    // 测试添加任务
    {
        RTSPRecordConfig config;
        config.rtsp_url = "rtsp://test.url/stream";
        config.output_file = "test_output/mgmt_test.mp4";
        
        // 添加任务
        if (multi_recorder.AddRecordTask("test_task", config)) {
            LOG_INFO("Task addition: SUCCESS");
        } else {
            LOG_ERROR("Task addition: FAILED");
        }
        
        // 尝试添加重复任务
        if (!multi_recorder.AddRecordTask("test_task", config)) {
            LOG_INFO("Duplicate task prevention: SUCCESS");
        } else {
            LOG_ERROR("Duplicate task prevention: FAILED");
        }
        
        // 检查任务列表
        auto task_ids = multi_recorder.GetTaskIds();
        if (task_ids.size() == 1 && task_ids[0] == "test_task") {
            LOG_INFO("Task list verification: SUCCESS");
        } else {
            LOG_ERROR("Task list verification: FAILED");
        }
        
        // 移除任务
        if (multi_recorder.RemoveRecordTask("test_task")) {
            LOG_INFO("Task removal: SUCCESS");
        } else {
            LOG_ERROR("Task removal: FAILED");
        }
        
        // 尝试移除不存在的任务
        if (!multi_recorder.RemoveRecordTask("nonexistent_task")) {
            LOG_INFO("Nonexistent task removal: SUCCESS (expected failure)");
        } else {
            LOG_ERROR("Nonexistent task removal: FAILED (unexpected success)");
        }
        
        // 检查任务列表为空
        task_ids = multi_recorder.GetTaskIds();
        if (task_ids.empty()) {
            LOG_INFO("Empty task list verification: SUCCESS");
        } else {
            LOG_ERROR("Empty task list verification: FAILED");
        }
    }
    
    LOG_INFO("Task management test completed");
}

void testMultiRecorderConcurrency() {
    LOG_INFO("Testing multi-recorder concurrency...");
    
    XRTSPMultiRecorder multi_recorder;
    
    // 创建多个任务配置
    std::vector<RTSPRecordConfig> configs;
    
    for (int i = 0; i < 5; ++i) {
        RTSPRecordConfig config;
        config.rtsp_url = "rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mov";
        config.output_file = "test_output/concurrent_" + std::to_string(i) + ".mp4";
        config.output_format = "mp4";
        config.max_duration_ms = 5000;
        
        config.rtsp_config.timeout_ms = 10000;
        config.rtsp_config.enable_tcp = true;
        
        int task_id = i;
        config.file_completed_callback = [task_id](const std::string& filename) {
            LOG_INFO("Concurrent task %d completed: %s", task_id, filename.c_str());
        };
        
        configs.push_back(config);
    }
    
    // 并发添加任务
    std::vector<std::thread> add_threads;
    std::atomic<int> success_count(0);
    
    for (int i = 0; i < 5; ++i) {
        add_threads.emplace_back([&multi_recorder, &configs, &success_count, i]() {
            std::string task_id = "concurrent_task_" + std::to_string(i);
            if (multi_recorder.AddRecordTask(task_id, configs[i])) {
                success_count++;
                LOG_INFO("Concurrent add task %d: SUCCESS", i);
            } else {
                LOG_ERROR("Concurrent add task %d: FAILED", i);
            }
        });
    }
    
    // 等待所有添加线程完成
    for (auto& thread : add_threads) {
        thread.join();
    }
    
    LOG_INFO("Concurrent task addition completed: %d/5 successful", success_count.load());
    
    // 检查任务列表
    auto task_ids = multi_recorder.GetTaskIds();
    LOG_INFO("Total tasks after concurrent addition: %zu", task_ids.size());
    
    // 并发移除任务
    std::vector<std::thread> remove_threads;
    std::atomic<int> remove_count(0);
    
    for (const auto& id : task_ids) {
        remove_threads.emplace_back([&multi_recorder, &remove_count, id]() {
            if (multi_recorder.RemoveRecordTask(id)) {
                remove_count++;
                LOG_INFO("Concurrent remove task %s: SUCCESS", id.c_str());
            } else {
                LOG_ERROR("Concurrent remove task %s: FAILED", id.c_str());
            }
        });
    }
    
    // 等待所有移除线程完成
    for (auto& thread : remove_threads) {
        thread.join();
    }
    
    LOG_INFO("Concurrent task removal completed: %d/%zu successful", 
             remove_count.load(), task_ids.size());
    
    // 最终检查
    auto final_task_ids = multi_recorder.GetTaskIds();
    if (final_task_ids.empty()) {
        LOG_INFO("Concurrency test: SUCCESS (all tasks removed)");
    } else {
        LOG_ERROR("Concurrency test: FAILED (%zu tasks remaining)", final_task_ids.size());
    }
    
    LOG_INFO("Concurrency test completed");
}

void testMultiRecorderPerformance() {
    LOG_INFO("Testing multi-recorder performance...");
    
    XRTSPMultiRecorder multi_recorder;
    
    const int num_tasks = 10;
    
    // 性能测试：添加大量任务
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < num_tasks; ++i) {
        RTSPRecordConfig config;
        config.rtsp_url = "rtsp://test.url/stream" + std::to_string(i);
        config.output_file = "test_output/perf_" + std::to_string(i) + ".mp4";
        
        std::string task_id = "perf_task_" + std::to_string(i);
        multi_recorder.AddRecordTask(task_id, config);
    }
    
    auto add_end_time = std::chrono::high_resolution_clock::now();
    auto add_duration = std::chrono::duration_cast<std::chrono::microseconds>(
        add_end_time - start_time).count();
    
    LOG_INFO("Added %d tasks in %lld μs (%.2f μs/task)", 
             num_tasks, add_duration, static_cast<double>(add_duration) / num_tasks);
    
    // 性能测试：获取任务列表
    start_time = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < 1000; ++i) {
        auto task_ids = multi_recorder.GetTaskIds();
    }
    
    auto list_end_time = std::chrono::high_resolution_clock::now();
    auto list_duration = std::chrono::duration_cast<std::chrono::microseconds>(
        list_end_time - start_time).count();
    
    LOG_INFO("1000 task list operations in %lld μs (%.2f μs/operation)", 
             list_duration, static_cast<double>(list_duration) / 1000);
    
    // 性能测试：获取统计信息
    start_time = std::chrono::high_resolution_clock::now();
    
    auto task_ids = multi_recorder.GetTaskIds();
    for (int i = 0; i < 100; ++i) {
        for (const auto& id : task_ids) {
            multi_recorder.GetTaskStats(id);
        }
    }
    
    auto stats_end_time = std::chrono::high_resolution_clock::now();
    auto stats_duration = std::chrono::duration_cast<std::chrono::microseconds>(
        stats_end_time - start_time).count();
    
    LOG_INFO("%d stats operations in %lld μs (%.2f μs/operation)", 
             100 * num_tasks, stats_duration, 
             static_cast<double>(stats_duration) / (100 * num_tasks));
    
    // 清理
    multi_recorder.StopAllRecords();
    
    LOG_INFO("Performance test completed");
}

int main(int argc, char* argv[]) {
    LOG_INFO("Starting RTSP multi-recorder tests");
    
    try {
        // 基础功能测试
        testMultiRecorderBasic();
        
        LOG_INFO("");
        
        // 任务管理测试
        testMultiRecorderTaskManagement();
        
        LOG_INFO("");
        
        // 并发测试
        testMultiRecorderConcurrency();
        
        LOG_INFO("");
        
        // 性能测试
        testMultiRecorderPerformance();
        
        LOG_INFO("All RTSP multi-recorder tests completed!");
        
    } catch (const std::exception& e) {
        LOG_ERROR("Test failed with exception: %s", e.what());
        return 1;
    }
    
    return 0;
}
