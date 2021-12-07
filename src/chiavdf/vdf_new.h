#ifndef VDF_NEW
#define VDF_NEW

#include "integer.h"

void normalize(integer& a, integer& b, integer& c);

void reduce_impl(integer& a, integer& b, integer& c);

void reduce(integer& a, integer& b, integer& c);

void generator_for_discriminant(const integer& d, integer& a, integer& b, integer& c);

void square(integer& a, integer& b, integer& c);

//reduced bounds:
// |b| <= a
// |a|< = sqrt(-d/3) <= sqrt(|d|)
// |c| = |(b^2-d)/(4a)| <= |b^2-d| = |b^2+|d|| <= |a^2+|d|| <= |-d/3+|d|| = ||d|/3+|d|| = |2/3*|d|| <= |d|
// a and b have half as many bits as d (rounded up). c can have as many bits as d (but it is usually half)
// |ac| = |(b^2-d)/(4)| <= |b^2-d| <= |d|
// |bc| <= |ac| <= |d|
// b is odd:
//  assume b=2n (even)
//  d = b^2-4ac = 4n^2 - 4ac = multiple of 4
//  d is prime so it is odd (contradiction)
struct form {
    integer a;
    integer b;
    integer c;

    static form from_abd(const integer& t_a, const integer& t_b, const integer& d) {
        form res;
        res.a=t_a;
        res.b=t_b;
        res.c=(t_b*t_b - d);

        if (t_a <= integer(0))
            throw std::runtime_error("Invalid form. Positive a");
        if (res.c % (t_a<<2) != integer(0))
            throw std::runtime_error("Invalid form. Can't find c.");

        res.c/=(t_a<<2);

        res.reduce();

        return res;
    }

    static form identity(const integer& d) {
        return from_abd(integer(1), integer(1), d);
    }

    static form generator(const integer& d) {
        return from_abd(integer(2), integer(1), d);
    }
    
    void reduce() {
        ::reduce(a, b, c);
    }

    bool is_reduced() {
        int a_cmp_c = mpz_cmp(a.impl, c.impl);

        // a <= c, and if a == c, then b >= 0
        if (a_cmp_c < 0 || (a_cmp_c == 0 && mpz_sgn(b.impl) >= 0)) {
            // -a < b <= a
            if (mpz_cmpabs(a.impl, b.impl) > 0 || mpz_cmp(a.impl, b.impl) == 0)
                return true;
        }
        return false;
    }

    form inverse() const {
        form res=*this;
        res.b=-res.b;
        res.reduce(); //doesn't do anything unless |a|==|b|
        return res;
    }

    bool check_valid(const integer& d) {
        return b*b-integer(4)*a*c==d;
    }

    void assert_valid(const integer& d) {
        assert(check_valid(d));
    }

    bool operator==(const form& f) const {
        return a==f.a && b==f.b && c==f.c;
    }

    bool operator<(const form& f) const {
        return make_tuple(a, b, c)<make_tuple(f.a, f.b, f.c);
    }

    //assumes this is normalized (c has the highest magnitude)
    //the inverse has the same hash
    int hash() const {
        uint64 res=c.to_vector()[0];
        return int((res>>4) & ((1ull<<31)-1)); //ignoring some of the lower bits because they might not be random enough
    }
};

integer generate_discriminant(int num_bits, int seed=-1);

form square(const form& f);

//inputs are: unsigned, unsigned, signed
integer three_gcd(integer a, integer b, integer c);

gcd_res test_gcd(integer a_signed, integer b_signed, int index=0);

//a and b are N bits and m is M bits; outputs are M bits
//a and b are signed and m is unsigned
//mu and v are unsigned
void solve_linear_congruence(const integer& a, const integer& b, const integer& m, integer& mu, integer& v, int index=0);

//f1.a,f1.b,f2.a,f2.b are N bits and f1.c,f2.c are 2N bits. a/c are unsigned and b is signed
form multiply(const form& f1, const form& f2);

form operator*(const form& a, const form& b);

/*integer arg_discriminant;
int arg_iterations;

void parse_args(int argc, char** argv) {
    arg_discriminant=integer(
        "-0xdc2a335cd2b355c99d3d8d92850122b3d8fe20d0f5360e7aaaecb448960d57bcddfee12a229bbd8d370feda5a17466fc725158ebb78a2a7d37d0a226d89b54434db9c3be9a9bb6ba2c2cd079221d873a17933ceb81a37b0665b9b7e247e8df66bdd45eb15ada12326db01e26c861adf0233666c01dec92bbb547df7369aed3b1fbdff867cfc670511cc270964fbd98e5c55fbe0947ac2b9803acbfd935f3abb8d9be6f938aa4b4cc6203f53c928a979a2f18a1ff501b2587a93e95a428a107545e451f0ac6c7f520a7e99bf77336b1659a2cb3dd1b60e0c6fcfffc05f74cfa763a1d0af7de9994b6e35a9682c4543ae991b3a39839230ef84dae63e88d90f457"
    );
    arg_iterations=1000;

    if (argc==1) {
    } else
    if (argc==2) {
        arg_iterations=from_string<int>(argv);
    } else
    if (argc==3) {
        arg_discriminant=integer(argv[1]);
        arg_iterations=from_string<int>(argv[2]);
    } else {
        assert(false);
    }
}**/

#endif // VDF_NEW
