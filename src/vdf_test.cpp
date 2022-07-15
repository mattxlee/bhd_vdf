#include <gtest/gtest.h>

#include <vector>
#include <iostream>

#include "vdf_computer.h"

// The initial form string is copied from chia-blockchain
char const* initial_form_str =
    "0100a27310baa44da5e1357dfd895f260dfa465d16f0f1fbf796e6f2a0df39c5dcbdca2192"
    "a6c2def98dc348d3730c416b0b5aaa8a99"
    "37177b15467694eb5cebac1c1574ae892501a265f62a85c99cfd5b7cb59d31567bfa2e931d"
    "c1de8e234da9050100";

// the number of iterations we want to run
uint64_t iters{10 * 1024};
char const hex_chars[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

TEST(VDF, BytesFromStr) {
    auto initial_form = vdf::utils::BytesFromStr(initial_form_str);
    EXPECT_EQ(initial_form.size(), 100);
    EXPECT_EQ(initial_form[0], 1);
    EXPECT_EQ(initial_form[1], 0);
    EXPECT_EQ(initial_form[2], 0xa2);
    EXPECT_EQ(initial_form[98], 1);
    EXPECT_EQ(initial_form[97], 5);
    EXPECT_EQ(initial_form[96], 0xa9);
}

static const char* SZ_VDF_CHALLENGE = "9159bc7838880dcf826ba5fd7f5b693f203c01e29070ffa4eb1a73b727e09d84";
static const uint64_t VDF_ITERS = 100000;
static const char* SZ_VDF_Y =
    "0000e16a31edf6070934cacf78d3c3139e6986b7cebd0a45b996720fc916a163803a9c73d43b05c0835e8f2e4c52b2e10ae5623f7c1d5b"
    "98db2fc13b140b4c6035080f1b12cfe429abcee3912844319f2f81858d7ed4f7a6a108cf14f4a71090a1130201";
static const char* SZ_VDF_PROOF =
    "0200f845362c6a25e82034591c60797922723c20f7fc4fd2f955c43baefaa96ec4b3014174ff36ce05070f119a875b7b2df2ced3fe4963"
    "bc34fe7091e61623da6408117c6c31aef6d9b40f1e5c3c6fa7295196733d3448139a85bb8e6e5e0bc6dd020d07";

TEST(VDF, VerifyVdfProofOnly) {
    // Vdf proof verification
    Bytes challenge = vdf::utils::BytesFromStr(SZ_VDF_CHALLENGE);
    auto D = vdf::utils::CreateDiscriminant(challenge);
    vdf::types::Proof proof;
    proof.y = vdf::utils::BytesFromStr(SZ_VDF_Y);
    proof.proof = vdf::utils::BytesFromStr(SZ_VDF_PROOF);
    auto proof_data = vdf::utils::SerializeProof(proof);

    EXPECT_EQ(memcmp(proof.y.data(), proof_data.data(), proof.y.size()), 0);
    EXPECT_EQ(memcmp(proof.proof.data(), proof_data.data() + proof.proof.size(), proof.proof.size()), 0);
    EXPECT_EQ(proof.y.size(), proof.proof.size());

    std::cout << "len of challenge: " << challenge.size() << std::endl;
    std::cout << "len of proof: " << proof_data.size() << std::endl;
    std::cout << "D=" << D.FormatString() << std::endl;

    EXPECT_TRUE(vdf::utils::VerifyProof(D, proof_data, VDF_ITERS));
}

#if defined(USE_VDF_COMPUTER)

namespace utils {

char Byte4bToHexChar(uint8_t hex) { return hex_chars[hex]; }

std::string ByteToHex(uint8_t byte) {
    std::string hex(2, '0');
    uint8_t hi = (byte & 0xf0) >> 4;
    uint8_t lo = byte & 0x0f;
    hex[0] = Byte4bToHexChar(hi);
    hex[1] = Byte4bToHexChar(lo);
    return hex;
}

std::string BytesToHex(const Bytes& bytes) {
    std::stringstream ss;
    for (uint8_t byte : bytes) {
        ss << ByteToHex(byte);
    }
    return ss.str();
}

}  // namespace utils

TEST(VDF, CalcVdfProofOne) {
    static const char* SZ_VDF_CHALLENGE = "9159bc7838880dcf826ba5fd7f5b693f203c01e29070ffa4eb1a73b727e09d84";
    static const uint64_t VDF_ITERS = 100000;

    Bytes challenge = vdf::utils::BytesFromStr(SZ_VDF_CHALLENGE);

    vdf::Computer::InitializeComputer();
    auto D = vdf::utils::CreateDiscriminant(challenge);
    vdf::Computer computer(D);

    std::atomic_bool stop_flag;
    computer.Run(VDF_ITERS, stop_flag);

    auto proof = computer.GetProof();

    std::cout << "y=" << utils::BytesToHex(proof.y) << std::endl;
    std::cout << "proof=" << utils::BytesToHex(proof.proof) << std::endl;
    std::cout << "challenge=" << utils::BytesToHex(challenge) << std::endl;
    std::cout << "iters=" << VDF_ITERS << std::endl;

    EXPECT_EQ(proof.y, vdf::utils::BytesFromStr(SZ_VDF_Y));
    EXPECT_EQ(proof.proof, vdf::utils::BytesFromStr(SZ_VDF_PROOF));
}

TEST(VDF, VerifyWithGenesisAndNext) {
    Bytes challenge{0, 0, 1, 2, 3, 3, 4, 4};

    vdf::Computer::InitializeComputer();
    auto D = vdf::utils::CreateDiscriminant(challenge);
    vdf::Computer computer(D);

    std::atomic_bool stop_flag;
    computer.Run(iters, stop_flag);

    vdf::types::Proof proof = computer.GetProof();
    auto proof_data = vdf::utils::SerializeProof(proof);
    EXPECT_TRUE(vdf::utils::VerifyProof(D, proof_data, iters));
    EXPECT_FALSE(vdf::utils::VerifyProof(D, proof_data, iters * 2));
    EXPECT_EQ(proof.y.size(), vdf::FORM_SIZE());

    vdf::Computer computer2(D, proof.y);

    computer2.Run(iters, stop_flag);

    vdf::types::Proof proof2 = computer2.GetProof();
    auto proof2_data = vdf::utils::SerializeProof(proof2);
    EXPECT_TRUE(vdf::utils::VerifyProof(D, proof2_data, iters, proof2.witness_type, proof.y));
    EXPECT_EQ(proof2.y.size(), vdf::FORM_SIZE());

    vdf::Computer computer3(D, proof2.y);

    computer3.Run(iters, stop_flag);

    auto proof3 = computer3.GetProof();
    auto proof3_data = vdf::utils::SerializeProof(proof3);
    EXPECT_TRUE(vdf::utils::VerifyProof(D, proof3_data, iters, proof3.witness_type, proof2.y));
}

#endif
