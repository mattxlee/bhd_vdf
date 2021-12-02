#include "vdf_session.h"

namespace vdf
{

Session::Session(std::vector<char> disc, std::vector<uint8_t> initial_form)
    : disc_(std::move(disc))
    , initial_form_(std::move(initial_form))
{
    fesetround(FE_TOWARDZERO);
}

} // namespace vdf
