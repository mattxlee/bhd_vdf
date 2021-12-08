#include <gtest/gtest.h>

#include <vector>

#include "vdf_computer.h"

char const* initial_form_str
    = "0100a27310baa44da5e1357dfd895f260dfa465d16f0f1fbf796e6f2a0df39c5dcbdca2192a6c2def98dc348d3730c416b0b5aaa8a99"
      "37177b15467694eb5cebac1c1574ae892501a265f62a85c99cfd5b7cb59d31567bfa2e931dc1de8e234da9050100";

TEST(VDF, BytesFromStr)
{
    auto initial_form = vdf::utils::BytesFromStr(initial_form_str);
    EXPECT_EQ(initial_form.size(), 100);
    EXPECT_EQ(initial_form[0], 1);
    EXPECT_EQ(initial_form[1], 0);
    EXPECT_EQ(initial_form[2], 0xa2);
    EXPECT_EQ(initial_form[98], 1);
    EXPECT_EQ(initial_form[97], 5);
    EXPECT_EQ(initial_form[96], 0xa9);
}

TEST(VDF, Verify)
{
    vdf::types::Bytes challenge { 0, 0, 1, 2, 3, 3, 4, 4 };
    uint64_t iters { 220 * 1024 };

    vdf::Computer::InitializeComputer();
    auto D = vdf::utils::CreateDiscriminant(challenge);
    vdf::Computer computer(D);

    computer.Run(iters);

    vdf::types::Proof proof = computer.GetProof();
    EXPECT_TRUE(vdf::utils::VerifyProof(D, proof, iters));
    EXPECT_FALSE(vdf::utils::VerifyProof(D, proof, iters * 2));
}
