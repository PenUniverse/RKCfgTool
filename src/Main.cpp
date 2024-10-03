#include <argparse/argparse.hpp>
#include <spdlog/spdlog.h>

#include "rockchip/RKCfg.h"

using namespace rockchip;

int main(int argc, char** argv) try {

    // ---  Logger  ---

#ifdef DEBUG
    spdlog::set_level(spdlog::level::debug);
#endif
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");

    // --- Program ---

    // clang-format off

    argparse::ArgumentParser program("rkcfgtool", "0.2.0");

    program.add_argument("-i", "--input")
        .help("Import a file.")
        .required();

    program.add_argument("-o", "--output")
        .help("Set the output file path (if any).");

    program.add_argument("-s", "--show")
        .help("Print the partition table information contained in the cfg file.")
        .flag();

    program.add_argument("--enable-auto-scan")
        .help("When converting  parameter.txt to cfg file, the image file in the current directory will be automatically scanned and applied.")
        .flag();
    
    program.add_argument("--set-auto-scan-prefix")
        .help("Add a prefix to the results of the automatic image_path scan, will add a slash at the end (if not already there). Example: './Output'")
        .default_value("");

    // program.add_argument("--add-partition")
    //     .help("Add a partition to the input file. Syntax: 'address:name:image_path', example: '0x0x0123a000:userdisk:'.")
    //     .append();

    program.add_argument("--remove-partition")
        .help("Remove all matching partitions from the input. Syntax: '(address|name|image_path|index):..., example: 'name:userdisk'.")
        .append();

    // clang-format on

    std::error_code ec;

    program.parse_args(argc, argv);

    auto input_file_path = program.get<std::string>("--input");

    spdlog::info("Loading... {}", input_file_path);

    std::optional<RKCfgFile> file;

    if (input_file_path.ends_with(".json")) {
        file = RKCfgFile::fromJson(input_file_path, ec);
    } else if (input_file_path.ends_with(".txt")) {
        RKCfgFile::AutoScanArgument auto_scan_args;
        auto_scan_args.enabled = program.get<bool>("--enable-auto-scan");
        auto_scan_args.prefix  = program.get<std::string>("--set-auto-scan-prefix");
        if (auto_scan_args.prefix.find("/") != std::string::npos) {
            if (!auto_scan_args.prefix.ends_with("/")) auto_scan_args.prefix += "/";
        } else if (auto_scan_args.prefix.find("\\") != std::string::npos) {
            if (!auto_scan_args.prefix.ends_with("\\")) auto_scan_args.prefix += "\\";
        }
        file = RKCfgFile::fromParameter(input_file_path, auto_scan_args, ec);
    } else {
        file = RKCfgFile::fromFile(input_file_path, ec);
    }

    if (ec) {
        spdlog::error(ec.message());
        return -1;
    }

    if (program.is_used("--remove-partition")) {
        auto should_removed_parts = program.get<std::vector<std::string>>("--remove-partition");
        for (const auto& part : should_removed_parts) {
            // "address=0x10000000,name=test,"
            std::unordered_map<std::string, std::string> kv_result;
            spdlog::debug("should_remove_part: {}", part);
            bool        is_in_quotation_mark{};
            size_t      idx{};
            std::string buffer;
            std::string left;
            std::string right;
            while (true) {
                if (idx > part.size() - 1) break;
                auto chr = part[idx];
                idx++;
                if (chr == '\'') {
                    is_in_quotation_mark = !is_in_quotation_mark;
                    continue;
                }
                if (is_in_quotation_mark) {
                    buffer += chr;
                    continue;
                }
                switch (chr) {
                case ':':
                    left = buffer;
                    buffer.clear();
                    break;
                case ',':
                    right = buffer;
                    buffer.clear();
                    kv_result[left] = right;
                    break;
                default:
                    buffer += chr;
                    break;
                }
            }
            right = buffer;
            buffer.clear();
            kv_result[left] = right;
            RKCfgFile::ItemFilterCollection filters;
            for (const auto& [key, value] : kv_result) {
                if (key == "address") {
                    auto address = util::string::to_uint32(value);
                    if (!address) {
                        spdlog::error("Syntax error: {} is not a number.", value);
                        return -1;
                    }
                    filters.emplace_back(new RKCfgFile::AddressItemFilter(*address));
                } else if (key == "name") {
                    filters.emplace_back(new RKCfgFile::NameItemFilter(value));
                } else if (key == "image_path") {
                    filters.emplace_back(new RKCfgFile::ImagePathItemFilter(value));
                } else if (key == "index") {
                    auto index = util::string::to_uint32(value);
                    if (!index) {
                        spdlog::error("Syntax error: {} is not a number.", value);
                        return -1;
                    }
                    filters.emplace_back(new RKCfgFile::IndexItemFilter(*index));
                } else {
                    spdlog::error("Syntax error: unknown filter({}).", key);
                    return -1;
                }
            }
            file->removeItem(filters);
        }
    }

    if (program["--show"] == true) {
        file->printDebugString();
    }

    if (program.is_used("--output")) {
        auto output_file_path = program.get<std::string>("--output");
        file->save(
            output_file_path,
            output_file_path.ends_with(".json") ? RKCfgFile::JsonMode : RKCfgFile::DefaultMode,
            ec
        );
        if (ec) {
            spdlog::error(ec.message());
            return -1;
        }
        spdlog::info("Results have been saved to {}", output_file_path);
    }

    return 0;
} catch (const std::runtime_error& e) {
    spdlog::error(e.what());
    return -1;
}
