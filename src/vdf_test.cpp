#include <gtest/gtest.h>

#include "vdf_computer.h"

TEST(VDF, Construct)
{
    std::vector<uint8_t> challenge(32, 0);
    int discriminant_size_bits = 1024;
    std::vector<uint8_t> initial_form(100, 0);
    vdf::Computer computer(challenge, discriminant_size_bits, initial_form);
    computer.Run(100);
}
