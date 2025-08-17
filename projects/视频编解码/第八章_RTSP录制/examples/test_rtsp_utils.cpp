#include "xrtsp.h"

void testURLParsing() {
    LOG_INFO("Testing RTSP URL parsing...");
    
    struct URLTest {
        std::string url;
        bool should_succeed;
        std::string expected_host;
        int expected_port;
        std::string expected_path;
    };
    
    std::vector<URLTest> tests = {
        {"rtsp://192.168.1.100:554/stream1", true, "192.168.1.100", 554, "/stream1"},
        {"rtsp://camera.local/live", true, "camera.local", 554, "/live"},
        {"rtsp://admin:pass@192.168.1.100:8554/stream", true, "admin:pass@192.168.1.100", 8554, "/stream"},
        {"rtsp://demo.server.com:1935/app/stream", true, "demo.server.com", 1935, "/app/stream"},
        {"rtsp://server.com", true, "server.com", 554, "/"},
        {"http://invalid.com/stream", false, "", 0, ""},
        {"rtsp://", false, "", 0, ""},
        {"invalid_url", false, "", 0, ""},
        {"rtsp://host:invalid_port/stream", false, "", 0, ""}
    };
    
    int passed = 0;
    int total = tests.size();
    
    for (const auto& test : tests) {
        std::string host, path;
        int port;
        
        bool result = RTSPUtils::ParseURL(test.url, host, port, path);
        
        bool test_passed = (result == test.should_succeed);
        if (test.should_succeed && result) {
            test_passed = test_passed && 
                         (host == test.expected_host) &&
                         (port == test.expected_port) &&
                         (path == test.expected_path);
        }
        
        if (test_passed) {
            passed++;
            LOG_INFO("  ✓ %s", test.url.c_str());
            if (result) {
                LOG_INFO("    Host: %s, Port: %d, Path: %s", host.c_str(), port, path.c_str());
            }
        } else {
            LOG_ERROR("  ✗ %s", test.url.c_str());
            if (result) {
                LOG_ERROR("    Got: Host=%s, Port=%d, Path=%s", host.c_str(), port, path.c_str());
                LOG_ERROR("    Expected: Host=%s, Port=%d, Path=%s", 
                         test.expected_host.c_str(), test.expected_port, test.expected_path.c_str());
            }
        }
    }
    
    LOG_INFO("URL parsing test: %d/%d passed", passed, total);
}

void testURLValidation() {
    LOG_INFO("Testing RTSP URL validation...");
    
    std::vector<std::pair<std::string, bool>> tests = {
        {"rtsp://192.168.1.100:554/stream1", true},
        {"rtsp://camera.local/live", true},
        {"rtsp://demo.server.com:1935/app/stream", true},
        {"rtsp://server.com", true},
        {"http://invalid.com/stream", false},
        {"rtsp://", false},
        {"invalid_url", false},
        {"rtsp://host:invalid_port/stream", false},
        {"rtsp://valid.host:554/path/to/stream?param=value", true},
        {"rtsp://user:pass@host:554/stream", true}
    };
    
    int passed = 0;
    int total = tests.size();
    
    for (const auto& [url, expected] : tests) {
        bool result = RTSPUtils::ValidateURL(url);
        
        if (result == expected) {
            passed++;
            LOG_INFO("  ✓ %s -> %s", url.c_str(), result ? "VALID" : "INVALID");
        } else {
            LOG_ERROR("  ✗ %s -> %s (expected %s)", 
                     url.c_str(), result ? "VALID" : "INVALID", expected ? "VALID" : "INVALID");
        }
    }
    
    LOG_INFO("URL validation test: %d/%d passed", passed, total);
}

void testConnectionTesting() {
    LOG_INFO("Testing RTSP connection testing...");
    
    std::vector<std::pair<std::string, std::string>> test_urls = {
        {"rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mov", "Public test stream"},
        {"rtsp://184.72.239.149/vod/mp4:BigBuckBunny_175k.mov", "Alternative test stream"},
        {"rtsp://demo:demo@ipvmdemo.dyndns.org:5541/onvif-media/media.amp", "Demo camera"},
        {"rtsp://invalid.server.test:554/stream", "Invalid server"},
        {"rtsp://192.168.1.999:554/stream", "Invalid IP"},
        {"rtsp://timeout.test:554/stream", "Timeout test"}
    };
    
    for (const auto& [url, description] : test_urls) {
        LOG_INFO("Testing: %s (%s)", description.c_str(), url.c_str());
        
        auto start_time = std::chrono::high_resolution_clock::now();
        bool connected = RTSPUtils::TestConnection(url, 5000);
        auto end_time = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time).count();
        
        LOG_INFO("  Result: %s (took %lld ms)", 
                connected ? "SUCCESS" : "FAILED", duration);
        
        if (connected) {
            // 如果连接成功，尝试获取流信息
            LOG_INFO("  Getting stream info...");
            MediaInfo info = RTSPUtils::GetStreamInfo(url, 10000);
            
            if (info.is_valid) {
                LOG_INFO("    Format: %s", info.format_name.c_str());
                LOG_INFO("    Duration: %lld us", info.duration_us);
                LOG_INFO("    Bitrate: %lld bps", info.bit_rate);
                LOG_INFO("    Streams: %zu", info.streams.size());
                
                for (const auto& stream : info.streams) {
                    if (stream.type == AVMEDIA_TYPE_VIDEO) {
                        LOG_INFO("      Video: %dx%d, %.2f fps, %s", 
                                stream.width, stream.height,
                                av_q2d(stream.frame_rate),
                                stream.codec_name.c_str());
                    } else if (stream.type == AVMEDIA_TYPE_AUDIO) {
                        LOG_INFO("      Audio: %d Hz, %d channels, %s",
                                stream.sample_rate, stream.channels,
                                stream.codec_name.c_str());
                    }
                }
            } else {
                LOG_INFO("    Failed to get stream info");
            }
        }
        
        LOG_INFO("");
    }
}

void testFileNameGeneration() {
    LOG_INFO("Testing file name generation...");
    
    struct FileNameTest {
        std::string base_name;
        int sequence;
        int64_t timestamp;
        std::string expected_pattern;
    };
    
    std::vector<FileNameTest> tests = {
        {"output", 0, 0, "output.mp4"},
        {"output.avi", 0, 0, "output.avi"},
        {"video", 1, 0, "video_1.mp4"},
        {"video.mkv", 5, 0, "video_5.mkv"},
        {"stream", 0, 1234567890, "stream_1234567890.mp4"},
        {"record.mp4", 3, 1234567890, "record_3_1234567890.mp4"},
        {"test/output", 0, 0, "test/output.mp4"},
        {"path/to/file.flv", 2, 9876543210, "path/to/file_2_9876543210.flv"}
    };
    
    int passed = 0;
    int total = tests.size();
    
    for (const auto& test : tests) {
        std::string result = RTSPUtils::GenerateFileName(test.base_name, test.sequence, test.timestamp);
        
        // 简单的模式匹配检查
        bool test_passed = true;
        
        // 检查基础名称
        if (test.base_name.find('.') == std::string::npos) {
            // 没有扩展名，应该添加.mp4
            test_passed = test_passed && (result.find(".mp4") != std::string::npos);
        }
        
        // 检查序列号
        if (test.sequence > 0) {
            std::string seq_str = "_" + std::to_string(test.sequence);
            test_passed = test_passed && (result.find(seq_str) != std::string::npos);
        }
        
        // 检查时间戳
        if (test.timestamp > 0) {
            std::string ts_str = "_" + std::to_string(test.timestamp);
            test_passed = test_passed && (result.find(ts_str) != std::string::npos);
        }
        
        if (test_passed) {
            passed++;
            LOG_INFO("  ✓ %s -> %s", test.base_name.c_str(), result.c_str());
        } else {
            LOG_ERROR("  ✗ %s -> %s", test.base_name.c_str(), result.c_str());
        }
    }
    
    LOG_INFO("File name generation test: %d/%d passed", passed, total);
}

void testUtilsPerformance() {
    LOG_INFO("Testing RTSP utils performance...");
    
    const int iterations = 10000;
    
    // URL解析性能
    {
        std::string test_url = "rtsp://192.168.1.100:554/stream1";
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < iterations; ++i) {
            std::string host, path;
            int port;
            RTSPUtils::ParseURL(test_url, host, port, path);
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - start_time).count();
        
        LOG_INFO("URL parsing: %d operations in %lld μs (%.2f μs/op)", 
                iterations, duration, static_cast<double>(duration) / iterations);
    }
    
    // URL验证性能
    {
        std::string test_url = "rtsp://camera.local:8554/live/stream";
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < iterations; ++i) {
            RTSPUtils::ValidateURL(test_url);
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - start_time).count();
        
        LOG_INFO("URL validation: %d operations in %lld μs (%.2f μs/op)", 
                iterations, duration, static_cast<double>(duration) / iterations);
    }
    
    // 文件名生成性能
    {
        std::string base_name = "output_file";
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < iterations; ++i) {
            RTSPUtils::GenerateFileName(base_name, i % 100, 1234567890 + i);
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - start_time).count();
        
        LOG_INFO("File name generation: %d operations in %lld μs (%.2f μs/op)", 
                iterations, duration, static_cast<double>(duration) / iterations);
    }
    
    LOG_INFO("Performance test completed");
}

int main(int argc, char* argv[]) {
    LOG_INFO("Starting RTSP utils tests");
    
    try {
        // URL解析测试
        testURLParsing();
        
        LOG_INFO("");
        
        // URL验证测试
        testURLValidation();
        
        LOG_INFO("");
        
        // 连接测试
        testConnectionTesting();
        
        LOG_INFO("");
        
        // 文件名生成测试
        testFileNameGeneration();
        
        LOG_INFO("");
        
        // 性能测试
        testUtilsPerformance();
        
        LOG_INFO("All RTSP utils tests completed!");
        
    } catch (const std::exception& e) {
        LOG_ERROR("Test failed with exception: %s", e.what());
        return 1;
    }
    
    return 0;
}
