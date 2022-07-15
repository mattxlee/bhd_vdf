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

TEST(VDF, RestoreProofToForm) {
    Bytes result = vdf::utils::BytesFromStr(
        "003f360be667de706fe886f766fe20240de04fe2c2f91207f1bbdddf20c554ab8d168b2ce9664d75f4613375a0ab12bf8158983574c9f5"
        "cd61c6b8a905fd3fa6bbffc5401b4ccedbe093b560293263a226e46302e720726586251116bc689ef09dc70d99e0a090c4409f928e218e"
        "85032fdbee02fedd563073be555b75a70a2d6a430033bc7a4926e3504e87698a0ace0dee6364cced2e9142b4e4cbe55a6371aab41e501c"
        "eed21d79d3a0dbbd82ce913c5de40b13eb7c59b1b52b6ef270ee603bd5e7fffcc9f5fae6dbd5aeec394181af130c0fdd195b22be745449"
        "b7a584ac80fc75ed49acfdb4d650f5cd344f86377ebbbaef5b19a0af3ae08101d1697f5656a52193000000000071c6f40024c342868a0c"
        "2a201b1b26a5d52c5d2f92a106c19ff926deb3fba1e74a444ecee3f8f507c062b949a2eaadd442b049417f82e8811526fa83c6d099d753"
        "23e068ffeca9dcd163761000c65d21dede72787ac350f25bdd3d29db6e9cb0e22c8124c724db33660c88784e2871b62ecf816846db7b46"
        "9c71cad9a5dcfc5548ed2dd781006fa15b968facf4d79219646267eb187a670306d1ff1a59fc28ae00d36bb5a1cba659f48aa64a902271"
        "1a66105ef14401ff3948add265240aaad329ee76ba4c2300496746b86bcccacff5947c3fcb956cde2cffae10435960d7097f989aac742c"
        "f1047887f11584d20297958385e1715fe0f9b69141750c20d8134420eafec68fd10000000001555540006958aabfe4cc5d870e61fef82b"
        "cf1f2c3859e2bd8b1177e8a8872376b5cabace5dcb59b6fecada7e522d05f6f0e352939a6bfdf8c454fbe822cfa5ce97d17be0ffde44a4"
        "812cde9d04ec5c08dce6f9146586fdc8e081e05ec690b7effe24ea756f3d300f361203b61e1a39220c6eafa7852842674e317dcae5549c"
        "78c7144296ff004a6d0d2854c55e4c1de2f17dc4480b81652cfec37124ef41560a28c853482732434d1c006763b2e341528ae0bcc29fb7"
        "6f1a4dafd99ade4fd75ec9cc9ca3f3d7001bcb6eb71e43eb22169ab721637551a8ec93838eb0825e9ecba9175297a00b146e9fdd244c5b"
        "722f29d3c46ec38840ba18f1f06ddec3dea844867386c2e1ac95");

    vdf::types::Integer D(
        "-1316533242541386366531638614143316983055310902214964679273603266867151809660942505983218996212499722203876871"
        "48397451395672779897144571112116763666653213748473909547482437246405018707472153290116227072825447643324530509"
        "016778432769802300913461285128339119844239772697652504835780459732685000796733645621728639");

    EXPECT_TRUE(!vdf::utils::VerifyProof(D, result, 33554432, 2));
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
