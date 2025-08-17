#include "xdemux.h"
#include <iostream>

int main(int argc, char* argv[]) {
    LOG_INFO("Starting XMux tests");
    
    // 测试MP4封装
    LOG_INFO("Testing MP4 muxing...");
    
    auto muxer = XMuxFactory::Create("mp4");
    if (!muxer) {
        LOG_ERROR("Failed to create MP4 muxer");
        return -1;
    }
    
    MuxConfig config;
    config.filename = "test_mux_output.mp4";
    config.format_name = "mp4";
    config.enable_video = true;
    config.video_codec = CodecType::H264;
    config.video_width = 640;
    config.video_height = 480;
    config.video_frame_rate = {25, 1};
    config.video_bit_rate = 1000000;
    
    if (!muxer->Open(config)) {
        LOG_ERROR("Failed to open MP4 muxer");
        return -1;
    }
    
    LOG_INFO("MP4 muxer opened successfully");
    LOG_INFO("Video stream index: %d", muxer->GetVideoStreamIndex());
    
    // 模拟写入视频包
    AVPacket* packet = av_packet_alloc();
    if (!packet) {
        LOG_ERROR("Failed to allocate packet");
        return -1;
    }
    
    const int NUM_PACKETS = 25;  // 1秒@25fps
    
    for (int i = 0; i < NUM_PACKETS; ++i) {
        // 模拟包数据
        packet->data = nullptr;
        packet->size = 1000 + (i % 100);  // 变化的包大小
        packet->pts = i * 40000;  // 40ms间隔 (微秒)
        packet->dts = packet->pts;
        packet->duration = 40000;
        packet->flags = (i % 10 == 0) ? AV_PKT_FLAG_KEY : 0;  // 每10帧一个关键帧
        
        if (!muxer->WritePacket(packet, muxer->GetVideoStreamIndex())) {
            LOG_ERROR("Failed to write packet %d", i);
            break;
        }
        
        if (i % 5 == 0) {
            LOG_INFO("Wrote packet %d (pts=%.2fs, key=%s)", 
                     i, packet->pts / 1000000.0,
                     (packet->flags & AV_PKT_FLAG_KEY) ? "yes" : "no");
        }
    }
    
    av_packet_free(&packet);
    
    // 获取统计信息
    auto stats = muxer->GetStats();
    
    LOG_INFO("MP4 muxing completed!");
    LOG_INFO("Statistics:");
    LOG_INFO("  Packets written: %lu", stats.packets_written);
    LOG_INFO("  Bytes written: %lu", stats.bytes_written);
    LOG_INFO("  Video packets: %lu", stats.video_packets);
    LOG_INFO("  Audio packets: %lu", stats.audio_packets);
    LOG_INFO("  Average write time: %.2f ms", stats.avg_write_time_ms);
    LOG_INFO("  Total time: %ld ms", stats.total_time_ms);
    
    // 测试其他格式
    LOG_INFO("Testing other formats...");
    
    std::vector<std::string> test_formats = {"avi", "mkv", "webm"};
    
    for (const auto& format : test_formats) {
        auto test_muxer = XMuxFactory::Create(format);
        if (test_muxer) {
            LOG_INFO("  %s muxer: CREATED", format.c_str());
            
            MuxConfig test_config;
            test_config.filename = "test_" + format + "_output." + format;
            test_config.format_name = format;
            test_config.enable_video = true;
            test_config.video_codec = CodecType::H264;
            test_config.video_width = 320;
            test_config.video_height = 240;
            test_config.video_frame_rate = {15, 1};
            test_config.video_bit_rate = 500000;
            
            if (test_muxer->Open(test_config)) {
                LOG_INFO("    %s muxer opened successfully", format.c_str());
            } else {
                LOG_INFO("    %s muxer failed to open", format.c_str());
            }
        } else {
            LOG_INFO("  %s muxer: FAILED", format.c_str());
        }
    }
    
    return 0;
}
