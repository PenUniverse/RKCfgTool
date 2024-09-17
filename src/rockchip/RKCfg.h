#pragma once

#include <vector>

#include <nlohmann/json.hpp>

#include "util/String.h"

#include "RKError.h"
#include "RKPreDefines.h"

using RKCfgItemContainer = std::vector<RKCfgItem>;
using RKParameter        = std::unordered_map<std::string, std::string>;

class RKCfgFile {
public:
    enum SaveMode { DefaultMode, JsonMode };

    class ItemFilter {
    public:
        virtual ~ItemFilter() = default;

        enum Type { Address, Name, ImagePath, Index };

        virtual Type getType() const = 0;

        virtual bool filt(size_t idx, const RKCfgItem& item) const = 0;
    };

    class AddressItemFilter : public ItemFilter {
    public:
        explicit AddressItemFilter(uint32_t address) : value(address) {}

        Type getType() const override { return Address; }

        bool filt(size_t idx, const RKCfgItem& item) const override { return item.address == value; }

    private:
        uint32_t value;
    };

    class NameItemFilter : public ItemFilter {
    public:
        explicit NameItemFilter(const std::string& name) : value(name) {}

        Type getType() const override { return Name; }

        bool filt(size_t idx, const RKCfgItem& item) const override { return Char16ToString(item.name) == value; }

    private:
        std::string value;
    };

    class ImagePathItemFilter : public ItemFilter {
    public:
        explicit ImagePathItemFilter(const std::string& path) : value(path) {}

        Type getType() const override { return ImagePath; }

        bool filt(size_t idx, const RKCfgItem& item) const override { return Char16ToString(item.image_path) == value; }

    private:
        std::string value;
    };

    class IndexItemFilter : public ItemFilter {
    public:
        explicit IndexItemFilter(size_t idx) : value(idx) {}

        Type getType() const override { return Index; }

        bool filt(size_t idx, const RKCfgItem& item) const override { return idx == value; }

    private:
        size_t value;
    };

    struct AutoScanArgument {
        bool        enabled;
        std::string prefix;
        AutoScanArgument() : enabled{}, prefix{} {}
    };

    using ItemFilterCollection = std::vector<std::unique_ptr<const ItemFilter>>;

    // TODO: Replace with: std::expected
    static std::optional<RKCfgFile> fromFile(const std::string& path, std::error_code& ec);

    // TODO: Replace with: std::expected
    static std::optional<RKCfgFile>
    fromParameter(const std::string& path, AutoScanArgument auto_scan_args, std::error_code& ec);

    // TODO: Replace with: std::expected
    static std::optional<RKCfgFile> fromJson(const std::string& path, std::error_code& ec);

    void save(const std::string& path, SaveMode mode, std::error_code& ec) const;

    nlohmann::json toJson() const;

    void addItem(const RKCfgItem& item, bool auto_increase_length = true);
    void addItem(const RKCfgItem& item, size_t index, bool auto_increase_length = true);

    RKCfgItem const& getItem(size_t index) const;

    void removeItem(size_t index);
    void removeItem(const ItemFilterCollection& filters);

    void updateItem(size_t index, const RKCfgItem& item);

    RKCfgHeader const&        getHeader() const;
    RKCfgItemContainer const& getItems() const;

    void printDebugString() const;

private:
    RKCfgFile() = default;

    RKCfgHeader        m_header{};
    RKCfgItemContainer m_items;
};
