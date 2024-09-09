#pragma once

#include <cstddef>
#include <cstdint>

// From: RKDevTool v2.86
//     * load_cfg 0x00425980
//     * save_cfg 0x004254C0

#pragma pack(push, 1)

struct RKCfgHeader {
    char     magic[4] = "CFG";
    char     gap_0[18];
    uint8_t  length;
    uint32_t begin;
    uint16_t item_size;
};

static_assert(sizeof(RKCfgHeader) == 29);

static_assert(offsetof(RKCfgHeader, length) == 22);
static_assert(offsetof(RKCfgHeader, begin) == 23);
static_assert(offsetof(RKCfgHeader, item_size) == 27);

struct RKCfgItem {
    char     gap_0[2];
    char16_t name[40];
    char16_t image_path[260];
    uint32_t address;
    uint8_t  is_selected;
    char     gap_1[3];
};

static_assert(sizeof(RKCfgItem) == 610);

static_assert(offsetof(RKCfgItem, name) == 2);
static_assert(offsetof(RKCfgItem, image_path) == 82);
static_assert(offsetof(RKCfgItem, address) == 602);
static_assert(offsetof(RKCfgItem, is_selected) == 606);

#pragma pack(pop)
