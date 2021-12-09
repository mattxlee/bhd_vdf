#include <gtest/gtest.h>

#include <vector>

#include "vdf_computer.h"

// The initial form string is copied from chia-blockchain
char const* initial_form_str
    = "0100a27310baa44da5e1357dfd895f260dfa465d16f0f1fbf796e6f2a0df39c5dcbdca2192a6c2def98dc348d3730c416b0b5aaa8a99"
      "37177b15467694eb5cebac1c1574ae892501a265f62a85c99cfd5b7cb59d31567bfa2e931dc1de8e234da9050100";

// A challenge infuse to the calculation
vdf::types::Bytes challenge { 0, 0, 1, 2, 3, 3, 4, 4 };

// the number of iterations we want to run
uint64_t iters { 220 * 1024 };

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

    vdf::Computer computer2(D, proof.proof);

    computer2.Run(iters);

    vdf::types::Proof proof2 = computer2.GetProof();
    auto proof2_data = vdf::utils::SerializeProof(proof2);
    EXPECT_TRUE(vdf::utils::VerifyProof(D, proof2_data, iters, proof2.witness_type, proof.proof));
}

TEST(VDF, VerifyFromChiaBlockChain)
{
    /*
     * Reference https://github.com/Chia-Network/chia-blockchain/wiki/Block-Format
     *
     * -- Proof of Time
     *
     * challenge_hash: the hash of the challenge, used to generate a VDF group.
     * number_of_iterations: the number of iterations that the VDF has to go through.
     * output: the output of the VDF, a class group element represented as two signed 512 bit numbers (a and b).
     * witness_type: proof type of VDF.
     * witness: VDF proof in bytes.
     */
    // The following data are recorded by chia-blockchain(main) from height 1254189 (sp_vdf)
    char const* prev_challenge_hash = "734ca7a97ade434ed4fccdba174d8734f97cba92486f7888bf32f1115edc237c";
    char const* challenge_hash = "1d7879a105a6213dfa7de2fbfe0ad71e1b5de177ad4ae711d356dd0d35b223fe";
    uint64_t number_of_iterations = 103563264;
    char const* output
        = "0100f6eba13c56a74cd4a1dda376623caba345c9e068604ed7ffece825e7125219081b63d57b4c3f9be47876c6c2ccf795ea898fd512"
          "2aa8034f10921748af24d3081fc71c764a272ed89a1ecc7e7e0b368a5a939e2b8b334e696e18c2455f5cd50c0302";
    int witness_type = 2;
    char const* witness
        = "0200a343cad9951338c3f061c2a8b1aa76dccc6c485caed381c56d1a4adb1da494375ac9a40701a5095b0505aa4339cb65c0ff04f220"
          "34aa9cc1b09f6460c223f62ebd0af338a025bbd45631bdd5382086148586908e586eaade9dc941621422600f0100000000000006b5f8"
          "eaea34e88151e979efd52318ff890f82ed6baef2ac6d6a0ecb71b326d5659553b1020076aa4892f2f7737bd3d5a270da6d71a9c8bd32"
          "7bfb69ac12831899f7ee13eb349d07a1d660d93980fe9ed7cbbb0ffe9aea8e57d135255f05173f36f4122cb42fddc03e461189533383"
          "dec32faf948c3badcad2b53349b03652e2d949ab92182201000000000000142120ea1b05b6411dad5948a49f3f9e929c88d52be7b800"
          "ea3f8bf833885c16e3d309030300e86f8c343c95912aeaf81a6696d4c45c5fe8ae17745ff16de4f4f5cca860e78d297a2fdc5ebdce8e"
          "eac5c00e9f0f4bdee41f96fd9da7924877629e17c52e88036b30c4b627e5685fc471d3f6ef25d0811aac3265217fac46de919a5da2cf"
          "83060100";

    vdf::Computer::InitializeComputer();
    auto D = vdf::utils::CreateDiscriminant(vdf::utils::BytesFromStr(challenge_hash));

    // EXPECT_TRUE(vdf::utils::VerifyProof(D, vdf::utils::BytesFromStr(output), number_of_iterations, witness_type,
    //     vdf::utils::BytesFromStr(prev_challenge_hash)));
}
