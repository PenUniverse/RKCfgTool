#include <argparse/argparse.hpp>
#include <spdlog/spdlog.h>

#include "rockchip/RKCfg.h"

#include "util/String.h"

int main(int argc, char** argv) {

    // ---  Logger  ---

#ifdef DEBUG
    spdlog::set_level(spdlog::level::debug);
#endif
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");

    // --- Program ---

    // clang-format off
    
    argparse::ArgumentParser program("para2cfg", "0.0.1");

    program.add_argument("--input")
        .help("Import a file.")
        .required();
    
    program.add_argument("--output")
        .help("Set the output file path (if any).");
    
    program.add_argument("--show")
        .help("Print the partition table information contained in the cfg file.");
    
    program.add_argument("--json")
        .help("Convert cfg file to json file.");
    
    program.add_argument("--cfg")
        .help("Convert json or parameter.txt to cfg file.");

    program.add_argument("--auto-scan")
        .help("When converting json or parameter.txt to cfg file, the image file in the current directory will be automatically scanned and written.");

    program.parse_args(argc, argv);

    // clang-format on

    auto inputFileName = program.get<std::string>("--input");

    spdlog::info("Input file: {}", inputFileName);

    auto Task_ShouldShowPartitions = program.is_used("--show");
    auto Task_ShouldConvertToJson  = program.is_used("--json");
    auto Task_ShouldConvertToCfg   = program.is_used("--cfg");

    if ((Task_ShouldShowPartitions || Task_ShouldConvertToJson) && Task_ShouldConvertToCfg) {
        spdlog::error("Unsupported operation.");
        return -1;
    }

    if (Task_ShouldShowPartitions || Task_ShouldConvertToJson) {
        RKCfgFile       file;
        std::error_code ec;
        file.load(inputFileName, ec);
        if (ec) {
            spdlog::error(ec.message());
            return -1;
        }
        if (Task_ShouldShowPartitions) {
            file.printDebugString();
        }
        if (Task_ShouldConvertToJson) {
            file.toJson();
        }
    }

    if (Task_ShouldConvertToCfg) {}

    return 0;
}
