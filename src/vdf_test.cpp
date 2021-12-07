#include <gtest/gtest.h>

#include "vdf_computer.h"

#include "create_discriminant.h"

std::tuple<std::vector<uint8_t>, std::vector<uint8_t>> ProveNWeso(std::vector<uint8_t> const& challenge,
    std::vector<uint8_t> const& x, uint64_t disc_size, uint64_t form_size, uint64_t iters, int witness, bool wrong_segm)
{
    auto iters_chunk = iters / (witness + 1);
    std::vector<uint8_t> partials;
}

TEST(VDF, Construct)
{
    std::vector<uint8_t> challenge(32, 0);
    int discriminant_size_bits = 1024;
    std::vector<uint8_t> initial_form(100, 0);
    vdf::Computer computer(challenge, discriminant_size_bits, initial_form);
    computer.Run(100);
}
