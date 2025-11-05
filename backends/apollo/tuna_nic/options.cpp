#include "options.h"

#include "frontends/common/parser_options.h"
#include "lib/exename.h"

namespace P4::Apollo {

using namespace P4::literals;

std::vector<const char *> *TunaNicOptions::process(int argc, char *const argv[]) {
    auto executablePath = P4::getExecutablePath(argv[0]);
    if (executablePath.empty()) {
        std::cerr << "Could not determine executable path" << std::endl;
        return nullptr;
    }
    this->exe_name = cstring(executablePath.stem().c_str());

    searchForIncludePath(
        p4includePath,
        {"backends/apollo/tuna_nic/p4include"_cs, "../backends/apollo/tuna_nic/p4include"_cs,
         "../../backends/apollo/tuna_nic/p4include"_cs, "p4include"_cs, "../p4include"_cs,
         "../../p4include"_cs},
        executablePath.c_str());

    auto remainingOptions = CompilerOptions::process(argc, argv);
    return remainingOptions;
}

}  // namespace P4::Apollo