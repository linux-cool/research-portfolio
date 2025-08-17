#include "xrtsp.h"
#include <filesystem>

void testClientPerformance() {
    LOG_INFO("Testing RTSP client performance...");
    
    // 使用公开测试流
    std::string test_url = "rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mov";
    
    XRTSPClient client;
    
    // 性能统计
    std::atomic<uint64_t> total_packets(0);
    std::atomic<uint64_t> total_bytes(0);
    std::atomic<uint64_t> video_packets(0);
    std::atomic<uint64_t> audio_packets(0);
    
    auto start_time = std::chrono::steady_clock::now();
    
    RTSPConfig config;
    config.url = test_url;
    config.timeout_ms = 10000;
    config.enable_tcp = true;
    config.buffer_size = 2 * 1024 * 1024; // 2MB缓冲区
    
    // 高性能包处理回调
    config.packet_callback = [&](AVPacket* packet, int stream_index) {
        total_packets++;
        total_bytes += packet->size;
        
        // 简单的流类型判断（基于包大小）
        if (packet->size > 1000) {
            video_packets++;
        } else {
            audio_packets++;
        }
    };
    
    // 连接性能测试
    auto connect_start = std::chrono::steady_clock::now();
    
    if (!client.Connect(config)) {
        LOG_ERROR("Failed to connect for performance test");
        return;
    }
    
    auto connect_end = std::chrono::steady_clock::now();
    auto connect_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        connect_end - connect_start).count();
    
    LOG_INFO("Connection time: %lld ms", connect_time);
    
    // 播放性能测试
    auto play_start = std::chrono::steady_clock::now();
    
    if (!client.Play()) {
        LOG_ERROR("Failed to start playback for performance test");
        return;
    }
    
    auto play_end = std::chrono::steady_clock::now();
    auto play_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        play_end - play_start).count();
    
    LOG_INFO("Play start time: %lld ms", play_time);
    
    // 运行性能测试30秒
    LOG_INFO("Running performance test for 30 seconds...");
    
    auto test_start = std::chrono::steady_clock::now();
    auto last_report = test_start;
    
    while (std::chrono::duration_cast<std::chrono::seconds>(
           std::chrono::steady_clock::now() - test_start).count() < 30) {
        
        auto now = std::chrono::steady_clock::now();
        
        // 每5秒报告一次性能
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_report).count() >= 5) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - test_start).count();
            
            uint64_t packets = total_packets.load();
            uint64_t bytes = total_bytes.load();
            uint64_t video = video_packets.load();
            uint64_t audio = audio_packets.load();
            
            double packets_per_sec = static_cast<double>(packets) / (elapsed / 1000.0);
            double mbps = (static_cast<double>(bytes) * 8.0) / (elapsed / 1000.0) / (1024 * 1024);
            
            LOG_INFO("Performance at %lld ms:", elapsed);
            LOG_INFO("  Packets: %llu (%.2f pps)", packets, packets_per_sec);
            LOG_INFO("  Bytes: %llu (%.2f Mbps)", bytes, mbps);
            LOG_INFO("  Video: %llu, Audio: %llu", video, audio);
            
            RTSPStats stats = client.GetStats();
            LOG_INFO("  Dropped: %llu (%.2f%%)", stats.dropped_packets,
                    packets > 0 ? (static_cast<double>(stats.dropped_packets) / packets * 100.0) : 0.0);
            
            last_report = now;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // 停止和断开
    auto stop_start = std::chrono::steady_clock::now();
    client.Stop();
    client.Disconnect();
    auto stop_end = std::chrono::steady_clock::now();
    
    auto stop_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        stop_end - stop_start).count();
    
    // 最终性能报告
    auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        stop_end - start_time).count();
    
    uint64_t final_packets = total_packets.load();
    uint64_t final_bytes = total_bytes.load();
    
    LOG_INFO("Final performance results:");
    LOG_INFO("  Total time: %lld ms", total_time);
    LOG_INFO("  Stop time: %lld ms", stop_time);
    LOG_INFO("  Total packets: %llu", final_packets);
    LOG_INFO("  Total bytes: %llu (%.2f MB)", final_bytes, static_cast<double>(final_bytes) / (1024 * 1024));
    LOG_INFO("  Average packet rate: %.2f pps", static_cast<double>(final_packets) / (total_time / 1000.0));
    LOG_INFO("  Average bitrate: %.2f Mbps", (static_cast<double>(final_bytes) * 8.0) / (total_time / 1000.0) / (1024 * 1024));
    LOG_INFO("  Video packets: %llu (%.2f%%)", video_packets.load(), 
            final_packets > 0 ? (static_cast<double>(video_packets.load()) / final_packets * 100.0) : 0.0);
    LOG_INFO("  Audio packets: %llu (%.2f%%)", audio_packets.load(),
            final_packets > 0 ? (static_cast<double>(audio_packets.load()) / final_packets * 100.0) : 0.0);
}

void testRecorderPerformance() {
    LOG_INFO("Testing RTSP recorder performance...");
    
    std::filesystem::create_directories("test_output");
    
    XRTSPRecorder recorder;
    
    RTSPRecordConfig config;
    config.rtsp_url = "rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mov";
    config.output_file = "test_output/performance_test.mp4";
    config.output_format = "mp4";
    config.max_duration_ms = 20000; // 20秒录制
    
    config.rtsp_config.timeout_ms = 10000;
    config.rtsp_config.enable_tcp = true;
    config.rtsp_config.buffer_size = 4 * 1024 * 1024; // 4MB缓冲区
    
    // 性能监控
    std::atomic<uint64_t> recorded_packets(0);
    std::atomic<uint64_t> recorded_bytes(0);
    
    config.progress_callback = [&](int64_t duration_ms, int64_t file_size) {
        recorded_bytes = file_size;
    };
    
    config.file_completed_callback = [&](const std::string& filename) {
        LOG_INFO("Recording completed: %s", filename.c_str());
        
        if (std::filesystem::exists(filename)) {
            auto file_size = std::filesystem::file_size(filename);
            LOG_INFO("Final file size: %llu bytes (%.2f MB)", 
                    file_size, static_cast<double>(file_size) / (1024 * 1024));
        }
    };
    
    // 开始录制性能测试
    auto start_time = std::chrono::steady_clock::now();
    
    if (!recorder.StartRecord(config)) {
        LOG_ERROR("Failed to start recording for performance test");
        return;
    }
    
    auto record_start = std::chrono::steady_clock::now();
    auto start_delay = std::chrono::duration_cast<std::chrono::milliseconds>(
        record_start - start_time).count();
    
    LOG_INFO("Recording start delay: %lld ms", start_delay);
    LOG_INFO("Recording performance test running...");
    
    // 监控录制性能
    auto last_report = record_start;
    uint64_t last_bytes = 0;
    
    while (recorder.IsRecording()) {
        auto now = std::chrono::steady_clock::now();
        
        // 每2秒报告一次
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_report).count() >= 2) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - record_start).count();
            
            RTSPStats stats = recorder.GetRecordStats();
            uint64_t current_bytes = recorded_bytes.load();
            
            double write_rate = static_cast<double>(current_bytes - last_bytes) / 2.0 / (1024 * 1024); // MB/s
            double avg_bitrate = (static_cast<double>(current_bytes) * 8.0) / (elapsed / 1000.0) / (1024 * 1024); // Mbps
            
            LOG_INFO("Recording at %lld ms:", elapsed);
            LOG_INFO("  Received packets: %llu", stats.packets_received);
            LOG_INFO("  File size: %llu bytes (%.2f MB)", current_bytes, static_cast<double>(current_bytes) / (1024 * 1024));
            LOG_INFO("  Write rate: %.2f MB/s", write_rate);
            LOG_INFO("  Average bitrate: %.2f Mbps", avg_bitrate);
            LOG_INFO("  Dropped packets: %llu", stats.dropped_packets);
            
            last_report = now;
            last_bytes = current_bytes;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    auto end_time = std::chrono::steady_clock::now();
    auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();
    
    // 最终性能统计
    RTSPStats final_stats = recorder.GetRecordStats();
    uint64_t final_bytes = recorded_bytes.load();
    
    LOG_INFO("Recording performance results:");
    LOG_INFO("  Total time: %lld ms", total_time);
    LOG_INFO("  Total packets received: %llu", final_stats.packets_received);
    LOG_INFO("  Total bytes recorded: %llu (%.2f MB)", final_bytes, static_cast<double>(final_bytes) / (1024 * 1024));
    LOG_INFO("  Average recording rate: %.2f MB/s", static_cast<double>(final_bytes) / (total_time / 1000.0) / (1024 * 1024));
    LOG_INFO("  Packet loss rate: %.2f%%", 
            final_stats.packets_received > 0 ? 
            (static_cast<double>(final_stats.dropped_packets) / final_stats.packets_received * 100.0) : 0.0);
}

void testConcurrentRecording() {
    LOG_INFO("Testing concurrent RTSP recording performance...");
    
    std::filesystem::create_directories("test_output");
    
    const int num_recorders = 3;
    std::vector<std::unique_ptr<XRTSPRecorder>> recorders;
    std::vector<std::atomic<bool>> completed(num_recorders);
    
    // 创建多个并发录制器
    for (int i = 0; i < num_recorders; ++i) {
        auto recorder = std::make_unique<XRTSPRecorder>();
        
        RTSPRecordConfig config;
        config.rtsp_url = "rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mov";
        config.output_file = "test_output/concurrent_" + std::to_string(i) + ".mp4";
        config.output_format = "mp4";
        config.max_duration_ms = 15000; // 15秒
        
        config.rtsp_config.timeout_ms = 10000;
        config.rtsp_config.enable_tcp = true;
        
        int recorder_id = i;
        completed[i] = false;
        
        config.file_completed_callback = [&completed, recorder_id](const std::string& filename) {
            LOG_INFO("Concurrent recorder %d completed: %s", recorder_id, filename.c_str());
            completed[recorder_id] = true;
        };
        
        recorders.push_back(std::move(recorder));
    }
    
    // 同时启动所有录制器
    auto start_time = std::chrono::steady_clock::now();
    
    for (int i = 0; i < num_recorders; ++i) {
        RTSPRecordConfig config;
        config.rtsp_url = "rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mov";
        config.output_file = "test_output/concurrent_" + std::to_string(i) + ".mp4";
        config.output_format = "mp4";
        config.max_duration_ms = 15000;
        config.rtsp_config.timeout_ms = 10000;
        config.rtsp_config.enable_tcp = true;
        
        int recorder_id = i;
        config.file_completed_callback = [&completed, recorder_id](const std::string& filename) {
            LOG_INFO("Concurrent recorder %d completed: %s", recorder_id, filename.c_str());
            completed[recorder_id] = true;
        };
        
        if (recorders[i]->StartRecord(config)) {
            LOG_INFO("Started concurrent recorder %d", i);
        } else {
            LOG_ERROR("Failed to start concurrent recorder %d", i);
        }
    }
    
    LOG_INFO("All concurrent recorders started, monitoring performance...");
    
    // 监控并发性能
    while (true) {
        bool all_completed = true;
        for (int i = 0; i < num_recorders; ++i) {
            if (recorders[i]->IsRecording()) {
                all_completed = false;
                break;
            }
        }
        
        if (all_completed) {
            break;
        }
        
        // 每3秒报告一次性能
        std::this_thread::sleep_for(std::chrono::seconds(3));
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
        
        LOG_INFO("Concurrent recording at %lld seconds:", elapsed);
        
        uint64_t total_packets = 0;
        uint64_t total_bytes = 0;
        uint64_t total_dropped = 0;
        
        for (int i = 0; i < num_recorders; ++i) {
            RTSPStats stats = recorders[i]->GetRecordStats();
            total_packets += stats.packets_received;
            total_bytes += stats.bytes_received;
            total_dropped += stats.dropped_packets;
            
            LOG_INFO("  Recorder %d: %llu packets, %llu KB, %llu dropped", 
                    i, stats.packets_received, stats.bytes_received / 1024, stats.dropped_packets);
        }
        
        LOG_INFO("  Total: %llu packets, %llu KB, %llu dropped", 
                total_packets, total_bytes / 1024, total_dropped);
        LOG_INFO("  Average per recorder: %.2f packets, %.2f KB", 
                static_cast<double>(total_packets) / num_recorders,
                static_cast<double>(total_bytes) / num_recorders / 1024);
    }
    
    auto end_time = std::chrono::steady_clock::now();
    auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();
    
    LOG_INFO("Concurrent recording completed in %lld ms", total_time);
    
    // 清理
    for (auto& recorder : recorders) {
        recorder->StopRecord();
    }
}

int main(int argc, char* argv[]) {
    LOG_INFO("Starting RTSP performance tests");
    
    try {
        // 客户端性能测试
        testClientPerformance();
        
        LOG_INFO("");
        
        // 录制器性能测试
        testRecorderPerformance();
        
        LOG_INFO("");
        
        // 并发录制性能测试
        testConcurrentRecording();
        
        LOG_INFO("All RTSP performance tests completed!");
        
    } catch (const std::exception& e) {
        LOG_ERROR("Performance test failed with exception: %s", e.what());
        return 1;
    }
    
    return 0;
}
