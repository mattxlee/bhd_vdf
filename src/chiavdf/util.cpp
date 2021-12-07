#include "util.h"

void Int64ToBytes(uint8_t *result, uint64_t input)
{
    uint64_t r = bswap_64(input);
    memcpy(result, &r, sizeof(r));
}

void Int32ToBytes(uint8_t *result, uint32_t input)
{
    uint32_t r = bswap_32(input);
    memcpy(result, &r, sizeof(r));
}

uint64_t BytesToInt64(const uint8_t *bytes)
{
    uint64_t i;
    memcpy(&i, bytes, sizeof(i));
    return bswap_64(i);
}

std::string BytesToStr(const std::vector<unsigned char> &in)
{
    std::vector<unsigned char>::const_iterator from = in.cbegin();
    std::vector<unsigned char>::const_iterator to = in.cend();
    std::ostringstream oss;
    for (; from != to; ++from)
       oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(*from);
    return oss.str();
}

void ApproximateParameters(uint64_t T, uint32_t& L, uint32_t& K) {
    double log_memory = 23.25349666;
    double log_T = log2(T);
    L = 1;
    if (log_T - log_memory > 0.000001) {
        L = ceil(pow(2, log_memory - 20));
    }
    double intermediate = T * (double)0.6931471 / (2.0 * L);
    K = std::max(std::round(log(intermediate) - log(log(intermediate)) + 0.25), 1.0);
}
