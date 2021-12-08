#ifndef VDF_COMPUTER_H
#define VDF_COMPUTER_H

#include <atomic>
#include <memory>
#include <vector>

#include <string>
#include <string_view>

struct integer;

namespace vdf
{

int const DEFAULT_DISC_SIZE = 1024;

namespace types
{

using Bytes = std::vector<uint8_t>;

class Integer
{
    std::shared_ptr<integer> val_;

public:
    explicit Integer(integer const&);

    explicit Integer(std::string_view str);

    integer const& Get_integer() const;

    std::string FormatString() const;
};

struct Proof {
    types::Bytes y;
    types::Bytes proof;
    uint8_t witness_type { 0 };
};

} // namespace types

namespace utils
{

types::Integer CreateDiscriminant(types::Bytes const& challenge, int disc_size = DEFAULT_DISC_SIZE);

bool VerifyProof(types::Integer const& D, types::Proof const& proof, uint64_t iters, uint8_t witness_type = 0);

types::Bytes BytesFromStr(std::string_view str);

types::Bytes GetDefaultForm();

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
    static void InitializeComputer();

    explicit Computer(types::Integer D);

    Computer(types::Integer D, types::Bytes initial_form);

    ~Computer();

    void Run(uint64_t iters);

    void SetStop(bool stopped);

    uint64_t GetIters() const { return iters_; }

    types::Proof const& GetProof() const { return proof_; }
};

} // namespace vdf

#endif
