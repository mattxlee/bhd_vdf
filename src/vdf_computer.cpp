#include "vdf_computer.h"

#include <thread>

#include <spdlog/spdlog.h>

#include "vdf.h"

#include "callback.h"

#include "create_discriminant.h"

namespace vdf
{

namespace utils
{

std::string BytesToStr(std::vector<unsigned char> const& in)
{
    std::ostringstream oss;
    for (auto ch : in)
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(ch);
    return oss.str();
}

std::string ProofToHex(Proof const& proof)
{
    std::vector<uint8_t> res(proof.y.size() + proof.proof.size());
    memcpy(res.data(), proof.y.data(), proof.y.size());
    memcpy(res.data() + proof.y.size(), proof.proof.data(), proof.proof.size());
    return BytesToStr(res);
}

Proof CreateProof(std::vector<uint8_t> y, std::vector<uint8_t> proof)
{
    Proof res;
    res.proof = std::move(proof);
    res.y = std::move(y);
    return res;
}

} // namespace utils

struct ComputerMembers {
    std::vector<uint8_t> challenge;
    int discriminant_size_bits;

    integer D;
    std::vector<uint8_t> initial_form;

    std::atomic<bool> stopped;
    uint64_t iters { 0 };
    Proof proof;
};

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
                    spdlog::info("Maximum possible number of iterations reached!");
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

    spdlog::info("VDF loop finished. Total iters: {}", num_iterations);
#ifdef VDF_TEST
    spdlog::info("fast average batch size {}", double(num_iterations_fast) / double(num_calls_fast));
    spdlog::info("fast iterations per slow iteration {}", double(num_iterations_fast) / double(num_iterations_slow));
#endif
}

Proof CreateAndWriteProofOneWeso(
    uint64_t iters, integer& D, form f, OneWesolowskiCallback* weso, std::atomic<bool>& stopped)
{
    Proof proof = ProveOneWesolowski(iters, D, f, weso, stopped);
    if (stopped) {
        spdlog::info("Got stop signal before completing the proof!");
    }
    return proof;
}

Computer::Computer(std::vector<uint8_t> challenge, int discriminant_size_bits, std::vector<uint8_t> initial_form)
{
    memImpl_ = new ComputerMembers;
    memImpl_->challenge = std::move(challenge);
    memImpl_->discriminant_size_bits = discriminant_size_bits;
    memImpl_->initial_form = std::move(initial_form);
    memImpl_->D
        = CreateDiscriminant(const_cast<std::vector<uint8_t>&>(memImpl_->challenge), memImpl_->discriminant_size_bits);

    fesetround(FE_TOWARDZERO);
}

Computer::~Computer() { }

void Computer::Run(uint64_t iter)
{
    try {
        integer L = root(-memImpl_->D, 4);
        spdlog::info("Discriminant = {}", to_string(memImpl_->D.impl));
        form f = DeserializeForm(memImpl_->D, memImpl_->initial_form.data(), memImpl_->initial_form.size());
        auto weso = std::make_unique<OneWesolowskiCallback>(memImpl_->D, f, iter);
        FastStorage* fast_storage = NULL;
        memImpl_->stopped = false;
        // Starting the calculation
        std::thread vdf_worker(RepeatedSquare, f, std::ref(memImpl_->D), std::ref(L), weso.get(), fast_storage,
            std::ref(memImpl_->stopped));
        std::thread th_prover(
            CreateAndWriteProofOneWeso, iter, std::ref(memImpl_->D), f, weso.get(), std::ref(memImpl_->stopped));
        // Calculation is finished
        memImpl_->stopped = true;
        vdf_worker.join();
        th_prover.join();
    } catch (std::exception& e) {
        spdlog::error("Exception in thread: {}", to_string(e.what()));
    }
}

void Computer::SetStop(bool stopped) { memImpl_->stopped = stopped; }

} // namespace vdf
