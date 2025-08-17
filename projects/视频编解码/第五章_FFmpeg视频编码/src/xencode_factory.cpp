#include "xencode.h"

// XEncodeFactory实现
std::unique_ptr<XEncode> XEncodeFactory::Create(CodecType codec_type) {
    switch (codec_type) {
        case CodecType::H264:
            return std::make_unique<H264Encoder>();
        case CodecType::H265:
            return std::make_unique<H265Encoder>();
        case CodecType::VP8:
        case CodecType::VP9:
        case CodecType::AV1:
        case CodecType::UNKNOWN:
            LOG_WARN("Codec type %d not implemented yet", static_cast<int>(codec_type));
            return nullptr;
        default:
            LOG_ERROR("Unknown codec type: %d", static_cast<int>(codec_type));
            return nullptr;
    }
}

std::vector<CodecType> XEncodeFactory::GetSupportedCodecs() {
    std::vector<CodecType> supported;
    
    // 检查H.264
    if (avcodec_find_encoder_by_name("libx264") || avcodec_find_encoder(AV_CODEC_ID_H264)) {
        supported.push_back(CodecType::H264);
    }
    
    // 检查H.265
    if (avcodec_find_encoder_by_name("libx265") || avcodec_find_encoder(AV_CODEC_ID_HEVC)) {
        supported.push_back(CodecType::H265);
    }
    
    // 检查VP8
    if (avcodec_find_encoder_by_name("libvpx") || avcodec_find_encoder(AV_CODEC_ID_VP8)) {
        supported.push_back(CodecType::VP8);
    }
    
    // 检查VP9
    if (avcodec_find_encoder_by_name("libvpx-vp9") || avcodec_find_encoder(AV_CODEC_ID_VP9)) {
        supported.push_back(CodecType::VP9);
    }
    
    // 检查AV1
    if (avcodec_find_encoder_by_name("libaom-av1") || avcodec_find_encoder(AV_CODEC_ID_AV1)) {
        supported.push_back(CodecType::AV1);
    }
    
    // 其他编码器暂不支持
    
    return supported;
}

bool XEncodeFactory::IsCodecSupported(CodecType codec_type) {
    auto supported = GetSupportedCodecs();
    return std::find(supported.begin(), supported.end(), codec_type) != supported.end();
}

std::string XEncodeFactory::GetCodecName(CodecType codec_type) {
    switch (codec_type) {
        case CodecType::H264: return "H.264/AVC";
        case CodecType::H265: return "H.265/HEVC";
        case CodecType::VP8: return "VP8";
        case CodecType::VP9: return "VP9";
        case CodecType::AV1: return "AV1";
        case CodecType::UNKNOWN:
        default: return "Unknown";
    }
}

std::string XEncodeFactory::GetPresetName(QualityPreset preset) {
    switch (preset) {
        case QualityPreset::ULTRAFAST: return "ultrafast";
        case QualityPreset::SUPERFAST: return "superfast";
        case QualityPreset::VERYFAST: return "veryfast";
        case QualityPreset::FASTER: return "faster";
        case QualityPreset::FAST: return "fast";
        case QualityPreset::MEDIUM: return "medium";
        case QualityPreset::SLOW: return "slow";
        case QualityPreset::SLOWER: return "slower";
        case QualityPreset::VERYSLOW: return "veryslow";
        case QualityPreset::PLACEBO: return "placebo";
        default: return "medium";
    }
}

// EncodeUtils实现
int64_t EncodeUtils::CalculateRecommendedBitrate(int width, int height, 
                                                double fps, CodecType codec_type) {
    // 基础码率计算：像素数 * fps * 比特率因子
    int64_t pixels = static_cast<int64_t>(width) * height;
    double base_bitrate = pixels * fps;
    
    // 根据编码器类型调整
    double codec_factor = 1.0;
    switch (codec_type) {
        case CodecType::H264:
            codec_factor = 0.1;  // H.264基准
            break;
        case CodecType::H265:
            codec_factor = 0.05; // H.265效率更高
            break;
        case CodecType::VP8:
            codec_factor = 0.12;
            break;
        case CodecType::VP9:
            codec_factor = 0.06;
            break;
        case CodecType::AV1:
            codec_factor = 0.04; // AV1效率最高
            break;
        case CodecType::UNKNOWN:
        default:
            codec_factor = 0.1;  // 默认值
            break;
    }
    
    int64_t recommended = static_cast<int64_t>(base_bitrate * codec_factor);
    
    // 设置合理的范围
    int64_t min_bitrate = 100000;   // 100kbps
    int64_t max_bitrate = 50000000; // 50Mbps
    
    return std::max(min_bitrate, std::min(max_bitrate, recommended));
}

bool EncodeUtils::ValidateConfig(const EncodeConfig& config) {
    // 检查基本参数
    if (config.width <= 0 || config.height <= 0) {
        LOG_ERROR("Invalid dimensions: %dx%d", config.width, config.height);
        return false;
    }
    
    if (config.width % 2 != 0 || config.height % 2 != 0) {
        LOG_ERROR("Dimensions must be even: %dx%d", config.width, config.height);
        return false;
    }
    
    if (config.frame_rate.num <= 0 || config.frame_rate.den <= 0) {
        LOG_ERROR("Invalid frame rate: %d/%d", config.frame_rate.num, config.frame_rate.den);
        return false;
    }
    
    if (config.bit_rate <= 0) {
        LOG_ERROR("Invalid bit rate: %ld", config.bit_rate);
        return false;
    }
    
    if (config.gop_size < 0) {
        LOG_ERROR("Invalid GOP size: %d", config.gop_size);
        return false;
    }
    
    if (config.max_b_frames < 0) {
        LOG_ERROR("Invalid max B frames: %d", config.max_b_frames);
        return false;
    }
    
    if (config.crf >= 0 && (config.crf < 0 || config.crf > 51)) {
        LOG_ERROR("Invalid CRF value: %d (should be 0-51)", config.crf);
        return false;
    }
    
    if (config.qmin < 0 || config.qmax < 0 || config.qmin > config.qmax) {
        LOG_ERROR("Invalid quantization range: qmin=%d, qmax=%d", config.qmin, config.qmax);
        return false;
    }
    
    // 检查编码器支持
    if (!XEncodeFactory::IsCodecSupported(config.codec_type)) {
        LOG_ERROR("Codec not supported: %s", XEncodeFactory::GetCodecName(config.codec_type).c_str());
        return false;
    }
    
    return true;
}

std::vector<std::string> EncodeUtils::GetHardwareDevices() {
    std::vector<std::string> devices;
    
    // 检查CUDA
    if (av_hwdevice_find_type_by_name("cuda") != AV_HWDEVICE_TYPE_NONE) {
        devices.push_back("cuda");
    }
    
    // 检查VAAPI
    if (av_hwdevice_find_type_by_name("vaapi") != AV_HWDEVICE_TYPE_NONE) {
        devices.push_back("vaapi");
    }
    
    // 检查QSV
    if (av_hwdevice_find_type_by_name("qsv") != AV_HWDEVICE_TYPE_NONE) {
        devices.push_back("qsv");
    }
    
    // 检查VideoToolbox (macOS)
    if (av_hwdevice_find_type_by_name("videotoolbox") != AV_HWDEVICE_TYPE_NONE) {
        devices.push_back("videotoolbox");
    }
    
    // 检查D3D11VA (Windows)
    if (av_hwdevice_find_type_by_name("d3d11va") != AV_HWDEVICE_TYPE_NONE) {
        devices.push_back("d3d11va");
    }
    
    return devices;
}

bool EncodeUtils::IsHardwareAccelAvailable(const std::string& device, CodecType codec_type) {
    // 检查设备类型是否存在
    if (av_hwdevice_find_type_by_name(device.c_str()) == AV_HWDEVICE_TYPE_NONE) {
        return false;
    }
    
    // 检查编码器是否支持该硬件加速
    std::string encoder_name;
    
    if (device == "cuda") {
        switch (codec_type) {
            case CodecType::H264: encoder_name = "h264_nvenc"; break;
            case CodecType::H265: encoder_name = "hevc_nvenc"; break;
            default: return false;
        }
    } else if (device == "vaapi") {
        switch (codec_type) {
            case CodecType::H264: encoder_name = "h264_vaapi"; break;
            case CodecType::H265: encoder_name = "hevc_vaapi"; break;
            default: return false;
        }
    } else if (device == "qsv") {
        switch (codec_type) {
            case CodecType::H264: encoder_name = "h264_qsv"; break;
            case CodecType::H265: encoder_name = "hevc_qsv"; break;
            default: return false;
        }
    } else {
        return false;
    }
    
    return avcodec_find_encoder_by_name(encoder_name.c_str()) != nullptr;
}
