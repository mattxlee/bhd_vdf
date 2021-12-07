#ifndef VDF_COMPUTER_H
#define VDF_COMPUTER_H

#include <atomic>
#include <vector>

namespace vdf
{

struct ComputerMembers;

namespace utils
{

struct Proof {
    std::vector<uint8_t> y;
    std::vector<uint8_t> proof;
    uint8_t witness_type { 0 };
};

std::string ProofToHex(Proof const& proof);

Proof CreateProof(std::vector<uint8_t> y, std::vector<uint8_t> proof);

} // namespace utils

class Computer
{
    ComputerMembers* memImpl_ { nullptr };

public:
    Computer(std::vector<uint8_t> challenge, int discriminant_size_bits, std::vector<uint8_t> initial_form);

    ~Computer();

    void Run(uint64_t iters);

    void SetStop(bool stopped);
};

} // namespace vdf

#endif
