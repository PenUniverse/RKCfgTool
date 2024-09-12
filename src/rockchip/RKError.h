#pragma once

#include <system_error>

// LoadError

enum class RKCfgLoadErrorCode {
    SUCCESS = 0,
    FileNotExists,
    UnableToOpenFile,
    UnsupportedItemSize,
    // Binary (Default)
    AbnormalFileSize,
    IsNotRKCfgFile,
    // Json
    JsonParseError,
    UnsupportedHeaderSize
};

class RKCfgLoadErrorCategory : public std::error_category {
public:
    const char* name() const noexcept override { return "RKCfgLoadError"; }
    std::string message(int ev) const override {
        switch (static_cast<RKCfgLoadErrorCode>(ev)) {
        case RKCfgLoadErrorCode::SUCCESS:
            return "Everything is ok.";
        case RKCfgLoadErrorCode::FileNotExists:
            return "The file does not exist.";
        case RKCfgLoadErrorCode::UnableToOpenFile:
            return "Unable to open file.";
        case RKCfgLoadErrorCode::UnsupportedItemSize:
            return "The file has an unsupported ItemSize. Please file an Issue on GitHub.";
        // Binary (Default)
        case RKCfgLoadErrorCode::AbnormalFileSize:
            return "The file size is abnormal, maybe it is corrupted?";
        case RKCfgLoadErrorCode::IsNotRKCfgFile:
            return "The target file is not an RKDevTool Config file.";
            // Json
        case RKCfgLoadErrorCode::JsonParseError:
            return "Json parsing failed, the format is incorrect!";
        case RKCfgLoadErrorCode::UnsupportedHeaderSize:
            return "The file has an unsupported HeaderSize, have you updated to the latest? ";
        default:
            return {};
        }
    }
};

inline const RKCfgLoadErrorCategory rkcfg_load_error_category{};

inline std::error_code make_rkcfg_load_error(RKCfgLoadErrorCode ec) {
    return {static_cast<int>(ec), rkcfg_load_error_category};
}

// SaveError

enum class RKCfgSaveErrorCode { SUCCESS = 0, UnableToOpenFile };

class RKCfgSaveErrorCategory : public std::error_category {
public:
    const char* name() const noexcept override { return "RKCfgSaveError"; }
    std::string message(int ev) const override {
        switch (static_cast<RKCfgSaveErrorCode>(ev)) {
        case RKCfgSaveErrorCode::SUCCESS:
            return "Everything is ok.";
        case RKCfgSaveErrorCode::UnableToOpenFile:
            return "Unable to open file.";
        default:
            return {};
        }
    }
};

inline const RKCfgSaveErrorCategory rkcfg_save_error_category{};

inline std::error_code make_rkcfg_save_error(RKCfgSaveErrorCode ec) {
    return {static_cast<int>(ec), rkcfg_save_error_category};
}

// ConvertParamError

enum class RKConvertParamErrorCode {
    SUCCESS = 0,
    FileNotExists,
    UnableToOpenFile,
    MtdPartsNotFound,
    IllegalMtdPartFormat
};

class RKConvertParamErrorCategory : public std::error_category {
public:
    const char* name() const noexcept override { return "RKConvertParamError"; }
    std::string message(int ev) const override {
        switch (static_cast<RKConvertParamErrorCode>(ev)) {
        case RKConvertParamErrorCode::SUCCESS:
            return "Everything is ok.";
        case RKConvertParamErrorCode::FileNotExists:
            return "The file does not exist.";
        case RKConvertParamErrorCode::UnableToOpenFile:
            return "Unable to open file.";
        case RKConvertParamErrorCode::MtdPartsNotFound:
            return "Unable to find mtdparts in parameter.";
        case RKConvertParamErrorCode::IllegalMtdPartFormat:
            return "Illegal mtdparts format.";
        default:
            return {};
        }
    }
};

inline const RKConvertParamErrorCategory rkcfg_convert_param_error_category{};

inline std::error_code make_rkcfg_convert_param_error(RKConvertParamErrorCode ec) {
    return {static_cast<int>(ec), rkcfg_convert_param_error_category};
}