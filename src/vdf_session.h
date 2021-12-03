#ifndef VDF_SESSION_H
#define VDF_SESSION_H

#include <atomic>
#include <vector>

#include "bqfc.h"
#include "vdf.h"

namespace vdf
{

class Session
{
    std::vector<char> disc_;
    std::vector<uint8_t> initial_form_;

    std::atomic<bool> stopped_;
    uint64_t iters_ { 0 };
    Proof proof_;

    void RepeatedSquare(
        form f, const integer& D, const integer& L, WesolowskiCallback* weso, FastStorage* fast_storage);

    void CreateAndWriteProofOneWeso(uint64_t iters, integer& D, form f, OneWesolowskiCallback* weso);

public:
    Session(std::vector<char> disc, std::vector<uint8_t> initial_form);

    ~Session();

    void Run(uint64_t iters);

    void SetStop(bool stopped);

    std::tuple<uint64_t, Proof> GetResult() const;
};

} // namespace vdf

#endif
