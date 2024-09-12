#include <argparse/argparse.hpp>
#include <spdlog/spdlog.h>

#include "rockchip/RKCfg.h"

int main(int argc, char** argv) {

    // ---  Logger  ---

#ifdef DEBUG
    spdlog::set_level(spdlog::level::debug);
#endif
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");

    // --- Program ---

    // clang-format off
    
    argparse::ArgumentParser program("para2cfg", "0.0.1");

    program.add_argument("-i", "--input")
        .help("Import a file.")
        .required();
    
    program.add_argument("-o", "--output")
        .help("Set the output file path (if any).");
    
    program.add_argument("-s", "--show")
        .help("Print the partition table information contained in the cfg file.")
        .flag();

    program.add_argument("--enable-auto-scan")
        .help("When converting json or parameter.txt to cfg file, the image file in the current directory will be automatically scanned and written.")
        .flag();

    // clang-format on

    std::error_code ec;

    try {
        program.parse_args(argc, argv);
    } catch (const std::runtime_error& e) {
        spdlog::error(e.what());
        return -1;
    }

    auto input_file_path = program.get<std::string>("--input");

    spdlog::info("{:<12} {}", "Input file:", input_file_path);
    spdlog::info("Parsing...");

    if (program["--show"] == true) {
        if (input_file_path.ends_with(".json")) {
            auto file = RKCfgFile::fromJson(input_file_path, ec);
            if (ec) {
                spdlog::error(ec.message());
                return -1;
            }
            file->printDebugString();
        } else {
            RKCfgFile file;
            file.load(input_file_path, ec);
            if (ec) {
                spdlog::error(ec.message());
                return -1;
            }
            file.printDebugString();
        }
    }

    if (program.is_used("--output")) {
        auto                     output_file_path = program.get<std::string>("--output");
        std::optional<RKCfgFile> file;
        if (input_file_path.ends_with(".json")) {
            file = RKCfgFile::fromJson(input_file_path, ec);
        } else if (input_file_path.ends_with(".txt")) {
            file = RKCfgFile::fromParameter(input_file_path, ec);
        } else {
            file = std::make_optional<RKCfgFile>();
            file->load(input_file_path, ec);
        }
        if (ec) {
            spdlog::error(ec.message());
            return -1;
        }
        if (output_file_path.ends_with(".json")) {
            file->save_to_json(output_file_path, ec);
        } else {
            file->save(output_file_path, ec);
        }
        if (ec) {
            spdlog::error(ec.message());
            return -1;
        }
    }

    return 0;
}
