#ifndef VDF_UTILS_H
#define VDF_UTILS_H

#include <string>

#include "vdf_types.h"

vdf::types::Bytes to_bytes(std::string const& str);

std::string address_to_string(void* p);

std::string to_string(vdf::types::Bytes const& bytes);

#endif
