#include "xrtsp.h"

void testRTSPConnection() {
    LOG_INFO("Testing RTSP connection...");
    
    // 测试URL列表（这些是示例URL，实际测试时需要有效的RTSP流）
    std::vector<std::string> test_urls = {
        "rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mov",
        "rtsp://184.72.239.149/vod/mp4:BigBuckBunny_175k.mov",
        "rtsp://demo:demo@ipvmdemo.dyndns.org:5541/onvif-media/media.amp?profile=profile_1_h264",
        "rtsp://admin:admin@192.168.1.100:554/stream1",  // 示例本地摄像头
        "rtsp://invalid.url.test:554/stream"  // 无效URL测试
    };
    
    for (const auto& url : test_urls) {
        LOG_INFO("Testing URL: %s", url.c_str());
        
        // 验证URL格式
        if (!RTSPUtils::ValidateURL(url)) {
            LOG_INFO("  Invalid URL format");
            continue;
        }
        
        // 测试连接
        bool connected = RTSPUtils::TestConnection(url, 5000);
        LOG_INFO("  Connection test: %s", connected ? "SUCCESS" : "FAILED");
        
        if (connected) {
            // 获取流信息
            MediaInfo info = RTSPUtils::GetStreamInfo(url, 10000);
            if (info.is_valid) {
                LOG_INFO("  Stream info:");
                LOG_INFO("    Format: %s", info.format_name.c_str());
                LOG_INFO("    Duration: %lld us", info.duration_us);
                LOG_INFO("    Bitrate: %lld bps", info.bit_rate);
                LOG_INFO("    Streams: %zu", info.streams.size());
                
                for (const auto& stream : info.streams) {
                    if (stream.type == AVMEDIA_TYPE_VIDEO) {
                        LOG_INFO("    Video: %dx%d, %.2f fps, %s", 
                                stream.width, stream.height,
                                av_q2d(stream.frame_rate),
                                stream.codec_name.c_str());
                    } else if (stream.type == AVMEDIA_TYPE_AUDIO) {
                        LOG_INFO("    Audio: %d Hz, %d channels, %s",
                                stream.sample_rate, stream.channels,
                                stream.codec_name.c_str());
                    }
                }
            } else {
                LOG_INFO("  Failed to get stream info");
            }
        }
        
        LOG_INFO("");
    }
}

void testRTSPClient() {
    LOG_INFO("Testing RTSP client...");
    
    // 使用公开的测试RTSP流
    std::string test_url = "rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mov";
    
    XRTSPClient client;
    
    // 配置RTSP客户端
    RTSPConfig config;
    config.url = test_url;
    config.timeout_ms = 10000;
    config.enable_tcp = true;
    config.auto_reconnect = true;
    config.max_reconnect_attempts = 3;
    
    // 设置回调函数
    int packet_count = 0;
    config.packet_callback = [&packet_count](AVPacket* packet, int stream_index) {
        packet_count++;
        if (packet_count % 100 == 0) {
            LOG_INFO("Received %d packets, stream=%d, size=%d", 
                    packet_count, stream_index, packet->size);
        }
    };
    
    config.state_callback = [](RTSPState state) {
        const char* state_names[] = {
            "DISCONNECTED", "CONNECTING", "CONNECTED", "PLAYING", "PAUSED", "ERROR"
        };
        LOG_INFO("RTSP state changed: %s", state_names[static_cast<int>(state)]);
    };
    
    config.error_callback = [](const std::string& error) {
        LOG_ERROR("RTSP error: %s", error.c_str());
    };
    
    // 连接RTSP流
    if (!client.Connect(config)) {
        LOG_ERROR("Failed to connect to RTSP stream");
        return;
    }
    
    LOG_INFO("RTSP connected successfully");
    
    // 获取媒体信息
    MediaInfo media_info = client.GetMediaInfo();
    LOG_INFO("Media info:");
    LOG_INFO("  Format: %s", media_info.format_name.c_str());
    LOG_INFO("  Streams: %zu", media_info.streams.size());
    
    // 开始播放
    if (!client.Play()) {
        LOG_ERROR("Failed to start playback");
        return;
    }
    
    LOG_INFO("RTSP playback started");
    
    // 运行10秒
    auto start_time = std::chrono::steady_clock::now();
    while (std::chrono::duration_cast<std::chrono::seconds>(
           std::chrono::steady_clock::now() - start_time).count() < 10) {
        
        // 每秒输出统计信息
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        RTSPStats stats = client.GetStats();
        LOG_INFO("Stats: packets=%llu, bytes=%llu KB, video=%llu, audio=%llu, dropped=%llu, bitrate=%.2f kbps",
                stats.packets_received, stats.bytes_received / 1024,
                stats.video_packets, stats.audio_packets, stats.dropped_packets,
                stats.avg_bitrate_kbps);
    }
    
    // 暂停播放
    LOG_INFO("Pausing playback...");
    client.Pause();
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // 恢复播放
    LOG_INFO("Resuming playback...");
    client.Play();
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    // 停止播放
    LOG_INFO("Stopping playback...");
    client.Stop();
    
    // 断开连接
    LOG_INFO("Disconnecting...");
    client.Disconnect();
    
    // 最终统计
    RTSPStats final_stats = client.GetStats();
    LOG_INFO("Final stats:");
    LOG_INFO("  Total packets: %llu", final_stats.packets_received);
    LOG_INFO("  Total bytes: %llu KB", final_stats.bytes_received / 1024);
    LOG_INFO("  Video packets: %llu", final_stats.video_packets);
    LOG_INFO("  Audio packets: %llu", final_stats.audio_packets);
    LOG_INFO("  Dropped packets: %llu", final_stats.dropped_packets);
    LOG_INFO("  Connection time: %lld ms", final_stats.connection_time_ms);
    LOG_INFO("  Reconnect count: %d", final_stats.reconnect_count);
    LOG_INFO("  Average bitrate: %.2f kbps", final_stats.avg_bitrate_kbps);
}

void testRTSPReconnection() {
    LOG_INFO("Testing RTSP reconnection...");
    
    // 使用一个可能不稳定的测试URL
    std::string test_url = "rtsp://demo:demo@ipvmdemo.dyndns.org:5541/onvif-media/media.amp";
    
    XRTSPClient client;
    
    RTSPConfig config;
    config.url = test_url;
    config.timeout_ms = 5000;
    config.auto_reconnect = true;
    config.max_reconnect_attempts = 5;
    config.reconnect_interval_ms = 2000;
    
    int reconnect_count = 0;
    config.state_callback = [&reconnect_count](RTSPState state) {
        if (state == RTSPState::ERROR) {
            reconnect_count++;
            LOG_INFO("Connection lost, will attempt reconnection #%d", reconnect_count);
        } else if (state == RTSPState::CONNECTED) {
            LOG_INFO("Reconnection successful");
        }
    };
    
    // 尝试连接
    if (client.Connect(config)) {
        LOG_INFO("Initial connection successful");
        
        if (client.Play()) {
            LOG_INFO("Playback started, monitoring for 30 seconds...");
            
            // 监控30秒，观察重连行为
            auto start_time = std::chrono::steady_clock::now();
            while (std::chrono::duration_cast<std::chrono::seconds>(
                   std::chrono::steady_clock::now() - start_time).count() < 30) {
                
                std::this_thread::sleep_for(std::chrono::seconds(2));
                
                RTSPStats stats = client.GetStats();
                LOG_INFO("State: %d, Packets: %llu, Reconnects: %d",
                        static_cast<int>(client.GetState()),
                        stats.packets_received, stats.reconnect_count);
            }
        }
    } else {
        LOG_INFO("Initial connection failed (expected for demo URL)");
    }
    
    client.Disconnect();
}

int main(int argc, char* argv[]) {
    LOG_INFO("Starting RTSP client tests");
    
    try {
        // 测试RTSP连接
        testRTSPConnection();
        
        LOG_INFO("");
        
        // 测试RTSP客户端
        testRTSPClient();
        
        LOG_INFO("");
        
        // 测试重连功能
        testRTSPReconnection();
        
        LOG_INFO("All RTSP client tests completed!");
        
    } catch (const std::exception& e) {
        LOG_ERROR("Test failed with exception: %s", e.what());
        return 1;
    }
    
    return 0;
}
