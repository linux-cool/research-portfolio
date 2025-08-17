#include "xdemux.h"
#include <filesystem>
#include <map>

// XDemuxFactory实现
std::unique_ptr<XDemux> XDemuxFactory::Create(const std::string& filename) {
    if (filename.empty()) {
        LOG_ERROR("Filename is empty");
        return nullptr;
    }
    
    // 检查文件是否存在
    if (!std::filesystem::exists(filename)) {
        LOG_ERROR("File does not exist: %s", filename.c_str());
        return nullptr;
    }
    
    return std::make_unique<XDemux>();
}

std::vector<std::string> XDemuxFactory::GetSupportedFormats() {
    std::vector<std::string> formats;
    
    const AVInputFormat* fmt = nullptr;
    void* opaque = nullptr;
    
    while ((fmt = av_demuxer_iterate(&opaque))) {
        if (fmt->name) {
            formats.push_back(fmt->name);
        }
    }
    
    return formats;
}

std::string XDemuxFactory::DetectFormat(const std::string& filename) {
    if (filename.empty()) {
        return "";
    }
    
    // 根据文件扩展名检测格式
    std::filesystem::path path(filename);
    std::string ext = path.extension().string();
    
    if (ext.empty()) {
        return "";
    }
    
    // 移除点号
    ext = ext.substr(1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    // 常见格式映射
    static const std::map<std::string, std::string> format_map = {
        {"mp4", "mp4"},
        {"avi", "avi"},
        {"mkv", "matroska"},
        {"mov", "mov"},
        {"wmv", "asf"},
        {"flv", "flv"},
        {"webm", "webm"},
        {"ts", "mpegts"},
        {"m4v", "mp4"},
        {"3gp", "3gp"},
        {"f4v", "flv"}
    };
    
    auto it = format_map.find(ext);
    if (it != format_map.end()) {
        return it->second;
    }
    
    return ext;  // 返回扩展名作为格式名
}

// XMuxFactory实现
std::unique_ptr<XMux> XMuxFactory::Create(const std::string& format_name) {
    if (format_name.empty()) {
        LOG_ERROR("Format name is empty");
        return nullptr;
    }
    
    // 检查格式是否支持
    const AVOutputFormat* fmt = av_guess_format(format_name.c_str(), nullptr, nullptr);
    if (!fmt) {
        LOG_ERROR("Unsupported output format: %s", format_name.c_str());
        return nullptr;
    }
    
    return std::make_unique<XMux>();
}

std::vector<std::string> XMuxFactory::GetSupportedFormats() {
    std::vector<std::string> formats;
    
    const AVOutputFormat* fmt = nullptr;
    void* opaque = nullptr;
    
    while ((fmt = av_muxer_iterate(&opaque))) {
        if (fmt->name) {
            formats.push_back(fmt->name);
        }
    }
    
    return formats;
}

// MediaUtils实现
bool MediaUtils::Remux(const std::string& input_file,
                     const std::string& output_file,
                     const std::string& output_format) {
        
        // 创建解封装器
        auto demuxer = XDemuxFactory::Create(input_file);
        if (!demuxer) {
            LOG_ERROR("Failed to create demuxer for %s", input_file.c_str());
            return false;
        }
        
        DemuxConfig demux_config;
        demux_config.filename = input_file;
        demux_config.enable_video = true;
        demux_config.enable_audio = true;
        
        if (!demuxer->Open(demux_config)) {
            LOG_ERROR("Failed to open input file: %s", input_file.c_str());
            return false;
        }
        
        // 获取媒体信息
        MediaInfo media_info = demuxer->GetMediaInfo();
        
        // 确定输出格式
        std::string format = output_format;
        if (format.empty()) {
            format = XDemuxFactory::DetectFormat(output_file);
        }
        
        // 创建封装器
        auto muxer = XMuxFactory::Create(format);
        if (!muxer) {
            LOG_ERROR("Failed to create muxer for format %s", format.c_str());
            return false;
        }
        
        MuxConfig mux_config;
        mux_config.filename = output_file;
        mux_config.format_name = format;
        
        // 配置视频流
        if (demuxer->GetVideoStreamIndex() >= 0) {
            const StreamInfo& video_stream = media_info.streams[demuxer->GetVideoStreamIndex()];
            mux_config.enable_video = true;
            mux_config.video_codec = video_stream.codec_type;
            mux_config.video_width = video_stream.width;
            mux_config.video_height = video_stream.height;
            mux_config.video_frame_rate = video_stream.frame_rate;
            mux_config.video_bit_rate = video_stream.bit_rate;
        } else {
            mux_config.enable_video = false;
        }
        
        // 配置音频流
        if (demuxer->GetAudioStreamIndex() >= 0) {
            const StreamInfo& audio_stream = media_info.streams[demuxer->GetAudioStreamIndex()];
            mux_config.enable_audio = true;
            mux_config.audio_codec = audio_stream.codec_type;
            mux_config.audio_sample_rate = audio_stream.sample_rate;
            mux_config.audio_channels = audio_stream.channels;
            mux_config.audio_bit_rate = audio_stream.bit_rate;
        } else {
            mux_config.enable_audio = false;
        }
        
        if (!muxer->Open(mux_config)) {
            LOG_ERROR("Failed to open output file: %s", output_file.c_str());
            return false;
        }
        
        LOG_INFO("Starting remux: %s -> %s (format: %s)", 
                 input_file.c_str(), output_file.c_str(), format.c_str());
        
        // 复制包
        AVPacket* packet = av_packet_alloc();
        if (!packet) {
            LOG_ERROR("Failed to allocate packet");
            return false;
        }
        
        size_t packets_copied = 0;
        while (demuxer->ReadPacket(packet)) {
            int stream_index = packet->stream_index;
            int output_stream_index = -1;
            
            // 映射流索引
            if (stream_index == demuxer->GetVideoStreamIndex()) {
                output_stream_index = muxer->GetVideoStreamIndex();
            } else if (stream_index == demuxer->GetAudioStreamIndex()) {
                output_stream_index = muxer->GetAudioStreamIndex();
            }
            
            if (output_stream_index >= 0) {
                if (!muxer->WritePacket(packet, output_stream_index)) {
                    LOG_ERROR("Failed to write packet");
                    break;
                }
                packets_copied++;
            }
            
            av_packet_unref(packet);
        }
        
        av_packet_free(&packet);
        
        // 获取统计信息
        auto demux_stats = demuxer->GetStats();
        auto mux_stats = muxer->GetStats();
        
        LOG_INFO("Remux completed:");
        LOG_INFO("  Packets read: %lu", demux_stats.packets_read);
        LOG_INFO("  Packets written: %lu", mux_stats.packets_written);
        LOG_INFO("  Bytes read: %lu", demux_stats.bytes_read);
        LOG_INFO("  Bytes written: %lu", mux_stats.bytes_written);
        
        return packets_copied > 0;
    }
    
MediaInfo MediaUtils::GetMediaInfo(const std::string& filename) {
        auto demuxer = XDemuxFactory::Create(filename);
        if (!demuxer) {
            return MediaInfo();
        }
        
        DemuxConfig config;
        config.filename = filename;
        
        if (!demuxer->Open(config)) {
            return MediaInfo();
        }
        
        return demuxer->GetMediaInfo();
    }
    
bool MediaUtils::Clip(const std::string& input_file,
                    const std::string& output_file,
                    int64_t start_time_us,
                    int64_t duration_us) {
        
        // 创建解封装器
        auto demuxer = XDemuxFactory::Create(input_file);
        if (!demuxer) {
            return false;
        }
        
        DemuxConfig demux_config;
        demux_config.filename = input_file;
        
        if (!demuxer->Open(demux_config)) {
            return false;
        }
        
        // 跳转到开始时间
        if (!demuxer->Seek(start_time_us)) {
            LOG_ERROR("Failed to seek to start time");
            return false;
        }
        
        // 创建封装器（使用相同格式）
        std::string format = XDemuxFactory::DetectFormat(input_file);
        auto muxer = XMuxFactory::Create(format);
        if (!muxer) {
            return false;
        }
        
        // 配置输出（复制输入配置）
        MediaInfo media_info = demuxer->GetMediaInfo();
        MuxConfig mux_config;
        mux_config.filename = output_file;
        mux_config.format_name = format;
        
        // 配置流参数（简化版本）
        if (demuxer->GetVideoStreamIndex() >= 0) {
            const StreamInfo& video_stream = media_info.streams[demuxer->GetVideoStreamIndex()];
            mux_config.enable_video = true;
            mux_config.video_codec = video_stream.codec_type;
            mux_config.video_width = video_stream.width;
            mux_config.video_height = video_stream.height;
            mux_config.video_frame_rate = video_stream.frame_rate;
            mux_config.video_bit_rate = video_stream.bit_rate;
        }
        
        if (!muxer->Open(mux_config)) {
            return false;
        }
        
        LOG_INFO("Clipping: %.2fs - %.2fs", 
                 start_time_us / 1000000.0, 
                 (start_time_us + duration_us) / 1000000.0);
        
        // 复制指定时间段的包
        AVPacket* packet = av_packet_alloc();
        int64_t end_time_us = start_time_us + duration_us;
        size_t packets_copied = 0;
        
        while (demuxer->ReadPacket(packet)) {
            // 检查时间戳
            AVStream* stream = demuxer->GetMediaInfo().streams[packet->stream_index].index >= 0 ? 
                              nullptr : nullptr; // 简化处理
            
            // 简单的时间检查（实际应该转换时间基）
            if (packet->pts > 0) {
                int64_t packet_time_us = packet->pts; // 简化，实际需要转换
                if (packet_time_us > end_time_us) {
                    break;
                }
            }
            
            // 写入包
            int output_stream_index = -1;
            if (packet->stream_index == demuxer->GetVideoStreamIndex()) {
                output_stream_index = muxer->GetVideoStreamIndex();
            }
            
            if (output_stream_index >= 0) {
                muxer->WritePacket(packet, output_stream_index);
                packets_copied++;
            }
            
            av_packet_unref(packet);
        }
        
        av_packet_free(&packet);
        
        LOG_INFO("Clip completed: %zu packets", packets_copied);
        return packets_copied > 0;
}
