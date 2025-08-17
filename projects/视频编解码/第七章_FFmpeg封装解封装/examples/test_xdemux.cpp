#include "xdemux.h"
#include <iostream>

void testFormatSupport() {
    LOG_INFO("Testing format support...");
    
    auto input_formats = XDemuxFactory::GetSupportedFormats();
    LOG_INFO("Supported input formats (%zu):", input_formats.size());
    
    // 显示前20个格式
    size_t count = 0;
    for (const auto& format : input_formats) {
        if (count++ < 20) {
            LOG_INFO("  - %s", format.c_str());
        }
    }
    if (input_formats.size() > 20) {
        LOG_INFO("  ... and %zu more", input_formats.size() - 20);
    }
    
    auto output_formats = XMuxFactory::GetSupportedFormats();
    LOG_INFO("Supported output formats (%zu):", output_formats.size());
    
    // 显示前20个格式
    count = 0;
    for (const auto& format : output_formats) {
        if (count++ < 20) {
            LOG_INFO("  - %s", format.c_str());
        }
    }
    if (output_formats.size() > 20) {
        LOG_INFO("  ... and %zu more", output_formats.size() - 20);
    }
}

void testFormatDetection() {
    LOG_INFO("Testing format detection...");
    
    std::vector<std::pair<std::string, std::string>> test_files = {
        {"test.mp4", "mp4"},
        {"video.avi", "avi"},
        {"movie.mkv", "matroska"},
        {"clip.mov", "mov"},
        {"stream.ts", "mpegts"},
        {"video.webm", "webm"},
        {"unknown.xyz", "xyz"}
    };
    
    for (const auto& [filename, expected] : test_files) {
        std::string detected = XDemuxFactory::DetectFormat(filename);
        bool correct = (detected == expected);
        LOG_INFO("  %s -> %s (%s)", filename.c_str(), detected.c_str(), 
                 correct ? "PASS" : "FAIL");
    }
}

void testDemuxerCreation() {
    LOG_INFO("Testing demuxer creation...");
    
    // 测试空文件名
    auto demuxer1 = XDemuxFactory::Create("");
    LOG_INFO("Empty filename: %s", demuxer1 ? "CREATED" : "FAILED (expected)");
    
    // 测试不存在的文件
    auto demuxer2 = XDemuxFactory::Create("nonexistent.mp4");
    LOG_INFO("Nonexistent file: %s", demuxer2 ? "CREATED" : "FAILED (expected)");
    
    // 测试有效的解封装器创建（不打开文件）
    auto demuxer3 = XDemuxFactory::Create("test.mp4");  // 文件不存在，但可以创建对象
    LOG_INFO("Valid demuxer object: %s", demuxer3 ? "CREATED" : "FAILED");
}

void testMuxerCreation() {
    LOG_INFO("Testing muxer creation...");
    
    std::vector<std::string> test_formats = {
        "mp4", "avi", "mkv", "mov", "webm", "flv", "invalid_format"
    };
    
    for (const auto& format : test_formats) {
        auto muxer = XMuxFactory::Create(format);
        LOG_INFO("  %s muxer: %s", format.c_str(), 
                 muxer ? "CREATED" : "FAILED");
    }
}

void testBasicDemuxing() {
    LOG_INFO("Testing basic demuxing...");
    
    // 创建一个测试文件路径（实际不存在）
    std::string test_file = "test_video.mp4";
    
    auto demuxer = XDemuxFactory::Create(test_file);
    if (!demuxer) {
        LOG_ERROR("Failed to create demuxer");
        return;
    }
    
    DemuxConfig config;
    config.filename = test_file;
    config.enable_video = true;
    config.enable_audio = true;
    
    size_t packets_received = 0;
    config.packet_callback = [&packets_received](AVPacket* packet, int stream_index) {
        packets_received++;
        LOG_INFO("Received packet: stream=%d, size=%d, pts=%ld",
                 stream_index, packet->size, packet->pts);
    };
    
    config.error_callback = [](const std::string& error) {
        LOG_ERROR("Demux error: %s", error.c_str());
    };
    
    // 尝试打开（会失败，因为文件不存在）
    if (demuxer->Open(config)) {
        LOG_INFO("Demuxer opened successfully");
        
        MediaInfo info = demuxer->GetMediaInfo();
        LOG_INFO("Media info:");
        LOG_INFO("  Format: %s", info.format_name.c_str());
        LOG_INFO("  Duration: %.2fs", info.duration_us / 1000000.0);
        LOG_INFO("  Streams: %zu", info.streams.size());
        
        // 尝试读取一些包
        AVPacket* packet = av_packet_alloc();
        for (int i = 0; i < 5; ++i) {
            if (!demuxer->ReadPacket(packet)) {
                break;
            }
            av_packet_unref(packet);
        }
        av_packet_free(&packet);
        
        // 获取统计信息
        auto stats = demuxer->GetStats();
        LOG_INFO("Demux statistics:");
        LOG_INFO("  Packets read: %lu", stats.packets_read);
        LOG_INFO("  Bytes read: %lu", stats.bytes_read);
        LOG_INFO("  Video packets: %lu", stats.video_packets);
        LOG_INFO("  Audio packets: %lu", stats.audio_packets);
        
    } else {
        LOG_INFO("Failed to open demuxer (expected - file doesn't exist)");
    }
}

void testBasicMuxing() {
    LOG_INFO("Testing basic muxing...");
    
    auto muxer = XMuxFactory::Create("mp4");
    if (!muxer) {
        LOG_ERROR("Failed to create MP4 muxer");
        return;
    }
    
    MuxConfig config;
    config.filename = "test_output.mp4";
    config.format_name = "mp4";
    config.enable_video = true;
    config.video_codec = CodecType::H264;
    config.video_width = 1280;
    config.video_height = 720;
    config.video_frame_rate = {30, 1};
    config.video_bit_rate = 2000000;
    
    config.error_callback = [](const std::string& error) {
        LOG_ERROR("Mux error: %s", error.c_str());
    };
    
    if (muxer->Open(config)) {
        LOG_INFO("Muxer opened successfully");
        LOG_INFO("Video stream index: %d", muxer->GetVideoStreamIndex());
        
        // 模拟写入一些包
        AVPacket* packet = av_packet_alloc();
        if (packet) {
            for (int i = 0; i < 3; ++i) {
                packet->data = nullptr;
                packet->size = 1000;  // 模拟包大小
                packet->pts = i;
                packet->dts = i;
                packet->flags = (i == 0) ? AV_PKT_FLAG_KEY : 0;
                
                if (muxer->WritePacket(packet, muxer->GetVideoStreamIndex())) {
                    LOG_INFO("Wrote packet %d", i);
                } else {
                    LOG_ERROR("Failed to write packet %d", i);
                }
            }
            av_packet_free(&packet);
        }
        
        // 获取统计信息
        auto stats = muxer->GetStats();
        LOG_INFO("Mux statistics:");
        LOG_INFO("  Packets written: %lu", stats.packets_written);
        LOG_INFO("  Bytes written: %lu", stats.bytes_written);
        LOG_INFO("  Video packets: %lu", stats.video_packets);
        
    } else {
        LOG_INFO("Failed to open muxer");
    }
}

void testSeekFunctionality() {
    LOG_INFO("Testing seek functionality...");
    
    auto demuxer = XDemuxFactory::Create("test_video.mp4");
    if (!demuxer) {
        return;
    }
    
    DemuxConfig config;
    config.filename = "test_video.mp4";
    
    if (demuxer->Open(config)) {
        // 测试跳转到不同时间点
        std::vector<int64_t> seek_times = {0, 5000000, 10000000, 30000000}; // 微秒
        
        for (int64_t time_us : seek_times) {
            if (demuxer->Seek(time_us)) {
                LOG_INFO("Seek to %.2fs: SUCCESS", time_us / 1000000.0);
            } else {
                LOG_INFO("Seek to %.2fs: FAILED", time_us / 1000000.0);
            }
        }
    } else {
        LOG_INFO("Cannot test seek - file doesn't exist");
    }
}

int main(int argc, char* argv[]) {
    LOG_INFO("Starting XDemux tests");
    
    try {
        testFormatSupport();
        testFormatDetection();
        testDemuxerCreation();
        testMuxerCreation();
        testBasicDemuxing();
        testBasicMuxing();
        testSeekFunctionality();
        
        LOG_INFO("All XDemux tests completed!");
        
    } catch (const std::exception& e) {
        LOG_ERROR("Test failed with exception: %s", e.what());
        return -1;
    }
    
    return 0;
}
