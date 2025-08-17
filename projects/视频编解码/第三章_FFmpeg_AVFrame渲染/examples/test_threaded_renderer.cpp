#include "threaded_renderer.h"
#include <iostream>
#include <cmath>

void generateTestFrame(AVFrame* frame, int frame_number) {
    if (!frame || !frame->data[0]) return;

    int width = frame->width;
    int height = frame->height;

    // 生成简单的测试图案
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int offset = (frame_number * 2) % width;
            int color_x = (x + offset) % width;

            uint8_t Y = static_cast<uint8_t>(128 + 127 * sin(color_x * 0.02));
            frame->data[0][y * frame->linesize[0] + x] = Y;

            if (x % 2 == 0 && y % 2 == 0) {
                int uv_x = x / 2;
                int uv_y = y / 2;
                frame->data[1][uv_y * frame->linesize[1] + uv_x] = 128;
                frame->data[2][uv_y * frame->linesize[2] + uv_x] = 128;
            }
        }
    }

    frame->pts = frame_number;
}

int main(int argc, char* argv[]) {
    LOG_INFO("Starting threaded renderer test");

    // 创建配置
    ThreadedRenderer::Config config;
    config.frame_queue_size = 5;
    config.target_fps = 25.0;
    config.enable_fps_control = true;

    // 创建组件
    auto frame_manager = AVFrameRendererFactory::CreateFrameManager(10);
    auto video_renderer = XVideoViewFactory::Create(XVideoViewFactory::RendererType::SDL);

    if (!video_renderer || !video_renderer->Init(640, 480, PixelFormat::YUV420P)) {
        LOG_ERROR("Failed to initialize video renderer");
        return -1;
    }

    auto renderer = std::make_unique<ThreadedRenderer>(config);
    if (!renderer->Init(std::move(video_renderer), frame_manager)) {
        LOG_ERROR("Failed to initialize threaded renderer");
        return -1;
    }

    if (!renderer->Start()) {
        LOG_ERROR("Failed to start renderer");
        return -1;
    }

    LOG_INFO("Renderer started, generating frames...");

    // 生成和提交帧
    const int NUM_FRAMES = 75; // 3秒 @ 25fps
    for (int i = 0; i < NUM_FRAMES; ++i) {
        AVFrame* frame = frame_manager->AllocFrame(640, 480, AV_PIX_FMT_YUV420P);
        if (frame) {
            generateTestFrame(frame, i);
            if (!renderer->SubmitFrame(frame, 100)) {
                LOG_WARN("Failed to submit frame %d", i);
                frame_manager->ReleaseFrame(frame);
            }
        }

        if (i % 25 == 0) {
            auto stats = renderer->GetStats();
            LOG_INFO("Frame %d: FPS=%.1f, Queue=%zu",
                     i, stats.current_fps, stats.queue_size);
        }

        Utils::SleepMs(40); // ~25fps
    }

    LOG_INFO("Waiting for rendering to complete...");
    Utils::SleepMs(2000);

    auto final_stats = renderer->GetStats();
    LOG_INFO("Final stats: FPS=%.2f, Total=%zu, Rendered=%zu",
             final_stats.current_fps, final_stats.total_frames,
             final_stats.rendered_frames);

    renderer->Stop();
    LOG_INFO("Test completed");

    return 0;
}


