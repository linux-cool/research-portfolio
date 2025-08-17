#include "xdemux.h"
#include <iostream>
#include <iomanip>

// MediaUtils类已在头文件中定义

void printStreamInfo(const StreamInfo& stream) {
    LOG_INFO("  Stream %d (%s):", stream.index, 
             stream.type == AVMEDIA_TYPE_VIDEO ? "Video" :
             stream.type == AVMEDIA_TYPE_AUDIO ? "Audio" : "Other");
    
    LOG_INFO("    Codec: %s", stream.codec_name.c_str());
    LOG_INFO("    Duration: %.2fs", stream.duration * av_q2d(stream.time_base));
    LOG_INFO("    Bit rate: %ld bps", stream.bit_rate);
    
    if (stream.type == AVMEDIA_TYPE_VIDEO) {
        LOG_INFO("    Resolution: %dx%d", stream.width, stream.height);
        LOG_INFO("    Frame rate: %.2f fps", av_q2d(stream.frame_rate));
        LOG_INFO("    Time base: %d/%d", stream.time_base.num, stream.time_base.den);
    } else if (stream.type == AVMEDIA_TYPE_AUDIO) {
        LOG_INFO("    Sample rate: %d Hz", stream.sample_rate);
        LOG_INFO("    Channels: %d", stream.channels);
    }
}

void printMediaInfo(const MediaInfo& info) {
    if (!info.is_valid) {
        LOG_INFO("Invalid media info");
        return;
    }
    
    LOG_INFO("Media Information:");
    LOG_INFO("  File: %s", info.filename.c_str());
    LOG_INFO("  Format: %s", info.format_name.c_str());
    LOG_INFO("  Duration: %.2fs", info.duration_us / 1000000.0);
    LOG_INFO("  File size: %ld bytes (%.2f MB)", 
             info.file_size, info.file_size / (1024.0 * 1024.0));
    LOG_INFO("  Bit rate: %ld bps (%.2f Mbps)", 
             info.bit_rate, info.bit_rate / 1000000.0);
    LOG_INFO("  Streams: %zu", info.streams.size());
    
    // 打印流信息
    for (const auto& stream : info.streams) {
        printStreamInfo(stream);
    }
    
    // 打印元数据
    if (!info.metadata.empty()) {
        LOG_INFO("  Metadata:");
        for (const auto& [key, value] : info.metadata) {
            LOG_INFO("    %s: %s", key.c_str(), value.c_str());
        }
    }
}

void testMediaInfoExtraction() {
    LOG_INFO("Testing media info extraction...");
    
    std::vector<std::string> test_files = {
        "sample_video.mp4",
        "test_movie.avi",
        "clip.mkv",
        "audio.mp3",
        "presentation.mov",
        "stream.ts",
        "nonexistent.mp4"
    };
    
    for (const auto& filename : test_files) {
        LOG_INFO("Analyzing: %s", filename.c_str());
        
        MediaInfo info = MediaUtils::GetMediaInfo(filename);
        printMediaInfo(info);
        
        LOG_INFO(""); // 空行分隔
    }
}

void testStreamAnalysis() {
    LOG_INFO("Testing stream analysis...");
    
    // 模拟不同类型的媒体文件分析
    struct TestCase {
        std::string filename;
        std::string description;
    };
    
    std::vector<TestCase> test_cases = {
        {"video_only.mp4", "Video-only file"},
        {"audio_only.mp3", "Audio-only file"},
        {"movie_with_subtitles.mkv", "Video with multiple audio tracks and subtitles"},
        {"4k_video.mp4", "4K video file"},
        {"hdr_content.mkv", "HDR video content"},
        {"multichannel_audio.flac", "Multi-channel audio"},
        {"live_stream.ts", "Live stream recording"}
    };
    
    for (const auto& test_case : test_cases) {
        LOG_INFO("Test case: %s", test_case.description.c_str());
        LOG_INFO("File: %s", test_case.filename.c_str());
        
        MediaInfo info = MediaUtils::GetMediaInfo(test_case.filename);
        
        if (info.is_valid) {
            // 分析视频流
            int video_streams = 0;
            int audio_streams = 0;
            int subtitle_streams = 0;
            
            for (const auto& stream : info.streams) {
                switch (stream.type) {
                    case AVMEDIA_TYPE_VIDEO:
                        video_streams++;
                        break;
                    case AVMEDIA_TYPE_AUDIO:
                        audio_streams++;
                        break;
                    case AVMEDIA_TYPE_SUBTITLE:
                        subtitle_streams++;
                        break;
                    default:
                        break;
                }
            }
            
            LOG_INFO("  Stream summary: %d video, %d audio, %d subtitle",
                     video_streams, audio_streams, subtitle_streams);
            
            // 计算总码率
            int64_t total_bitrate = 0;
            for (const auto& stream : info.streams) {
                total_bitrate += stream.bit_rate;
            }
            
            LOG_INFO("  Calculated total bitrate: %ld bps", total_bitrate);
            LOG_INFO("  Container bitrate: %ld bps", info.bit_rate);
            
        } else {
            LOG_INFO("  Failed to analyze (expected - file doesn't exist)");
        }
        
        LOG_INFO(""); // 空行
    }
}

void testFormatCompatibility() {
    LOG_INFO("Testing format compatibility...");
    
    struct FormatTest {
        std::string extension;
        std::string expected_format;
        std::string description;
    };
    
    std::vector<FormatTest> format_tests = {
        {".mp4", "mp4", "MPEG-4 container"},
        {".avi", "avi", "Audio Video Interleave"},
        {".mkv", "matroska", "Matroska container"},
        {".mov", "mov", "QuickTime container"},
        {".webm", "webm", "WebM container"},
        {".flv", "flv", "Flash Video"},
        {".ts", "mpegts", "MPEG Transport Stream"},
        {".m4v", "mp4", "iTunes Video"},
        {".3gp", "3gp", "3GPP container"}
    };
    
    for (const auto& test : format_tests) {
        std::string test_file = "sample" + test.extension;
        LOG_INFO("Testing %s (%s)", test_file.c_str(), test.description.c_str());
        
        // 检测格式
        std::string detected = XDemuxFactory::DetectFormat(test_file);
        bool correct = (detected == test.expected_format);
        
        LOG_INFO("  Expected: %s, Detected: %s (%s)",
                 test.expected_format.c_str(), detected.c_str(),
                 correct ? "PASS" : "FAIL");
        
        // 尝试获取媒体信息
        MediaInfo info = MediaUtils::GetMediaInfo(test_file);
        if (info.is_valid) {
            LOG_INFO("  Successfully opened with format: %s", info.format_name.c_str());
        } else {
            LOG_INFO("  Failed to open (expected - file doesn't exist)");
        }
    }
}

void testPerformanceBenchmark() {
    LOG_INFO("Testing performance benchmark...");
    
    std::vector<std::string> benchmark_files = {
        "small_video.mp4",    // < 10MB
        "medium_video.mp4",   // 10-100MB  
        "large_video.mp4",    // 100MB-1GB
        "huge_video.mp4"      // > 1GB
    };
    
    for (const auto& filename : benchmark_files) {
        LOG_INFO("Benchmarking: %s", filename.c_str());
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        MediaInfo info = MediaUtils::GetMediaInfo(filename);
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (info.is_valid) {
            LOG_INFO("  Analysis time: %ld ms", duration.count());
            LOG_INFO("  File size: %.2f MB", info.file_size / (1024.0 * 1024.0));
            LOG_INFO("  Streams: %zu", info.streams.size());
            LOG_INFO("  Performance: %.2f MB/s", 
                     (info.file_size / (1024.0 * 1024.0)) / (duration.count() / 1000.0));
        } else {
            LOG_INFO("  Analysis time: %ld ms (failed)", duration.count());
        }
    }
}

int main(int argc, char* argv[]) {
    LOG_INFO("Starting media info tests");
    
    try {
        testMediaInfoExtraction();
        testStreamAnalysis();
        testFormatCompatibility();
        testPerformanceBenchmark();
        
        LOG_INFO("All media info tests completed!");
        
    } catch (const std::exception& e) {
        LOG_ERROR("Test failed with exception: %s", e.what());
        return -1;
    }
    
    return 0;
}
