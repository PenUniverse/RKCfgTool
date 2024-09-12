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
    if (file_size != sizeof(m_header) + result.m_header.item_size * result.m_header.length) {
        ec = make_rkcfg_load_error(RKCfgLoadErrorCode::AbnormalFileSize);
        return {};
    }
    result.m_items.reserve(result.m_header.length + 1);
    for (size_t idx = 0; idx < result.m_header.length; idx++) {
        file.seekg(result.m_header.begin + idx * result.m_header.item_size, std::ios::beg);
        RKCfgItem item{};
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
    result.m_header = _makeHeader();
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
        result.m_items.emplace_back(std::move(item));
    }
    return result;
}

std::optional<RKCfgFile> RKCfgFile::fromJson(const std::string& path, std::error_code& ec) {
    // TODO
    return {};
}

void RKCfgFile::save(const std::string& path, std::error_code& ec) const {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        ec = make_rkcfg_save_error(RKCfgSaveErrorCode::UnableToOpenFile);
        return;
    }
    file.write(reinterpret_cast<const char*>(&m_header), sizeof(m_header));
    for (auto& item : m_items) {
        file.write(reinterpret_cast<const char*>(&item), sizeof(item));
    }
}

void RKCfgFile::save_to_json(const std::string& path, std::error_code& ec) const {
    nlohmann::json result;
    result["header"]["size"]      = m_header.begin;
    result["header"]["item_size"] = m_header.item_size;
    for (auto& item : m_items) {
        result["items"].emplace_back(nlohmann::json{
            {"selected",   item.is_selected               },
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

RKCfgHeader RKCfgFile::_makeHeader() {
    RKCfgHeader result;
    result.length    = 0;
    result.begin     = sizeof(RKCfgHeader);
    result.item_size = sizeof(RKCfgItem);
    return result;
}
