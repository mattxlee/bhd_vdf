#include "vdf_computer.h"

#include <sstream>

#if !defined(_WIN32)
#include "vdf.h"

int gcd_base_bits = 50;
int gcd_128_max_iter = 3;

#endif

#include "verifier.h"

#include "create_discriminant.h"

std::ostream& operator<<(std::ostream& os, __mpz_struct const* z) { return os; }

namespace vdf {

namespace types {

Integer::Integer(integer const& val) { val_.reset(new integer(val)); }

Integer::Integer(std::string const& str) { val_.reset(new integer(str)); }

integer Integer::Get_integer() const { return *val_; }

std::string Integer::FormatString() const {
    std::stringstream ss;
    ss << to_string(val_->impl);
    return ss.str();
}

}  // namespace types

namespace utils {

types::Integer CreateDiscriminant(Bytes const& challenge, int disc_size) {
    return types::Integer(::CreateDiscriminant(const_cast<Bytes&>(challenge), disc_size));
}

Bytes ConnectBytes(Bytes const& lhs, Bytes const& rhs) {
    Bytes res(lhs.size() + rhs.size());
    memcpy(res.data(), lhs.data(), lhs.size());
    memcpy(res.data() + lhs.size(), rhs.data(), rhs.size());
    return res;
}

Bytes SerializeProof(types::Proof const& proof) { return ConnectBytes(proof.y, proof.proof); }

bool VerifyProof(
    types::Integer const& D_int, Bytes const& proof_data, uint64_t iters, uint8_t depth, Bytes const& x_data) {
    static const uint8_t DEFAULT_ELEMENT[1] = {0x08};
    integer D = D_int.Get_integer();
    return CheckProofOfTimeNWesolowski(
        D, x_data.empty() ? DEFAULT_ELEMENT : x_data.data(), proof_data.data(), proof_data.size(), iters,
        DEFAULT_DISC_SIZE, depth);
}

uint8_t ValueFromHexChar(char ch) {
    char hex[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    char chlo = std::tolower(ch);
    auto it = std::find(std::begin(hex), std::end(hex), ch);
    if (it == std::end(hex)) {
        throw std::runtime_error("invalid hex character");
    }
    return std::distance(std::begin(hex), it);
}

Bytes BytesFromStr(std::string const& str) {
    if (str.size() % 2 != 0) {
        throw std::runtime_error("invalid hex string, the number of length cannot be divided by 2");
    }
    uint32_t size = str.size() / 2;
    std::vector<uint8_t> res(size);
    for (uint32_t i = 0; i < size; ++i) {
        uint8_t byte = (ValueFromHexChar(str[i * 2]) << 4) + ValueFromHexChar(str[i * 2 + 1]);
        res[i] = byte;
    }
    return res;
}

Bytes GetDefaultForm() {
    Bytes default_form(100, 0);
    default_form[0] = 8;
    return default_form;
}

}  // namespace utils

void CreateAndWriteProofOneWeso(
    uint64_t iters, integer& D, form f, OneWesolowskiCallback* weso, std::atomic<bool>& stopped, types::Proof& out) {
    Proof proof = ProveOneWesolowski(iters, D, f, weso, stopped);
    if (stopped) {
        print("Got stop signal before completing the proof!");
    }
    out.y = proof.y;
    out.proof = proof.proof;
    out.witness_type = proof.witness_type;
    stopped = true;
}

int FORM_SIZE() { return BQFC_FORM_SIZE; }

void Computer::InitializeComputer() {
    init_gmp();
    if (hasAVX2()) {
        gcd_base_bits = 63;
        gcd_128_max_iter = 2;
    }
}

Computer::Computer(types::Integer D) : D_(std::move(D)) {}

Computer::Computer(types::Integer D, Bytes initial_form) : D_(std::move(D)), initial_form_(std::move(initial_form)) {}

Computer::~Computer() {}

void Computer::Run(uint64_t iter) {
    stopped_ = false; // reset flag `stop`
#if defined(_WIN32)
    while (!stopped_)
        ;
#else
    try {
        std::lock_guard<std::mutex> __lock_guard(m_);

        integer D = D_.Get_integer();
        print("discriminant = ", D_.FormatString());
        assert(fesetround(FE_TOWARDZERO) == 0);
        integer L = root(-D, 4);
        form f;
        if (initial_form_.empty()) {
            f = form::generator(D);
        } else {
            f = DeserializeForm(D, initial_form_.data(), initial_form_.size());
        }
        print("form initialized");
        std::unique_ptr<OneWesolowskiCallback> weso(new OneWesolowskiCallback(D, f, iter));
        FastStorage* fast_storage = NULL;
        // Starting the calculation
        std::thread vdf_worker(repeated_square, f, std::ref(D), std::ref(L), weso.get(), fast_storage, std::ref(stopped_));
        std::thread th_prover(
            CreateAndWriteProofOneWeso, iter, std::ref(D), f, weso.get(), std::ref(stopped_), std::ref(proof_));
        iters_ = iter;  // Assign the number of iterations
        // Calculation is finished
        vdf_worker.join();
        th_prover.join();
        print("calculating is finished");
    } catch (std::exception& e) {
        print("run error: ", to_string(e.what()));
    }
#endif
    }

    void Computer::SetStop(bool stopped) { stopped_ = stopped; }

    void Computer::Join() { m_.lock(); }

}  // namespace vdf
