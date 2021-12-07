#ifndef VDF_NEW_H
#define VDF_NEW_H

/**
    Copyright (C) 2012 William Hart
    
    Permission is hereby granted, free of charge, to any person obtaining a copy of this
    software and associated documentation files (the "Software"), to deal in the Software 
    without restriction, including without limitation the rights to use, copy, modify, merge, 
    publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons 
    to whom the Software is furnished to do so, subject to the following conditions:
    The above copyright notice and this permission notice shall be included in all copies or
    substantial portions of the Software.
    
    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
    INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
    PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE
    FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
    OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
    
    MIT licensing permission obtained January 13, 2020 by Chia Network Inc. from William Hart

Copyright 2020 Chia Network Inc

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
**/

#include "integer_common.h"

#define LOG2(X) (63 - __builtin_clzll((X)))
//using namespace std;

typedef struct qfb
{
    mpz_t a;
    mpz_t b;
    mpz_t c;
} qfb;

typedef qfb qfb_t[1];

struct form;

// From Antic using Flint (works!)
void qfb_nucomp(qfb_t r, const qfb_t f, const qfb_t g, mpz_t& D, mpz_t& L);

// a = b * c
void nucomp_form(form &a, form &b, form &c, integer &D, integer &L);

void qfb_nudupl(qfb_t r, qfb_t f, mpz_t D, mpz_t L);

// a = b * b
void nudupl_form(form &a, form &b, integer &D, integer &L);

#endif // VDF_NEW_H
