#ifndef VDF_COMPUTER_H
#define VDF_COMPUTER_H

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "vdf_types.h"

struct integer;

namespace vdf {

int const DEFAULT_DISC_SIZE = 1024;

namespace types {

class Integer {
    std::shared_ptr<integer> val_;

public:
    explicit Integer(integer const&);

    explicit Integer(std::string const& str);

    integer const& Get_integer() const;

    std::string FormatString() const;
};

struct Proof {
    Bytes y;
    Bytes proof;
    uint8_t witness_type{0};
};

}  // namespace types

namespace utils {

types::Integer CreateDiscriminant(Bytes const& challenge, int disc_size = DEFAULT_DISC_SIZE);

Bytes ConnectBytes(Bytes const& lhs, Bytes const& rhs);

Bytes SerializeProof(types::Proof const& proof);

bool VerifyProof(
    types::Integer const& D, Bytes const& proof, uint64_t iters, uint8_t witness_type = 0, Bytes const& x = {0x08});

Bytes BytesFromStr(std::string const& str);

Bytes GetDefaultForm();

}  // namespace utils

int FORM_SIZE();

class Computer {
    // Members to initialize the object
    Bytes initial_form_;
    types::Integer D_;
    // Flags
    std::mutex m_;
    // Results
    uint64_t iters_{0};
    types::Proof proof_;

public:
    static void InitializeComputer();

    explicit Computer(types::Integer D);

    Computer(types::Integer D, Bytes initial_form);

    ~Computer();

    void Run(uint64_t iters, std::atomic_bool& stop);

    void SetStop(bool stopped);

    void Join();

    uint64_t GetIters() const { return iters_; }

    types::Proof const& GetProof() const { return proof_; }
};

}  // namespace vdf

#endif
