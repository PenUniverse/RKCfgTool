#include "RKCfg.h"

#include "util/String.h"

#include <filesystem>
#include <fstream>

#include <spdlog/spdlog.h>

std::optional<RKCfgFile> RKCfgFile::fromFile(const std::string& path, std::error_code& ec) {
    if (!std::filesystem::exists(path)) {
        ec = make_rkcfg_load_error(RKCfgLoadErrorCode::FileNotExists);
        return {};
    }
    auto file_size = std::filesystem::file_size(path);
    if (file_size < sizeof(RKCfgHeader)) {
        ec = make_rkcfg_load_error(RKCfgLoadErrorCode::IsNotRKCfgFile);
        return {};
    }
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        ec = make_rkcfg_load_error(RKCfgLoadErrorCode::UnableToOpenFile);
        return {};
    }
    RKCfgFile result;
    file.read(reinterpret_cast<char*>(&result.m_header), sizeof(m_header));
    if (strcmp(result.m_header.magic, "CFG") != 0) {
        ec = make_rkcfg_load_error(RKCfgLoadErrorCode::IsNotRKCfgFile);
        return {};
    }
    if (result.m_header.item_size != sizeof(RKCfgItem)) {
        ec = make_rkcfg_load_error(RKCfgLoadErrorCode::UnsupportedItemSize);
        return {};
    }
    auto legal_size = sizeof(m_header) + result.m_header.item_size * result.m_header.length;
    if (file_size != legal_size) {
        spdlog::debug("file_size = {:#x} (legal size = {:#x})", file_size, legal_size);
        ec = make_rkcfg_load_error(RKCfgLoadErrorCode::AbnormalFileSize);
        return {};
    }
    result.m_items.reserve(result.m_header.length + 1);
    for (size_t idx = 0; idx < result.m_header.length; idx++) {
        file.seekg(result.m_header.begin + idx * result.m_header.item_size, std::ios::beg);
        RKCfgItem item;
        file.read(reinterpret_cast<char*>(&item), sizeof(item));
        result.m_items.emplace_back(std::move(item));
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
    // "mtdparts=rk29xxnand:0x00002000@0x00004000(uboot),...,0x00400000@0x00E3a000(userdata),-@0x0123a000(userdisk:grow)"
    std::string mtdparts;
    while (std::getline(file, mtdparts)) {
        if (mtdparts.starts_with("mtdparts=")) break;
    }
    spdlog::debug("mtdparts: {}", mtdparts);
    if (mtdparts.empty()) {
        ec = make_rkcfg_convert_param_error(RKConvertParamErrorCode::MtdPartsNotFound);
        return {};
    }
    auto first_quotation_mark_pos = mtdparts.find(':');
    if (first_quotation_mark_pos == std::string::npos) {
        spdlog::debug("Illegal mark position. (0)");
        ec = make_rkcfg_convert_param_error(RKConvertParamErrorCode::IllegalMtdPartFormat);
        return {};
    }
    auto mtdparts_stream = std::stringstream(mtdparts.substr(first_quotation_mark_pos + 1));
    // "0x00002000@0x00004000(uboot)"
    // "-@0x0123a000(userdisk:grow)"
    std::string mtdpart;
    struct Partition {
        std::string name;
        uint32_t    address;
        // size is unused in rkcfg format.
        [[maybe_unused]] std::optional<uint32_t> size;
    };
    std::vector<Partition> parts;
    while (std::getline(mtdparts_stream, mtdpart, ',')) {
        spdlog::debug("mtdpart: {}", mtdpart);
        auto at_mark_pos             = mtdpart.find('@');
        auto left_quotation_mark_pos = mtdpart.find('(');
        if (at_mark_pos == std::string::npos || left_quotation_mark_pos == std::string::npos
            || at_mark_pos > left_quotation_mark_pos) {
            spdlog::debug("Illegal mark position. (1)");
            ec = make_rkcfg_convert_param_error(RKConvertParamErrorCode::IllegalMtdPartFormat);
            return {};
        }
        auto right_quotation_mark_pos = mtdpart.find(')', left_quotation_mark_pos);
        if (right_quotation_mark_pos == std::string::npos) {
            spdlog::debug("Illegal mark position. (2)");
            ec = make_rkcfg_convert_param_error(RKConvertParamErrorCode::IllegalMtdPartFormat);
            return {};
        }

        auto name = mtdpart.substr(left_quotation_mark_pos + 1, mtdpart.size() - left_quotation_mark_pos - 2);

        auto size_str    = mtdpart.substr(0, at_mark_pos);
        auto address_str = mtdpart.substr(at_mark_pos + 1, 10);

        std::optional<uint32_t> size;
        std::optional<uint32_t> address = StringToUInt32(address_str);

        if (!address) {
            spdlog::debug("Invalid address. (3)");
            ec = make_rkcfg_convert_param_error(RKConvertParamErrorCode::IllegalMtdPartFormat);
            return {};
        }

        if (size_str != "-") { // grow
            size = StringToUInt32(size_str);
            if (!size) {
                StringRemoveSuffix(name, ":grow"); // extend to maximum position.
                spdlog::debug("Invalid size. (4)");
                ec = make_rkcfg_convert_param_error(RKConvertParamErrorCode::IllegalMtdPartFormat);
                return {};
            }
        }

        parts.emplace_back(name, *address, size);
    }
    RKCfgFile result;
    // add rkcfg default parts
    RKCfgItem loader;
    StringToChar16("Loader", loader.name, RKCfgItem::RK_V286_MAX_NAME_SIZE);
    StringToChar16("MiniLoaderAll.bin", loader.image_path, RKCfgItem::RK_V286_MAX_PATH_SIZE);
    loader.address     = 0x00000000;
    loader.is_selected = true;
    result.m_items.emplace_back(loader);
    RKCfgItem parameter;
    StringToChar16("parameter", parameter.name, RKCfgItem::RK_V286_MAX_NAME_SIZE);
    StringToChar16(path, parameter.image_path, RKCfgItem::RK_V286_MAX_PATH_SIZE);
    parameter.address     = 0x00000000;
    parameter.is_selected = true;
    result.m_items.emplace_back(parameter);
    for (auto& part : parts) {
        RKCfgItem item;
        if (!StringToChar16(part.name, item.name, RKCfgItem::RK_V286_MAX_NAME_SIZE)) {
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
            StringToChar16(potential_image_path, item.image_path, RKCfgItem::RK_V286_MAX_PATH_SIZE);
        }
        item.address     = part.address;
        item.is_selected = true;
        result.m_items.emplace_back(std::move(item));
        result.m_header.length++;
    }
    return result;
}

std::optional<RKCfgFile> RKCfgFile::fromJson(const std::string& path, std::error_code& ec) {
    using json = nlohmann::json;

    if (!std::filesystem::exists(path)) {
        ec = make_rkcfg_load_error(RKCfgLoadErrorCode::FileNotExists);
        return {};
    }
    std::ifstream file(path);
    if (!file.is_open()) {
        ec = make_rkcfg_load_error(RKCfgLoadErrorCode::UnableToOpenFile);
        return {};
    }
    try {
        const auto data = json::parse(file);
        RKCfgFile  file;
        if (file.m_header.begin != data["header"]["size"]) {
            ec = make_rkcfg_load_error(RKCfgLoadErrorCode::UnsupportedHeaderSize);
            return {};
        }
        if (file.m_header.item_size != data["header"]["item_size"]) {
            ec = make_rkcfg_load_error(RKCfgLoadErrorCode::UnsupportedItemSize);
            return {};
        }
        file.m_header.length = data["items"].size();
        for (auto& item_data : data["items"]) {
            RKCfgItem item;
            StringToChar16(item_data["name"], item.name, RKCfgItem::RK_V286_MAX_NAME_SIZE);
            StringToChar16(item_data["image_path"], item.image_path, RKCfgItem::RK_V286_MAX_PATH_SIZE);
            item.address     = item_data["address"];
            item.is_selected = item_data["is_selected"];
            file.m_items.emplace_back(item);
        }
        return file;
    } catch (const json::exception& e) {
        ec = make_rkcfg_load_error(RKCfgLoadErrorCode::JsonParseError);
        return {};
    }
}

void RKCfgFile::save(const std::string& path, SaveMode mode, std::error_code& ec) const {
    if (mode == JsonMode) {
        std::ofstream file(path);
        if (!file.is_open()) {
            ec = make_rkcfg_save_error(RKCfgSaveErrorCode::UnableToOpenFile);
            return;
        }
        file << toJson().dump(4);
        file.close();
        return;
    }
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        ec = make_rkcfg_save_error(RKCfgSaveErrorCode::UnableToOpenFile);
        return;
    }
    file.write(reinterpret_cast<const char*>(&m_header), sizeof(m_header));
    for (auto& item : m_items) {
        file.write(reinterpret_cast<const char*>(&item), sizeof(item));
    }
}

nlohmann::json RKCfgFile::toJson() const {
    nlohmann::json result;
    result["header"]["size"]      = m_header.begin;
    result["header"]["item_size"] = m_header.item_size;
    for (auto& item : m_items) {
        result["items"].emplace_back(nlohmann::json{
            {"is_selected", (bool)item.is_selected         },
            {"address",     item.address                   },
            {"name",        Char16ToString(item.name)      },
            {"image_path",  Char16ToString(item.image_path)}
        });
    }
    return result;
}

uint8_t RKCfgFile::getTableLength() const { return m_header.length; }

void RKCfgFile::addItem(const RKCfgItem& item) { m_items.emplace_back(item); }

void RKCfgFile::addItem(const RKCfgItem& item, size_t index) { m_items.insert(m_items.begin() + index, item); }

void RKCfgFile::removeItem(size_t index) { m_items.erase(m_items.begin() + index); }

void RKCfgFile::updateItem(size_t index, const RKCfgItem& item) { m_items.at(index) = item; }

RKCfgHeader const& RKCfgFile::getHeader() const { return m_header; }

RKCfgItemContainer const& RKCfgFile::getItems() const { return m_items; }

void RKCfgFile::printDebugString() const {
    spdlog::info("{:<12} {:#x}", "Header size:", m_header.begin);
    spdlog::info("{:<12} {:#x}", "Item size:", m_header.item_size);
    spdlog::info("Partitions({}): ", m_header.length);
    spdlog::info("    {:<10} {:10} {}", "Address", "Name", "Path");
    for (auto& item : m_items) {
        spdlog::info(
            "[{}] {:#010x} {:<10} {}",
            item.is_selected ? "x" : " ",
            item.address,
            Char16ToString(item.name),
            Char16ToString(item.image_path)
        );
    }
}
