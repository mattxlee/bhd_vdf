#ifndef PROOF_COMMON_H
#define PROOF_COMMON_H

#include <vector>

#include "Reducer.h"
extern "C" {
#include "bqfc.h"
}

#include "include.h"
#include "integer_common.h"
#include "nucomp.h"
#include "vdf_new.h"

#include "picosha2.h"

const int B_bits = 264;
const int B_bytes = (B_bits + 7) / 8;


// Generates a random psuedoprime using the hash and check method:
// Randomly chooses x with bit-length `length`, then applies a mask
//   (for b in bitmask) { x |= (1 << b) }.
// Then return x if it is a psuedoprime, otherwise repeat.
integer HashPrime(std::vector<uint8_t> seed, int length, vector<int> bitmask);

std::vector<unsigned char> SerializeForm(form &y, int d_bits);

form DeserializeForm(const integer &D, const uint8_t *bytes, size_t size);

integer FastPow(uint64_t a, uint64_t b, integer& c);

integer GetB(const integer& D, form &x, form& y);

class PulmarkReducer {
    ClassGroupContext *t;
    Reducer *reducer;

  public:
    PulmarkReducer() {
        t=new ClassGroupContext(4096);
        reducer=new Reducer(*t);
    }

    ~PulmarkReducer() {
        delete(reducer);
        delete(t);
    }

    void reduce(form &f) {
        mpz_set(t->a, f.a.impl);
        mpz_set(t->b, f.b.impl);
        mpz_set(t->c, f.c.impl);

        reducer->run();

        mpz_set(f.a.impl, t->a);
        mpz_set(f.b.impl, t->b);
        mpz_set(f.c.impl, t->c);
    }
};

form FastPowFormNucomp(form x, integer &D, integer num_iterations, integer &L, PulmarkReducer& reducer);

# endif // PROOF_COMMON_H
