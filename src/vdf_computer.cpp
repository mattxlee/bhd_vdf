#include "vdf_computer.h"

#include <sstream>
#include <thread>

#include "vdf.h"

#include "verifier.h"

#include "create_discriminant.h"

int gcd_base_bits = 50;
int gcd_128_max_iter = 3;

namespace vdf
{

namespace types
{

Integer::Integer(integer const& val) { val_.reset(new integer(val)); }

Integer::Integer(std::string_view str) { val_.reset(new integer(str.data())); }

integer const& Integer::Get_integer() const { return *val_; }

std::string Integer::FormatString() const
{
    std::stringstream ss;
    ss << val_->impl;
    return ss.str();
}

} // namespace types

namespace utils
{

types::Integer CreateDiscriminant(types::Bytes const& challenge, int disc_size)
{
    return types::Integer(::CreateDiscriminant(const_cast<types::Bytes&>(challenge), disc_size));
}

types::Bytes SerializeProof(types::Proof const& proof)
{
    std::vector<unsigned char> bytes;
    bytes.insert(bytes.end(), proof.y.begin(), proof.y.end());
    bytes.insert(bytes.end(), proof.proof.begin(), proof.proof.end());
    return bytes;
}

bool VerifyProof(
    types::Integer const& D, types::Bytes const& proof, uint64_t iters, uint8_t witness_type, types::Bytes const& x)
{
    return CheckProofOfTimeNWesolowski(
        D.Get_integer(), x.data(), proof.data(), proof.size(), iters, DEFAULT_DISC_SIZE, witness_type);
}

uint8_t ValueFromHexChar(char ch)
{
    char hex[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
    char chlo = std::tolower(ch);
    auto it = std::find(std::begin(hex), std::end(hex), ch);
    if (it == std::end(hex)) {
        throw std::runtime_error("invalid hex character");
    }
    return std::distance(std::begin(hex), it);
}

types::Bytes BytesFromStr(std::string_view str)
{
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

types::Bytes GetDefaultForm()
{
    types::Bytes default_form(100, 0);
    default_form[0] = 8;
    return default_form;
}

} // namespace utils

// thread safe; but it is only called from the main thread
void RepeatedSquare(form f, const integer& D, const integer& L, WesolowskiCallback* weso, FastStorage* fast_storage,
    std::atomic<bool>& stopped)
{
#ifdef VDF_TEST
    uint64 num_calls_fast = 0;
    uint64 num_iterations_fast = 0;
    uint64 num_iterations_slow = 0;
#endif

    uint64_t num_iterations = 0;
    uint64_t last_checkpoint = 0;

    while (!stopped) {
        uint64 c_checkpoint_interval = checkpoint_interval;

#ifdef VDF_TEST
        form f_copy;
        form f_copy_3;
        bool f_copy_3_valid = false;
        if (vdf_test_correctness) {
            f_copy = f;
            c_checkpoint_interval = 1;

            f_copy_3 = f;
            f_copy_3_valid = square_fast_impl(f_copy_3, D, L, num_iterations);
        }
#endif

        uint64 batch_size = c_checkpoint_interval;

#ifdef ENABLE_TRACK_CYCLES
        print("track cycles enabled; results will be wrong");
        repeated_square_original(*weso->vdfo, f, D, L, 100); // randomize the a and b values
#endif

        // This works single threaded
        square_state_type square_state;
        square_state.pairindex = 0;

        uint64 actual_iterations = repeated_square_fast(square_state, f, D, L, num_iterations, batch_size, weso);

#ifdef VDF_TEST
        ++num_calls_fast;
        if (actual_iterations != ~uint64(0))
            num_iterations_fast += actual_iterations;
#endif

#ifdef ENABLE_TRACK_CYCLES
        print("track cycles actual iterations", actual_iterations);
        return; // exit the program
#endif

        if (actual_iterations == ~uint64(0)) {
            // corruption; f is unchanged. do the entire batch with the slow algorithm
            repeated_square_original(*weso->vdfo, f, D, L, num_iterations, batch_size, weso);
            actual_iterations = batch_size;

#ifdef VDF_TEST
            num_iterations_slow += batch_size;
#endif

            if (warn_on_corruption_in_production) {
                print("!!!! corruption detected and corrected !!!!");
            }
        }

        if (actual_iterations < batch_size) {
            // the fast algorithm terminated prematurely for whatever reason. f is still valid
            // it might terminate prematurely again (e.g. gcd quotient too large), so will do one iteration of the slow
            // algorithm this will also reduce f if the fast algorithm terminated because it was too big
            repeated_square_original(*weso->vdfo, f, D, L, num_iterations + actual_iterations, 1, weso);

#ifdef VDF_TEST
            ++num_iterations_slow;
            if (vdf_test_correctness) {
                assert(actual_iterations == 0);
                print("fast vdf terminated prematurely", num_iterations);
            }
#endif

            ++actual_iterations;
        }

        num_iterations += actual_iterations;
        if (num_iterations >= last_checkpoint) {
            weso->iterations = num_iterations;

            // n-weso specific logic.
            if (fast_algorithm) {
                if (fast_storage != NULL) {
                    fast_storage->SubmitCheckpoint(static_cast<FastAlgorithmCallback*>(weso)->y_ret, last_checkpoint);
                } else if (last_checkpoint % (1 << 16) == 0) {
                    // Notify prover event loop, we have a new segment with intermediates stored.
                    {
                        std::lock_guard<std::mutex> lk(new_event_mutex);
                        new_event = true;
                    }
                    new_event_cv.notify_all();
                }
            }

            // 2-weso specific logic.
            if (two_weso) {
                TwoWesolowskiCallback* nweso = (TwoWesolowskiCallback*)weso;
                if (num_iterations >= kSwitchIters && !nweso->LargeConstants()) {
                    uint64 round_up = (100 - num_iterations % 100) % 100;
                    if (round_up > 0) {
                        repeated_square_original(*weso->vdfo, f, D, L, num_iterations, round_up, weso);
                    }
                    num_iterations += round_up;
                    nweso->IncreaseConstants(num_iterations);
                    weso->iterations = num_iterations;
                }
                if (num_iterations >= kMaxItersAllowed - 500000) {
                    print("Maximum possible number of iterations reached!");
                    return;
                }
            }

            last_checkpoint += (1 << 15);
        }

#ifdef VDF_TEST
        if (vdf_test_correctness) {
            form f_copy_2 = f;
            weso->reduce(f_copy_2);

            repeated_square_original(*weso->vdfo, f_copy, D, L, actual_iterations);
            assert(f_copy == f_copy_2);
        }
#endif
    }

    print("VDF loop finished. Total iters: ", num_iterations);
#ifdef VDF_TEST
    print("fast average batch size ", double(num_iterations_fast) / double(num_calls_fast));
    print("fast iterations per slow iteration ", double(num_iterations_fast) / double(num_iterations_slow));
#endif
}

void CreateAndWriteProofOneWeso(
    uint64_t iters, integer& D, form f, OneWesolowskiCallback* weso, std::atomic<bool>& stopped, types::Proof& out)
{
    Proof proof = ProveOneWesolowski(iters, D, f, weso, stopped);
    if (stopped) {
        print("Got stop signal before completing the proof!");
    }
    out.y = proof.y;
    out.proof = proof.proof;
    out.witness_type = proof.witness_type;
    stopped = true;
}

void Computer::InitializeComputer()
{
    init_gmp();
    if (hasAVX2()) {
        gcd_base_bits = 63;
        gcd_128_max_iter = 2;
    }
}

Computer::Computer(types::Integer D)
    : D_(std::move(D))
{
}

Computer::Computer(types::Integer D, types::Bytes initial_form)
    : D_(std::move(D))
    , initial_form_(std::move(initial_form))
{
}

Computer::~Computer() { }

void Computer::Run(uint64_t iter)
{
    try {
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
        auto weso = std::make_unique<OneWesolowskiCallback>(D, f, iter);
        FastStorage* fast_storage = NULL;
        stopped_ = false;
        // Starting the calculation
        std::thread vdf_worker(
            RepeatedSquare, f, std::ref(D), std::ref(L), weso.get(), fast_storage, std::ref(stopped_));
        std::thread th_prover(
            CreateAndWriteProofOneWeso, iter, std::ref(D), f, weso.get(), std::ref(stopped_), std::ref(proof_));
        iters_ = iter; // Assign the number of iterations
        // Calculation is finished
        vdf_worker.join();
        th_prover.join();
        print("calculating is finished");
    } catch (std::exception& e) {
        print("run error: ", to_string(e.what()));
    }
}

void Computer::SetStop(bool stopped) { stopped = stopped; }

} // namespace vdf
