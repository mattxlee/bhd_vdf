#include "integer_common.h"

int mpz_num_bits_upper_bound(mpz_struct* v) { return mpz_size(v) * sizeof(mp_limb_t) * 8; }

integer abs(const integer& t)
{
    integer res;
    mpz_abs(res.impl, t.impl);
    return res;
}

integer root(const integer& t, int n)
{
    integer res;
    mpz_root(res.impl, t.impl, n);
    return res;
}

// a*s + b*t = gcd ; gcd>=0
//  abs(s) < abs(b) / (2 gcd)
//  abs(t) < abs(a) / (2 gcd)
//(except if |s|<=1 or |t|<=1)
gcd_res gcd(const integer& a, const integer& b)
{
    gcd_res res;

    mpz_gcdext(res.gcd.impl, res.s.impl, res.t.impl, a.impl, b.impl);

    return res;
}

integer rand_integer(int num_bits, int seed)
{
    thread_local gmp_randstate_t state;
    thread_local bool is_init = false;

    if (!is_init) {
        gmp_randinit_mt(state);
        gmp_randseed_ui(state, 0);
        is_init = true;
    }

    if (seed != -1) {
        gmp_randseed_ui(state, seed);
    }

    integer res;
    assert(num_bits >= 0);
    mpz_urandomb(res.impl, state, num_bits);
    return res;
}

USED string to_string(mpz_struct* t)
{
    integer t_int;
    mpz_set(t_int.impl, t);
    return t_int.to_string();
}
