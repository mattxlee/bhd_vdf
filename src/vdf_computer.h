#ifndef VDF_COMPUTER_H
#define VDF_COMPUTER_H

#include <atomic>
#include <memory>
#include <vector>

#include "include.h"

#include "integer_common.h"
#include "util.h"

namespace vdf
{

class Computer
{
    std::vector<uint8_t> challenge;
    int discriminant_size_bits;

    integer D;
    std::vector<uint8_t> initial_form;

    std::atomic<bool> stopped;
    uint64_t iters { 0 };
    Proof proof;

public:
    Computer(std::vector<uint8_t> challenge, int discriminant_size_bits, std::vector<uint8_t> initial_form);

    ~Computer();

    void Run(uint64_t iters);

    void SetStop(bool stopped);
};

} // namespace vdf

#endif
