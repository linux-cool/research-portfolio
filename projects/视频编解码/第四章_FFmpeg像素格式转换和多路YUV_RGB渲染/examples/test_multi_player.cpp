#include "multi_player.h"
#include <iostream>

int main(int argc, char* argv[]) {
    LOG_INFO("Starting multi-player test");
    
    // 创建多路播放器
    auto multi_player = MultiPlayerFactory::CreateMultiPlayer(4);
    if (!multi_player) {
        LOG_ERROR("Failed to create multi-player");
        return -1;
    }
    
    LOG_INFO("Multi-player created successfully");
    
    // 测试添加播放器（使用虚拟文件名）
    std::vector<std::string> test_files = {
        "test_video1.mp4",
        "test_video2.mp4", 
        "test_video3.mp4",
        "test_video4.mp4"
    };
    
    for (size_t i = 0; i < test_files.size(); ++i) {
        MultiVideoPlayer::PlayerConfig config;
        config.player_id = static_cast<int>(i);
        config.filename = test_files[i];
        config.render_x = (i % 2) * 320;
        config.render_y = (i / 2) * 240;
        config.render_width = 320;
        config.render_height = 240;
        
        // 注意：这里会失败因为文件不存在，但可以测试接口
        int player_id = multi_player->AddPlayer(config);
        if (player_id >= 0) {
            LOG_INFO("Added player %d for %s", player_id, test_files[i].c_str());
        } else {
            LOG_WARN("Failed to add player for %s (expected - file doesn't exist)", test_files[i].c_str());
        }
    }
    
    // 测试布局创建
    LOG_INFO("Testing layout creation...");
    
    auto layout_player = MultiPlayerFactory::CreateWithLayout(
        "2x2", test_files, 1280, 720);
    
    if (layout_player) {
        LOG_INFO("Layout player created successfully");
    } else {
        LOG_WARN("Layout player creation failed (expected - files don't exist)");
    }
    
    // 测试统计信息
    auto stats = multi_player->GetPlayersStats();
    LOG_INFO("Player stats count: %zu", stats.size());
    
    for (const auto& stat : stats) {
        LOG_INFO("Player %d: state=%d, time=%ld us", 
                 stat.player_id, static_cast<int>(stat.state), stat.current_time_us);
    }
    
    LOG_INFO("Multi-player test completed");
    return 0;
}
