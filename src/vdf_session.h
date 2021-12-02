#ifndef VDF_SESSION_H
#define VDF_SESSION_H

#include <vector>

#include "vdf.h"
#include "bqfc.h"

namespace vdf
{

class Session
{
    std::vector<char> disc_;
    std::vector<uint8_t> initial_form_;

public:
    Session(std::vector<char> disc, std::vector<uint8_t> initial_form);
};

} // namespace vdf

#endif
