#ifndef VDF_COMPUTER_H
#define VDF_COMPUTER_H

#include <atomic>
#include <memory>
#include <vector>

namespace vdf
{

namespace types
{

using Bytes = std::vector<uint8_t>;

class Integer
{
    Bytes data_;

public:
    explicit Integer(Bytes data);

    Bytes ToBytes() const;
};

struct Proof {
    types::Bytes y;
    types::Bytes proof;
};

} // namespace types

namespace utils
{

types::Integer CreateDiscriminant(types::Bytes const& challenge, int disc_size);

bool VerifyProof(types::Integer const& D, types::Proof const& proof, uint64_t iters, int disc_size, uint64_t recursion);

} // namespace utils

class Computer
{
    // Members to initialize the object
    types::Bytes initial_form_;
    types::Integer D_;
    // Flags
    std::atomic<bool> stopped_ { false };
    // Results
    uint64_t iters_ { 0 };
    types::Proof proof_;

public:
    Computer(types::Integer D, types::Bytes initial_form);

    ~Computer();

    void Run(uint64_t iters);

    void SetStop(bool stopped);

    uint64_t GetIters() const { return iters_; }

    types::Proof const& GetProof() const { return proof_; }
};

} // namespace vdf

#endif
