#include <gtest/gtest.h>

#include <vector>

#include "vdf_computer.h"

// The initial form string is copied from chia-blockchain
char const* initial_form_str =
    "0100a27310baa44da5e1357dfd895f260dfa465d16f0f1fbf796e6f2a0df39c5dcbdca2192"
    "a6c2def98dc348d3730c416b0b5aaa8a99"
    "37177b15467694eb5cebac1c1574ae892501a265f62a85c99cfd5b7cb59d31567bfa2e931d"
    "c1de8e234da9050100";

// A challenge infuse to the calculation
vdf::types::Bytes challenge{0, 0, 1, 2, 3, 3, 4, 4};

// the number of iterations we want to run
uint64_t iters{10 * 1024};

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

TEST(VDF, VerifyWithGenesisAndNext)
{
  vdf::Computer::InitializeComputer();
  auto D = vdf::utils::CreateDiscriminant(challenge);
  vdf::Computer computer(D);

  computer.Run(iters);

  vdf::types::Proof proof = computer.GetProof();
  auto proof_data = vdf::utils::SerializeProof(proof);
  EXPECT_TRUE(vdf::utils::VerifyProof(D, proof_data, iters));
  EXPECT_FALSE(vdf::utils::VerifyProof(D, proof_data, iters * 2));

  vdf::Computer computer2(D, proof.y);

  computer2.Run(iters);

  vdf::types::Proof proof2 = computer2.GetProof();
  auto proof2_data = vdf::utils::SerializeProof(proof2);
  EXPECT_TRUE(vdf::utils::VerifyProof(D, proof2_data, iters,
                                      proof2.witness_type, proof.y));

  vdf::Computer computer3(D, proof2.y);

  computer3.Run(iters);

  auto proof3 = computer3.GetProof();
  auto proof3_data = vdf::utils::SerializeProof(proof3);
  EXPECT_TRUE(vdf::utils::VerifyProof(D, proof3_data, iters,
                                      proof3.witness_type, proof2.y));
}
