#ifndef VDF_COMPUTER_H
#define VDF_COMPUTER_H

#include <atomic>
#include <vector>

#include "bqfc.h"
#include "vdf.h"

namespace vdf
{

class Computer
{
    std::vector<uint8_t> challenge_;
    int discriminant_size_bits_;

    integer D_;
    std::vector<uint8_t> initial_form_;

    std::atomic<bool> stopped_;
    uint64_t iters_ { 0 };
    Proof proof_;

    void RepeatedSquare(
        form f, const integer& D, const integer& L, WesolowskiCallback* weso, FastStorage* fast_storage);

    void CreateAndWriteProofOneWeso(uint64_t iters, integer& D, form f, OneWesolowskiCallback* weso);

public:
    Computer(std::vector<uint8_t> challenge, int discriminant_size_bits, std::vector<uint8_t> initial_form);

    ~Computer();

    void Run(uint64_t iters);

    void SetStop(bool stopped);

    std::tuple<uint64_t, Proof> GetResult() const;
};

} // namespace vdf

#endif
