#ifndef INTEGER_H
#define INTEGER_H

#include "include.h"
#include "parameters.h"
#include "integer_common.h"

extern "C" {
#include "xgcd_partial.h"
}

//a and b are nonnegative
void xgcd_partial(integer& u, integer& v, integer& a, integer& b, const integer& L);

void inject_error(mpz_struct* i);

// end Headerguard INTEGER_H
#endif
