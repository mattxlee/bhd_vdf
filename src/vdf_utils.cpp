#include "vdf_utils.h"

#include <sstream>

using vdf::types::Bytes;

Bytes to_bytes(std::string const& str) {
    Bytes res(str.begin(), str.end());
    return res;
}

std::string address_to_string(void* p) {
    std::stringstream ss;
    ss << std::hex << "0x" << reinterpret_cast<uint64_t>(p);
    return ss.str();
}

std::string to_string(Bytes const& bytes) {
    std::string res(reinterpret_cast<char const*>(bytes.data()), bytes.size());
    return res;
}
