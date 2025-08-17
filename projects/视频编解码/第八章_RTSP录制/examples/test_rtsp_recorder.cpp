#include "xrtsp.h"
#include <filesystem>

void testBasicRecording() {
    LOG_INFO("Testing basic RTSP recording...");
    
    XRTSPRecorder recorder;
    
    // 配置录制参数
    RTSPRecordConfig config;
    config.rtsp_url = "rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mov";
    config.output_file = "test_output/rtsp_record_basic.mp4";
    config.output_format = "mp4";
    config.max_duration_ms = 10000; // 录制10秒
    
    // 配置RTSP参数
    config.rtsp_config.timeout_ms = 10000;
    config.rtsp_config.enable_tcp = true;
    config.rtsp_config.enable_video = true;
    config.rtsp_config.enable_audio = true;
    
    // 设置回调函数
    config.file_completed_callback = [](const std::string& filename) {
        LOG_INFO("Recording completed: %s", filename.c_str());
        
        // 检查文件是否存在
        if (std::filesystem::exists(filename)) {
            auto file_size = std::filesystem::file_size(filename);
            LOG_INFO("  File size: %llu bytes", file_size);
        } else {
            LOG_ERROR("  File not found!");
        }
    };
    
    config.progress_callback = [](int64_t duration_ms, int64_t file_size) {
        LOG_INFO("Recording progress: %lld ms, %lld bytes", duration_ms, file_size);
    };
    
    // 创建输出目录
    std::filesystem::create_directories("test_output");
    
    // 开始录制
    if (!recorder.StartRecord(config)) {
        LOG_ERROR("Failed to start recording");
        return;
    }
    
    LOG_INFO("Recording started, waiting for completion...");
    
    // 等待录制完成
    while (recorder.IsRecording()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        RTSPStats stats = recorder.GetRecordStats();
        LOG_INFO("Recording stats: packets=%llu, bytes=%llu KB, bitrate=%.2f kbps",
                stats.packets_received, stats.bytes_received / 1024,
                stats.avg_bitrate_kbps);
    }
    
    LOG_INFO("Basic recording test completed");
}

void testSegmentedRecording() {
    LOG_INFO("Testing segmented RTSP recording...");
    
    XRTSPRecorder recorder;
    
    RTSPRecordConfig config;
    config.rtsp_url = "rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mov";
    config.output_file = "test_output/rtsp_segment";
    config.output_format = "mp4";
    config.max_duration_ms = 5000;  // 每个片段5秒
    config.max_file_size = 1024 * 1024; // 或者1MB
    
    config.rtsp_config.timeout_ms = 10000;
    config.rtsp_config.enable_tcp = true;
    
    int segment_count = 0;
    config.file_completed_callback = [&segment_count](const std::string& filename) {
        segment_count++;
        LOG_INFO("Segment %d completed: %s", segment_count, filename.c_str());
        
        if (std::filesystem::exists(filename)) {
            auto file_size = std::filesystem::file_size(filename);
            LOG_INFO("  Segment size: %llu bytes", file_size);
        }
    };
    
    // 开始录制
    if (!recorder.StartRecord(config)) {
        LOG_ERROR("Failed to start segmented recording");
        return;
    }
    
    LOG_INFO("Segmented recording started, will record for 20 seconds...");
    
    // 录制20秒，应该产生4个片段
    auto start_time = std::chrono::steady_clock::now();
    while (std::chrono::duration_cast<std::chrono::seconds>(
           std::chrono::steady_clock::now() - start_time).count() < 20) {
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        if (!recorder.IsRecording()) {
            LOG_WARN("Recording stopped unexpectedly");
            break;
        }
        
        LOG_INFO("Current file: %s", recorder.GetCurrentFile().c_str());
    }
    
    recorder.StopRecord();
    
    LOG_INFO("Segmented recording completed, total segments: %d", segment_count);
}

void testRecordingControl() {
    LOG_INFO("Testing recording control (pause/resume)...");
    
    XRTSPRecorder recorder;
    
    RTSPRecordConfig config;
    config.rtsp_url = "rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mov";
    config.output_file = "test_output/rtsp_control.mp4";
    config.output_format = "mp4";
    
    config.rtsp_config.timeout_ms = 10000;
    config.rtsp_config.enable_tcp = true;
    
    config.progress_callback = [](int64_t duration_ms, int64_t file_size) {
        LOG_INFO("Progress: %lld ms, %lld bytes", duration_ms, file_size);
    };
    
    // 开始录制
    if (!recorder.StartRecord(config)) {
        LOG_ERROR("Failed to start recording for control test");
        return;
    }
    
    LOG_INFO("Recording started");
    
    // 录制3秒
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    // 暂停录制
    LOG_INFO("Pausing recording...");
    recorder.PauseRecord();
    
    // 暂停2秒
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // 恢复录制
    LOG_INFO("Resuming recording...");
    recorder.ResumeRecord();
    
    // 再录制3秒
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    // 停止录制
    LOG_INFO("Stopping recording...");
    recorder.StopRecord();
    
    LOG_INFO("Recording control test completed");
}

void testMultipleFormats() {
    LOG_INFO("Testing multiple output formats...");
    
    std::vector<std::string> formats = {"mp4", "avi", "mkv", "flv"};
    
    for (const auto& format : formats) {
        LOG_INFO("Testing format: %s", format.c_str());
        
        XRTSPRecorder recorder;
        
        RTSPRecordConfig config;
        config.rtsp_url = "rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mov";
        config.output_file = "test_output/rtsp_format_" + format;
        config.output_format = format;
        config.max_duration_ms = 5000; // 5秒测试
        
        config.rtsp_config.timeout_ms = 10000;
        config.rtsp_config.enable_tcp = true;
        
        bool recording_completed = false;
        config.file_completed_callback = [&recording_completed, format](const std::string& filename) {
            LOG_INFO("Format %s recording completed: %s", format.c_str(), filename.c_str());
            recording_completed = true;
            
            if (std::filesystem::exists(filename)) {
                auto file_size = std::filesystem::file_size(filename);
                LOG_INFO("  File size: %llu bytes", file_size);
            }
        };
        
        if (recorder.StartRecord(config)) {
            // 等待录制完成
            while (recorder.IsRecording() && !recording_completed) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            
            LOG_INFO("Format %s test: SUCCESS", format.c_str());
        } else {
            LOG_ERROR("Format %s test: FAILED", format.c_str());
        }
        
        LOG_INFO("");
    }
}

void testErrorHandling() {
    LOG_INFO("Testing error handling...");
    
    XRTSPRecorder recorder;
    
    // 测试无效URL
    {
        LOG_INFO("Testing invalid RTSP URL...");
        
        RTSPRecordConfig config;
        config.rtsp_url = "rtsp://invalid.url.test:554/stream";
        config.output_file = "test_output/invalid_url.mp4";
        
        config.rtsp_config.timeout_ms = 3000;
        
        if (!recorder.StartRecord(config)) {
            LOG_INFO("  Invalid URL test: PASSED (expected failure)");
        } else {
            LOG_ERROR("  Invalid URL test: FAILED (unexpected success)");
            recorder.StopRecord();
        }
    }
    
    // 测试无效输出路径
    {
        LOG_INFO("Testing invalid output path...");
        
        RTSPRecordConfig config;
        config.rtsp_url = "rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mov";
        config.output_file = "/invalid/path/output.mp4";
        
        config.rtsp_config.timeout_ms = 5000;
        
        if (!recorder.StartRecord(config)) {
            LOG_INFO("  Invalid path test: PASSED (expected failure)");
        } else {
            LOG_ERROR("  Invalid path test: FAILED (unexpected success)");
            recorder.StopRecord();
        }
    }
    
    // 测试不支持的格式
    {
        LOG_INFO("Testing unsupported format...");
        
        RTSPRecordConfig config;
        config.rtsp_url = "rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mov";
        config.output_file = "test_output/unsupported.xyz";
        config.output_format = "xyz";
        
        config.rtsp_config.timeout_ms = 5000;
        
        if (!recorder.StartRecord(config)) {
            LOG_INFO("  Unsupported format test: PASSED (expected failure)");
        } else {
            LOG_ERROR("  Unsupported format test: FAILED (unexpected success)");
            recorder.StopRecord();
        }
    }
    
    LOG_INFO("Error handling tests completed");
}

int main(int argc, char* argv[]) {
    LOG_INFO("Starting RTSP recorder tests");
    
    try {
        // 基础录制测试
        testBasicRecording();
        
        LOG_INFO("");
        
        // 分段录制测试
        testSegmentedRecording();
        
        LOG_INFO("");
        
        // 录制控制测试
        testRecordingControl();
        
        LOG_INFO("");
        
        // 多格式测试
        testMultipleFormats();
        
        LOG_INFO("");
        
        // 错误处理测试
        testErrorHandling();
        
        LOG_INFO("All RTSP recorder tests completed!");
        
    } catch (const std::exception& e) {
        LOG_ERROR("Test failed with exception: %s", e.what());
        return 1;
    }
    
    return 0;
}
