#ifndef VDF_COMPUTER_H
#define VDF_COMPUTER_H

#include <atomic>
#include <vector>

namespace vdf
{

struct ComputerMembers;

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
