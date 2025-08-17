#include "xdecode.h"

// XDecodeFactory实现
std::unique_ptr<XDecode> XDecodeFactory::Create(CodecType codec_type) {
    switch (codec_type) {
        case CodecType::H264:
            return std::make_unique<H264Decoder>();
        case CodecType::H265:
            return std::make_unique<H265Decoder>();
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

std::vector<CodecType> XDecodeFactory::GetSupportedCodecs() {
    std::vector<CodecType> supported;
    
    // 检查H.264
    if (avcodec_find_decoder(AV_CODEC_ID_H264)) {
        supported.push_back(CodecType::H264);
    }
    
    // 检查H.265
    if (avcodec_find_decoder(AV_CODEC_ID_HEVC)) {
        supported.push_back(CodecType::H265);
    }
    
    // 检查VP8
    if (avcodec_find_decoder(AV_CODEC_ID_VP8)) {
        supported.push_back(CodecType::VP8);
    }
    
    // 检查VP9
    if (avcodec_find_decoder(AV_CODEC_ID_VP9)) {
        supported.push_back(CodecType::VP9);
    }
    
    // 检查AV1
    if (avcodec_find_decoder(AV_CODEC_ID_AV1)) {
        supported.push_back(CodecType::AV1);
    }
    
    return supported;
}

bool XDecodeFactory::IsCodecSupported(CodecType codec_type) {
    auto supported = GetSupportedCodecs();
    return std::find(supported.begin(), supported.end(), codec_type) != supported.end();
}

std::string XDecodeFactory::GetCodecName(CodecType codec_type) {
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

// DecodeUtils实现
CodecType DecodeUtils::DetectCodecType(const uint8_t* data, size_t size) {
    if (!data || size < 4) {
        return CodecType::UNKNOWN;
    }
    
    // H.264 NAL单元检测
    if (size >= 4) {
        // 检查起始码 0x00000001 或 0x000001
        if ((data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x00 && data[3] == 0x01) ||
            (data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x01)) {
            
            int offset = (data[2] == 0x01) ? 3 : 4;
            if (size > offset) {
                uint8_t nal_type = data[offset] & 0x1F;
                // H.264 NAL类型
                if (nal_type >= 1 && nal_type <= 12) {
                    return CodecType::H264;
                }
            }
        }
    }
    
    // H.265 NAL单元检测
    if (size >= 6) {
        if ((data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x00 && data[3] == 0x01) ||
            (data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x01)) {
            
            int offset = (data[2] == 0x01) ? 3 : 4;
            if (size > offset + 1) {
                uint8_t nal_type = (data[offset] >> 1) & 0x3F;
                // H.265 NAL类型
                if (nal_type >= 0 && nal_type <= 40) {
                    return CodecType::H265;
                }
            }
        }
    }
    
    return CodecType::UNKNOWN;
}

bool DecodeUtils::ValidateConfig(const DecodeConfig& config) {
    // 检查编码器支持
    if (!XDecodeFactory::IsCodecSupported(config.codec_type)) {
        LOG_ERROR("Codec not supported: %s", XDecodeFactory::GetCodecName(config.codec_type).c_str());
        return false;
    }
    
    // 检查尺寸（如果指定）
    if (config.width > 0 && config.height > 0) {
        if (config.width % 2 != 0 || config.height % 2 != 0) {
            LOG_ERROR("Dimensions must be even: %dx%d", config.width, config.height);
            return false;
        }
        
        if (config.width > 8192 || config.height > 8192) {
            LOG_ERROR("Dimensions too large: %dx%d", config.width, config.height);
            return false;
        }
    }
    
    // 检查线程数
    if (config.thread_count < 0 || config.thread_count > 64) {
        LOG_ERROR("Invalid thread count: %d", config.thread_count);
        return false;
    }
    
    return true;
}

std::vector<std::string> DecodeUtils::GetHardwareDevices() {
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

bool DecodeUtils::IsHardwareDecodeAvailable(const std::string& device, CodecType codec_type) {
    // 检查设备类型是否存在
    if (av_hwdevice_find_type_by_name(device.c_str()) == AV_HWDEVICE_TYPE_NONE) {
        return false;
    }
    
    // 检查解码器是否支持该硬件加速
    std::string decoder_name;
    
    if (device == "cuda") {
        switch (codec_type) {
            case CodecType::H264: decoder_name = "h264_cuvid"; break;
            case CodecType::H265: decoder_name = "hevc_cuvid"; break;
            default: return false;
        }
    } else if (device == "vaapi") {
        switch (codec_type) {
            case CodecType::H264: decoder_name = "h264_vaapi"; break;
            case CodecType::H265: decoder_name = "hevc_vaapi"; break;
            default: return false;
        }
    } else if (device == "qsv") {
        switch (codec_type) {
            case CodecType::H264: decoder_name = "h264_qsv"; break;
            case CodecType::H265: decoder_name = "hevc_qsv"; break;
            default: return false;
        }
    } else {
        return false;
    }
    
    return avcodec_find_decoder_by_name(decoder_name.c_str()) != nullptr;
}

int DecodeUtils::GetRecommendedThreadCount() {
    // 获取CPU核心数
    int cpu_count = std::thread::hardware_concurrency();
    
    // 解码通常不需要太多线程，限制在合理范围内
    if (cpu_count <= 2) {
        return 1;
    } else if (cpu_count <= 4) {
        return 2;
    } else if (cpu_count <= 8) {
        return 4;
    } else {
        return 6;  // 最多6个线程
    }
}
