#pragma once

#include <vector>

#include "RKError.h"
#include "RKPreDefines.h"

#include <nlohmann/json.hpp>

using RKCfgItemContainer = std::vector<RKCfgItem>;
using RKParameter        = std::unordered_map<std::string, std::string>;

class RKCfgFile {
public:
    enum SaveMode { DefaultMode, JsonMode };

    // TODO: Replace with: std::expected
    static std::optional<RKCfgFile> fromFile(const std::string& path, std::error_code& ec);

    // TODO: Replace with: std::expected
    static std::optional<RKCfgFile> fromParameter(const std::string& path, std::error_code& ec);

    // TODO: Replace with: std::expected
    static std::optional<RKCfgFile> fromJson(const std::string& path, std::error_code& ec);

    void save(const std::string& path, SaveMode mode, std::error_code& ec) const;

    nlohmann::json toJson() const;

    uint8_t getTableLength() const;

    void addItem(const RKCfgItem& item);
    void addItem(const RKCfgItem& item, size_t index);

    RKCfgItem const& getItem(size_t index) const;

    void removeItem(size_t index);

    void updateItem(size_t index, const RKCfgItem& item);

    RKCfgHeader const&        getHeader() const;
    RKCfgItemContainer const& getItems() const;

    void printDebugString() const;

private:
    RKCfgFile() = default;

    RKCfgHeader        m_header{};
    RKCfgItemContainer m_items;
};
