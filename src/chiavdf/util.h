#ifndef UTIL_H
#define UTIL_H

#include "vdf_new.h"

/* Platform-specific byte swap macros. */
#if defined(_WIN32)
#include <cstdlib>

#define bswap_16(x) _byteswap_ushort(x)
#define bswap_32(x) _byteswap_ulong(x)
#define bswap_64(x) _byteswap_uint64(x)
#elif defined(__APPLE__)

#include <libkern/OSByteOrder.h>

#define bswap_16(x) OSSwapInt16(x)
#define bswap_32(x) OSSwapInt32(x)
#define bswap_64(x) OSSwapInt64(x)
#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
# if defined(__OpenBSD__)
#  include <endian.h>
#  define bswap_16(x) swap16(x)
#  define bswap_32(x) swap32(x)
#  define bswap_64(x) swap64(x)
# else
#  include <sys/endian.h>
#  define bswap_16(x) bswap16(x)
#  define bswap_32(x) bswap32(x)
#  define bswap_64(x) bswap64(x)
# endif
#else
#include <byteswap.h>
#endif


struct Segment {
    uint64_t start;
    uint64_t length;
    form x;
    form y;
    form proof;
    bool is_empty;

    Segment() {
        is_empty = true;
    }

    Segment(uint64_t start, uint64_t length, form& x, form& y) {        
        this->start = start;
        this->length = length;
        this->x = x;
        this->y = y;
        is_empty = false;
    }

    bool IsWorseThan(Segment& other) {
        if (is_empty) {
            if (!other.is_empty)
                return true;
            return false;
        }
        if (length > other.length)
            return true;
        if (length < other.length)  
            return false;
        return start > other.start;
    }

    int GetSegmentBucket() {
        uint64_t c_length = length;
        length >>= 16;
        int index = 0;
        while (length > 1) {
            index++;
            if (length == 2 || length == 3) {
                std::cout << "Warning: Invalid segment length.\n";
            }
            length >>= 2;
        }
        length = c_length;
        return index;
    }
};

void Int64ToBytes(uint8_t *result, uint64_t input);

void Int32ToBytes(uint8_t *result, uint32_t input);

uint64_t BytesToInt64(const uint8_t *bytes);

template<typename T>
void VectorAppend(std::vector<T> &dst, const std::vector<T> &src)
{
    dst.insert(dst.end(), src.begin(), src.end());
}

template<typename T>
void VectorAppendArray(std::vector<T> &dst, const T *src, size_t len)
{
    dst.insert(dst.end(), src, src + len);
}

std::string BytesToStr(const std::vector<unsigned char> &in);

void ApproximateParameters(uint64_t T, uint32_t& L, uint32_t& K);

struct Proof {
    Proof() {

    }

    Proof(std::vector<unsigned char> y, std::vector<unsigned char> proof) {
        this->y = y;
        this->proof = proof;
    }

    string hex() {
        std::vector<unsigned char> bytes(y);
        bytes.insert(bytes.end(), proof.begin(), proof.end());
        return BytesToStr(bytes);
    }

    std::vector<unsigned char> y;
    std::vector<unsigned char> proof;
    uint8_t witness_type;
};

#endif // UTIL_H
