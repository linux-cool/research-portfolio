#include "xdemux.h"
#include <iostream>

// MediaUtils类已在头文件中定义

int main(int argc, char* argv[]) {
    LOG_INFO("Starting video clip tests");
    
    // 测试基本剪辑功能
    LOG_INFO("Testing basic video clipping...");
    
    struct ClipTest {
        std::string input_file;
        std::string output_file;
        double start_seconds;
        double duration_seconds;
        std::string description;
    };
    
    std::vector<ClipTest> clip_tests = {
        {"movie.mp4", "clip_intro.mp4", 0.0, 30.0, "First 30 seconds"},
        {"movie.mp4", "clip_middle.mp4", 60.0, 45.0, "45 seconds from 1 minute"},
        {"movie.mp4", "clip_ending.mp4", 300.0, 60.0, "Last minute (from 5 min)"},
        {"long_video.mkv", "highlight.mkv", 120.0, 15.0, "15-second highlight"},
        {"presentation.avi", "summary.avi", 0.0, 120.0, "2-minute summary"}
    };
    
    for (const auto& test : clip_tests) {
        LOG_INFO("Test: %s", test.description.c_str());
        LOG_INFO("  Input: %s", test.input_file.c_str());
        LOG_INFO("  Output: %s", test.output_file.c_str());
        LOG_INFO("  Time range: %.1fs - %.1fs (duration: %.1fs)",
                 test.start_seconds, 
                 test.start_seconds + test.duration_seconds,
                 test.duration_seconds);
        
        int64_t start_us = static_cast<int64_t>(test.start_seconds * 1000000);
        int64_t duration_us = static_cast<int64_t>(test.duration_seconds * 1000000);
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        bool result = MediaUtils::Clip(test.input_file, test.output_file, 
                                      start_us, duration_us);
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto clip_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        LOG_INFO("  Result: %s", result ? "SUCCESS" : "FAILED (expected - no input file)");
        LOG_INFO("  Processing time: %ld ms", clip_duration.count());
        LOG_INFO("");
    }
    
    // 测试批量剪辑
    LOG_INFO("Testing batch clipping...");
    
    const std::string source_video = "source_video.mp4";
    
    struct BatchClip {
        std::string output_name;
        double start;
        double duration;
    };
    
    std::vector<BatchClip> batch_clips = {
        {"scene1.mp4", 0.0, 45.0},
        {"scene2.mp4", 50.0, 60.0},
        {"scene3.mp4", 120.0, 30.0},
        {"scene4.mp4", 180.0, 75.0},
        {"scene5.mp4", 300.0, 40.0}
    };
    
    size_t successful_clips = 0;
    auto batch_start = std::chrono::high_resolution_clock::now();
    
    for (const auto& clip : batch_clips) {
        LOG_INFO("Creating clip: %s (%.1fs, duration: %.1fs)",
                 clip.output_name.c_str(), clip.start, clip.duration);
        
        int64_t start_us = static_cast<int64_t>(clip.start * 1000000);
        int64_t duration_us = static_cast<int64_t>(clip.duration * 1000000);
        
        if (MediaUtils::Clip(source_video, clip.output_name, start_us, duration_us)) {
            successful_clips++;
            LOG_INFO("  SUCCESS");
        } else {
            LOG_INFO("  FAILED (expected - no input file)");
        }
    }
    
    auto batch_end = std::chrono::high_resolution_clock::now();
    auto batch_duration = std::chrono::duration_cast<std::chrono::milliseconds>(batch_end - batch_start);
    
    LOG_INFO("Batch clipping completed:");
    LOG_INFO("  Total clips: %zu", batch_clips.size());
    LOG_INFO("  Successful: %zu", successful_clips);
    LOG_INFO("  Failed: %zu", batch_clips.size() - successful_clips);
    LOG_INFO("  Total time: %ld ms", batch_duration.count());
    LOG_INFO("  Average time per clip: %.2f ms", 
             batch_duration.count() / (double)batch_clips.size());
    
    // 测试精确剪辑
    LOG_INFO("Testing precise clipping...");
    
    struct PreciseClip {
        std::string description;
        double start;
        double duration;
        bool expect_keyframe;
    };
    
    std::vector<PreciseClip> precise_tests = {
        {"Keyframe start", 0.0, 10.0, true},
        {"Mid-GOP start", 2.5, 5.0, false},
        {"Frame-accurate", 1.234, 3.456, false},
        {"Very short clip", 10.0, 0.5, false},
        {"Single frame", 5.0, 0.04, false}  // ~1 frame at 25fps
    };
    
    for (const auto& test : precise_tests) {
        LOG_INFO("Precise test: %s", test.description.c_str());
        LOG_INFO("  Start: %.3fs, Duration: %.3fs", test.start, test.duration);
        
        std::string output = "precise_" + std::to_string(static_cast<int>(test.start * 1000)) + ".mp4";
        
        int64_t start_us = static_cast<int64_t>(test.start * 1000000);
        int64_t duration_us = static_cast<int64_t>(test.duration * 1000000);
        
        bool result = MediaUtils::Clip("precise_source.mp4", output, start_us, duration_us);
        LOG_INFO("  Result: %s", result ? "SUCCESS" : "FAILED (expected)");
        
        if (result) {
            // 验证输出文件信息
            MediaInfo output_info = MediaUtils::GetMediaInfo(output);
            if (output_info.is_valid) {
                double actual_duration = output_info.duration_us / 1000000.0;
                double expected_duration = test.duration;
                double duration_diff = std::abs(actual_duration - expected_duration);
                
                LOG_INFO("  Expected duration: %.3fs", expected_duration);
                LOG_INFO("  Actual duration: %.3fs", actual_duration);
                LOG_INFO("  Difference: %.3fs", duration_diff);
                LOG_INFO("  Accuracy: %s", (duration_diff < 0.1) ? "GOOD" : "POOR");
            }
        }
    }
    
    // 测试错误处理
    LOG_INFO("Testing error handling...");
    
    struct ErrorTest {
        std::string input;
        std::string output;
        double start;
        double duration;
        std::string description;
    };
    
    std::vector<ErrorTest> error_tests = {
        {"", "output.mp4", 0.0, 10.0, "Empty input filename"},
        {"input.mp4", "", 0.0, 10.0, "Empty output filename"},
        {"input.mp4", "output.mp4", -5.0, 10.0, "Negative start time"},
        {"input.mp4", "output.mp4", 0.0, -10.0, "Negative duration"},
        {"input.mp4", "output.mp4", 1000.0, 10.0, "Start time beyond file duration"},
        {"nonexistent.mp4", "output.mp4", 0.0, 10.0, "Non-existent input file"}
    };
    
    for (const auto& test : error_tests) {
        LOG_INFO("Error test: %s", test.description.c_str());
        
        int64_t start_us = static_cast<int64_t>(test.start * 1000000);
        int64_t duration_us = static_cast<int64_t>(test.duration * 1000000);
        
        bool result = MediaUtils::Clip(test.input, test.output, start_us, duration_us);
        LOG_INFO("  Result: %s (expected: FAILED)", result ? "SUCCESS" : "FAILED");
    }
    
    // 测试性能基准
    LOG_INFO("Testing clipping performance...");
    
    struct PerfTest {
        std::string file_size;
        std::string filename;
        double clip_duration;
    };
    
    std::vector<PerfTest> perf_tests = {
        {"Small (10MB)", "small_video.mp4", 30.0},
        {"Medium (100MB)", "medium_video.mp4", 60.0},
        {"Large (500MB)", "large_video.mp4", 120.0},
        {"Huge (2GB)", "huge_video.mp4", 300.0}
    };
    
    for (const auto& test : perf_tests) {
        LOG_INFO("Performance test: %s", test.file_size.c_str());
        
        auto perf_start = std::chrono::high_resolution_clock::now();
        
        bool result = MediaUtils::Clip(test.filename, "perf_output.mp4", 
                                      0, static_cast<int64_t>(test.clip_duration * 1000000));
        
        auto perf_end = std::chrono::high_resolution_clock::now();
        auto perf_duration = std::chrono::duration_cast<std::chrono::milliseconds>(perf_end - perf_start);
        
        LOG_INFO("  Processing time: %ld ms", perf_duration.count());
        LOG_INFO("  Clip duration: %.1fs", test.clip_duration);
        
        if (perf_duration.count() > 0) {
            double speed_ratio = (test.clip_duration * 1000.0) / perf_duration.count();
            LOG_INFO("  Speed ratio: %.2fx realtime", speed_ratio);
        }
        
        LOG_INFO("  Result: %s", result ? "SUCCESS" : "FAILED (expected)");
    }
    
    LOG_INFO("All video clip tests completed!");
    
    return 0;
}
