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

void RKCfgFile::save_to_json(const std::string& path, std::error_code& ec) const {
    nlohmann::json result;
    for (auto& item : mItems) {
        result["partitions"].emplace_back(nlohmann::json{
            {"address",    item.address                   },
            {"name",       Char16ToString(item.name)      },
            {"image_path", Char16ToString(item.image_path)}
        });
    }
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        ec = make_rkcfg_save_error(RKCfgSaveErrorCode::UnableToOpenFile);
        return;
    }
    file << result.dump(4);
    file.close();
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
    // "mtdparts=rk29xxnand:0x00002000@0x00004000(uboot),...,0x00400000@0x00E3a000(userdata),-@0x0123a000(userdisk:grow)"
    std::string mtdparts;
    while (std::getline(file, mtdparts)) {
        if (mtdparts.starts_with("mtdparts=")) break;
    }
    if (mtdparts.empty()) {
        ec = make_rkcfg_convert_param_error(RKConvertParamErrorCode::MtdPartsNotFound);
        return {};
    }
    auto first_quotation_mark_pos = mtdparts.find(':');
    if (first_quotation_mark_pos == std::string::npos) {
        ec = make_rkcfg_convert_param_error(RKConvertParamErrorCode::IllegalMtdPartFormat);
        return {};
    }
    auto mtdparts_stream = std::stringstream(mtdparts.substr(first_quotation_mark_pos));
    // "0x00002000@0x00004000(uboot)"
    // "-@0x0123a000(userdisk:grow)"
    std::string mtdpart;
    struct Partition {
        std::string             name;
        uint32_t                address;
        std::optional<uint32_t> size;
    };
    std::vector<Partition> parts;
    while (std::getline(mtdparts_stream, mtdpart, ',')) {
        auto at_mark_pos             = mtdpart.find('@');
        auto left_quotation_mark_pos = mtdpart.find('(');
        if (at_mark_pos == std::string::npos || left_quotation_mark_pos == std::string::npos
            || at_mark_pos > left_quotation_mark_pos) {
            ec = make_rkcfg_convert_param_error(RKConvertParamErrorCode::IllegalMtdPartFormat);
            return {};
        }
        auto right_quotation_mark_pos = mtdpart.find(')', left_quotation_mark_pos);
        if (right_quotation_mark_pos == std::string::npos) {
            ec = make_rkcfg_convert_param_error(RKConvertParamErrorCode::IllegalMtdPartFormat);
            return {};
        }

        auto name = mtdpart.substr(left_quotation_mark_pos + 1, mtdpart.size() - left_quotation_mark_pos - 2);

        auto size_str    = mtdpart.substr(0, at_mark_pos);
        auto address_str = mtdpart.substr(at_mark_pos + 1, 10);

        std::optional<uint32_t> size;
        std::optional<uint32_t> address = StringToUInt32(address_str);

        if (!address) {
            ec = make_rkcfg_convert_param_error(RKConvertParamErrorCode::IllegalMtdPartFormat);
            return {};
        }

        if (size_str == "-") { // grow
            size = StringToUInt32(size_str);
            if (!size) {
                ec = make_rkcfg_convert_param_error(RKConvertParamErrorCode::IllegalMtdPartFormat);
                return {};
            }
        }

        parts.emplace_back(name, *address, size);
    }
    RKCfgFile result;
    result.mHeader = _makeHeader();
    for (auto& part : parts) {
        constexpr size_t MAX_PATH_SIZE = sizeof(RKCfgItem::name) / sizeof(decltype(RKCfgItem::name[0]));

        RKCfgItem item;
        if (!StringToChar16(part.name, item.name, MAX_PATH_SIZE)) {
            ec = make_rkcfg_convert_param_error(RKConvertParamErrorCode::IllegalMtdPartFormat);
            return {};
        }
        // scan potential image file
        auto potential_image_name = part.name;
        auto potential_image_path = std::string();

        auto base_dir = std::filesystem::path(path).parent_path();
        auto scan     = [&]() {
            for (auto& entry : std::filesystem::directory_iterator(base_dir)) {
                auto this_path = entry.path();
                if (entry.is_regular_file() && this_path.string().ends_with(potential_image_name)) {
                    potential_image_path = this_path.filename();
                    break;
                }
            }
        };
        scan();
        if (potential_image_path.empty()) {
            StringRemoveSuffix(potential_image_name, "_a");
            StringRemoveSuffix(potential_image_name, "_b");
            scan();
        }
        if (!potential_image_path.empty()) {
            spdlog::info("Selected {} as the image file of {}.", potential_image_path, Char16ToString(item.name));
            StringToChar16(potential_image_path, item.image_path, MAX_PATH_SIZE);
        }
        item.address     = part.address;
        item.is_selected = true;
        result.mItems.emplace_back(std::move(item));
    }
    return result;
}

std::optional<RKCfgFile> RKCfgFile::fromJson(const std::string& path, std::error_code& ec) {
    // TODO
    return {};
}

void RKCfgFile::_cleanUp() {
    mHeader = {};
    mItems.clear();
}

RKCfgHeader RKCfgFile::_makeHeader() {
    RKCfgHeader result;
    result.length    = 0;
    result.begin     = sizeof(RKCfgHeader);
    result.item_size = sizeof(RKCfgItem);
    return result;
}
