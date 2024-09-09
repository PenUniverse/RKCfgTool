#include "RKCfg.h"

#include "util/String.h"

#include <filesystem>
#include <fstream>

#include <spdlog/spdlog.h>

void RKCfgFile::load(const std::string& path, std::error_code& ec) {
    _cleanUp();
    if (!std::filesystem::exists(path)) {
        ec = make_rkcfg_load_error(RKCfgLoadErrorCode::FileNotExists);
        return;
    }
    auto file_size = std::filesystem::file_size(path);
    if (file_size < sizeof(RKCfgHeader)) {
        ec = make_rkcfg_load_error(RKCfgLoadErrorCode::IsNotRKCfgFile);
        return;
    }
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        ec = make_rkcfg_load_error(RKCfgLoadErrorCode::UnableToOpenFile);
        return;
    }
    file.read(reinterpret_cast<char*>(&mHeader), sizeof(mHeader));
    if (strcmp(mHeader.magic, "CFG") != 0) {
        ec = make_rkcfg_load_error(RKCfgLoadErrorCode::IsNotRKCfgFile);
        return;
    }
    if (mHeader.item_size != sizeof(RKCfgItem)) {
        ec = make_rkcfg_load_error(RKCfgLoadErrorCode::UnsupportedItemSize);
        return;
    }
    if (file_size != sizeof(mHeader) + mHeader.item_size * mHeader.length) {
        ec = make_rkcfg_load_error(RKCfgLoadErrorCode::AbnormalFileSize);
        return;
    }
    mItems.reserve(mHeader.length + 1);
    for (size_t idx = 0; idx < mHeader.length; idx++) {
        file.seekg(mHeader.begin + idx * mHeader.item_size, std::ios::beg);
        RKCfgItem item{};
        file.read(reinterpret_cast<char*>(&item), sizeof(item));
        mItems.emplace_back(std::move(item));
    }
}

void RKCfgFile::save(const std::string& path, std::error_code& ec) const {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        ec = make_rkcfg_save_error(RKCfgSaveErrorCode::UnableToOpenFile);
        return;
    }
    file.write(reinterpret_cast<const char*>(&mHeader), sizeof(mHeader));
    for (auto& item : mItems) {
        file.write(reinterpret_cast<const char*>(&item), sizeof(item));
    }
}

uint8_t RKCfgFile::getTableLength() const { return mHeader.length; }

void RKCfgFile::addItem(const RKCfgItem& item) { mItems.emplace_back(item); }

void RKCfgFile::addItem(const RKCfgItem& item, size_t index) { mItems.insert(mItems.begin() + index, item); }

void RKCfgFile::removeItem(size_t index) { mItems.erase(mItems.begin() + index); }

void RKCfgFile::updateItem(size_t index, const RKCfgItem& item) { mItems.at(index) = item; }

RKCfgHeader const& RKCfgFile::getHeader() const { return mHeader; }

RKCfgItemContainer const& RKCfgFile::getItems() const { return mItems; }

void RKCfgFile::printDebugString() const {
    spdlog::info("Starting item address: {:#x}", mHeader.begin);
    spdlog::info("Item size:             {:#x}", mHeader.item_size);
    spdlog::info("Partitions({}): ", mHeader.length);
    for (auto& item : mItems) {
        spdlog::info(
            "\t[{}] {:#x} {} {}",
            item.is_selected ? "x" : " ",
            item.address,
            Char16ToString(item.name),
            Char16ToString(item.image_path)
        );
    }
}

nlohmann::json RKCfgFile::toJson() const {
    nlohmann::json result;
    for (auto& item : mItems) {
        result["partitions"].emplace_back(nlohmann::json{
            {"address",    item.address                   },
            {"name",       Char16ToString(item.name)      },
            {"image_path", Char16ToString(item.image_path)}
        });
    }
    return result;
}

std::optional<RKCfgFile> RKCfgFile::fromParameter(const std::string& path, std::error_code& ec) {
    if (!std::filesystem::exists(path)) {
        ec = make_rkcfg_convert_param_error(RKConvertParamErrorCode::FileNotExists);
        return {};
    }
    std::ifstream file(path);
    if (!file.is_open()) {
        ec = make_rkcfg_convert_param_error(RKConvertParamErrorCode::UnableToOpenFile);
        return {};
    }
}

void RKCfgFile::_cleanUp() {
    mHeader = {};
    mItems.clear();
}
