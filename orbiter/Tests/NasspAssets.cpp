extern "C" {
#include "agc_engine.h"
}
#include "BasicExcelVC6.hpp"
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>

static void require(bool ok, const char *message)
{
    if (!ok) throw std::runtime_error(message);
}

int main(int argc, char **argv)
{
    try {
        require(argc == 3, "ROM and checklist paths required");
        auto agc = std::make_unique<agc_t>();
        require(agc_load_binfile(agc.get(), argv[1]) == 0, "AGC ROM load succeeds");
        std::ifstream rom(argv[1], std::ios::binary);
        for (int block = 0; block < 36; ++block) {
            const int bank = block < 4 ? (block ^ 2) : block;
            for (int word = 0; word < 02000; ++word) {
                unsigned char bytes[2];
                require(bool(rom.read(reinterpret_cast<char *>(bytes), 2)), "ROM contains all banks");
                const unsigned value = (unsigned(bytes[0]) << 8) | bytes[1];
                require(agc->Fixed[bank][word] == (value >> 1), "ROM word maps to the correct bank");
            }
        }
        require(agc_load_binfile(agc.get(), "MissingNativeAGC.bin") != 0,
                "missing AGC image reports failure");
        require(YExcel::GetDoubleFromRKValue(static_cast<std::int32_t>(0xbff80000U)) == -1.5,
                "negative packed Excel double decodes correctly");
        require(YExcel::GetDoubleFromRKValue(0x40590001) == 1.0,
                "scaled packed Excel double decodes correctly");
        YExcel::BasicExcel workbook;
        require(workbook.Load(argv[2]), "real Apollo 11 checklist loads");
        auto *groups = workbook.GetWorksheet("GROUPS");
        require(groups && groups->GetTotalRows() > 1, "checklist contains named groups");
        std::cout << "AGC ROM banks and Apollo 11 checklist verified\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
