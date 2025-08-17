#include "xdemux.h"
#include <iostream>

// MediaUtils类已在头文件中定义

int main(int argc, char* argv[]) {
    LOG_INFO("Starting remux tests");
    
    // 测试格式转换
    LOG_INFO("Testing format conversion...");
    
    std::vector<std::tuple<std::string, std::string, std::string>> test_cases = {
        {"input.mp4", "output.avi", "avi"},
        {"input.avi", "output.mkv", "matroska"},
        {"input.mkv", "output.mp4", "mp4"},
        {"input.mov", "output.webm", "webm"}
    };
    
    for (const auto& [input, output, format] : test_cases) {
        LOG_INFO("Testing: %s -> %s (format: %s)", 
                 input.c_str(), output.c_str(), format.c_str());
        
        // 由于测试文件不存在，这会失败，但可以测试接口
        bool result = MediaUtils::Remux(input, output, format);
        LOG_INFO("  Result: %s (expected failure - no input file)", 
                 result ? "SUCCESS" : "FAILED");
    }
    
    // 测试自动格式检测
    LOG_INFO("Testing automatic format detection...");
    
    std::vector<std::pair<std::string, std::string>> auto_cases = {
        {"video.mp4", "video_copy.avi"},
        {"movie.avi", "movie_copy.mkv"},
        {"clip.mkv", "clip_copy.mp4"}
    };
    
    for (const auto& [input, output] : auto_cases) {
        LOG_INFO("Testing: %s -> %s (auto format)", 
                 input.c_str(), output.c_str());
        
        bool result = MediaUtils::Remux(input, output);  // 不指定格式
        LOG_INFO("  Result: %s (expected failure - no input file)", 
                 result ? "SUCCESS" : "FAILED");
    }
    
    // 测试媒体信息提取
    LOG_INFO("Testing media info extraction...");
    
    std::vector<std::string> info_test_files = {
        "sample.mp4",
        "video.avi", 
        "movie.mkv",
        "nonexistent.mp4"
    };
    
    for (const auto& filename : info_test_files) {
        LOG_INFO("Getting info for: %s", filename.c_str());
        
        MediaInfo info = MediaUtils::GetMediaInfo(filename);
        if (info.is_valid) {
            LOG_INFO("  Format: %s", info.format_name.c_str());
            LOG_INFO("  Duration: %.2fs", info.duration_us / 1000000.0);
            LOG_INFO("  File size: %ld bytes", info.file_size);
            LOG_INFO("  Bit rate: %ld bps", info.bit_rate);
            LOG_INFO("  Streams: %zu", info.streams.size());
            
            for (const auto& stream : info.streams) {
                if (stream.type == AVMEDIA_TYPE_VIDEO) {
                    LOG_INFO("    Video: %dx%d, %.2ffps, %s",
                             stream.width, stream.height,
                             av_q2d(stream.frame_rate),
                             stream.codec_name.c_str());
                } else if (stream.type == AVMEDIA_TYPE_AUDIO) {
                    LOG_INFO("    Audio: %dHz, %dch, %s",
                             stream.sample_rate, stream.channels,
                             stream.codec_name.c_str());
                }
            }
        } else {
            LOG_INFO("  Failed to get media info (expected - file doesn't exist)");
        }
    }
    
    // 测试批量重封装
    LOG_INFO("Testing batch remux...");
    
    struct RemuxTask {
        std::string input;
        std::string output;
        std::string format;
    };
    
    std::vector<RemuxTask> batch_tasks = {
        {"video1.mp4", "converted/video1.avi", "avi"},
        {"video2.avi", "converted/video2.mkv", "matroska"},
        {"video3.mkv", "converted/video3.mp4", "mp4"},
        {"video4.mov", "converted/video4.webm", "webm"}
    };
    
    size_t successful_conversions = 0;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (const auto& task : batch_tasks) {
        LOG_INFO("Converting: %s -> %s", task.input.c_str(), task.output.c_str());
        
        if (MediaUtils::Remux(task.input, task.output, task.format)) {
            successful_conversions++;
            LOG_INFO("  SUCCESS");
        } else {
            LOG_INFO("  FAILED (expected - no input files)");
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    LOG_INFO("Batch remux completed:");
    LOG_INFO("  Total tasks: %zu", batch_tasks.size());
    LOG_INFO("  Successful: %zu", successful_conversions);
    LOG_INFO("  Failed: %zu", batch_tasks.size() - successful_conversions);
    LOG_INFO("  Total time: %ld ms", duration.count());
    
    // 测试性能基准
    LOG_INFO("Testing remux performance...");
    
    const std::string test_input = "benchmark.mp4";
    const int NUM_ITERATIONS = 5;
    
    std::vector<double> conversion_times;
    
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        std::string output = "benchmark_" + std::to_string(i) + ".avi";
        
        auto iter_start = std::chrono::high_resolution_clock::now();
        bool result = MediaUtils::Remux(test_input, output, "avi");
        auto iter_end = std::chrono::high_resolution_clock::now();
        
        auto iter_duration = std::chrono::duration_cast<std::chrono::milliseconds>(iter_end - iter_start);
        conversion_times.push_back(iter_duration.count());
        
        LOG_INFO("  Iteration %d: %ld ms (%s)", 
                 i + 1, iter_duration.count(),
                 result ? "SUCCESS" : "FAILED");
    }
    
    // 计算统计信息
    if (!conversion_times.empty()) {
        double avg_time = 0.0;
        for (double time : conversion_times) {
            avg_time += time;
        }
        avg_time /= conversion_times.size();
        
        double min_time = *std::min_element(conversion_times.begin(), conversion_times.end());
        double max_time = *std::max_element(conversion_times.begin(), conversion_times.end());
        
        LOG_INFO("Performance statistics:");
        LOG_INFO("  Average time: %.2f ms", avg_time);
        LOG_INFO("  Min time: %.2f ms", min_time);
        LOG_INFO("  Max time: %.2f ms", max_time);
    }
    
    LOG_INFO("All remux tests completed!");
    
    return 0;
}
