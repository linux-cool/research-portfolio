#include "xdecode.h"
#include "xencode.h"
#include "avframe_manager.h"
#include <iostream>
#include <vector>

void generateTestFrame(AVFrame* frame, int frame_number) {
    if (!frame || !frame->data[0]) return;
    
    int width = frame->width;
    int height = frame->height;
    
    // 生成动态测试图案
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // Y分量 - 移动的渐变
            uint8_t Y = static_cast<uint8_t>((x + y + frame_number * 2) % 256);
            frame->data[0][y * frame->linesize[0] + x] = Y;
            
            // UV分量 - 每2x2像素一个
            if (x % 2 == 0 && y % 2 == 0) {
                int uv_x = x / 2;
                int uv_y = y / 2;
                frame->data[1][uv_y * frame->linesize[1] + uv_x] = 128; // U
                frame->data[2][uv_y * frame->linesize[2] + uv_x] = 128; // V
            }
        }
    }
    
    frame->pts = frame_number;
}

int main(int argc, char* argv[]) {
    LOG_INFO("Starting encode-decode loop test");
    
    // 创建编码器
    auto encoder = XEncodeFactory::Create(CodecType::H264);
    if (!encoder) {
        LOG_ERROR("Failed to create H.264 encoder");
        return -1;
    }
    
    // 创建解码器
    auto decoder = XDecodeFactory::Create(CodecType::H264);
    if (!decoder) {
        LOG_ERROR("Failed to create H.264 decoder");
        return -1;
    }
    
    // 配置编码器
    EncodeConfig encode_config;
    encode_config.width = 640;
    encode_config.height = 480;
    encode_config.pixel_format = AV_PIX_FMT_YUV420P;
    encode_config.frame_rate = {25, 1};
    encode_config.time_base = {1, 25};
    encode_config.bit_rate = 1000000;  // 1Mbps
    encode_config.gop_size = 25;
    encode_config.preset = QualityPreset::FAST;
    
    // 存储编码后的包
    std::vector<AVPacket*> encoded_packets;
    
    encode_config.packet_callback = [&encoded_packets](const AVPacket* packet) {
        AVPacket* pkt = av_packet_alloc();
        if (pkt) {
            av_packet_ref(pkt, packet);
            encoded_packets.push_back(pkt);
            LOG_INFO("Encoded packet: size=%d, pts=%ld, flags=0x%x",
                     pkt->size, pkt->pts, pkt->flags);
        }
    };
    
    if (!encoder->Init(encode_config)) {
        LOG_ERROR("Failed to initialize encoder");
        return -1;
    }
    
    // 配置解码器
    DecodeConfig decode_config;
    decode_config.codec_type = CodecType::H264;
    decode_config.pixel_format = AV_PIX_FMT_YUV420P;
    decode_config.enable_multithreading = true;
    decode_config.thread_count = 2;
    
    size_t frames_decoded = 0;
    decode_config.frame_callback = [&frames_decoded](AVFrame* frame) {
        frames_decoded++;
        LOG_INFO("Decoded frame %zu: %dx%d, pts=%ld",
                 frames_decoded, frame->width, frame->height, frame->pts);
    };
    
    if (!decoder->Init(decode_config)) {
        LOG_ERROR("Failed to initialize decoder");
        return -1;
    }
    
    LOG_INFO("Encoder: %s", encoder->GetEncoderInfo().c_str());
    LOG_INFO("Decoder: %s", decoder->GetDecoderInfo().c_str());
    
    // 创建帧管理器
    AVFrameManager frame_manager(5);
    
    // 编码阶段
    LOG_INFO("Starting encoding phase...");
    const int NUM_FRAMES = 25;  // 1秒@25fps
    
    auto encode_start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < NUM_FRAMES; ++i) {
        AVFrame* frame = frame_manager.AllocFrame(640, 480, AV_PIX_FMT_YUV420P);
        if (!frame) {
            LOG_ERROR("Failed to allocate frame %d", i);
            break;
        }
        
        generateTestFrame(frame, i);
        
        if (!encoder->Encode(frame)) {
            LOG_ERROR("Failed to encode frame %d", i);
            frame_manager.ReleaseFrame(frame);
            break;
        }
        
        frame_manager.ReleaseFrame(frame);
    }
    
    // 刷新编码器
    encoder->Flush();
    
    auto encode_end = std::chrono::high_resolution_clock::now();
    auto encode_duration = std::chrono::duration_cast<std::chrono::milliseconds>(encode_end - encode_start);
    
    LOG_INFO("Encoding completed: %zu packets in %ld ms", encoded_packets.size(), encode_duration.count());
    
    // 解码阶段
    LOG_INFO("Starting decoding phase...");
    
    auto decode_start = std::chrono::high_resolution_clock::now();
    
    for (AVPacket* packet : encoded_packets) {
        if (!decoder->Decode(packet)) {
            LOG_ERROR("Failed to decode packet");
        }
    }
    
    // 刷新解码器
    decoder->Flush();
    
    auto decode_end = std::chrono::high_resolution_clock::now();
    auto decode_duration = std::chrono::duration_cast<std::chrono::milliseconds>(decode_end - decode_start);
    
    LOG_INFO("Decoding completed: %zu frames in %ld ms", frames_decoded, decode_duration.count());
    
    // 获取统计信息
    auto encode_stats = encoder->GetStats();
    auto decode_stats = decoder->GetStats();
    
    LOG_INFO("Encode-Decode Loop Test Results:");
    LOG_INFO("Encoding:");
    LOG_INFO("  Frames encoded: %lu", encode_stats.frames_encoded);
    LOG_INFO("  Bytes encoded: %lu", encode_stats.bytes_encoded);
    LOG_INFO("  Average FPS: %.2f", encode_stats.avg_fps);
    LOG_INFO("  Time: %ld ms", encode_duration.count());
    
    LOG_INFO("Decoding:");
    LOG_INFO("  Frames decoded: %lu", decode_stats.frames_decoded);
    LOG_INFO("  Bytes decoded: %lu", decode_stats.bytes_decoded);
    LOG_INFO("  Average FPS: %.2f", decode_stats.avg_fps);
    LOG_INFO("  Time: %ld ms", decode_duration.count());
    
    // 验证结果
    bool success = (encode_stats.frames_encoded == NUM_FRAMES) && 
                   (decode_stats.frames_decoded == encode_stats.frames_encoded);
    
    LOG_INFO("Loop test result: %s", success ? "SUCCESS" : "FAILED");
    
    // 清理
    for (AVPacket* packet : encoded_packets) {
        av_packet_free(&packet);
    }
    
    return success ? 0 : -1;
}
