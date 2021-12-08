#include "vdf_computer.h"

#include <thread>

#include <spdlog/spdlog.h>

#include "vdf.h"

#include "create_discriminant.h"

namespace vdf
{

namespace types
{

Integer::Integer(Bytes data)
    : data_(std::move(data))
{
}

Bytes Integer::ToBytes() const { return data_; }

} // namespace types

namespace utils
{

types::Integer CreateDiscriminant(types::Bytes const& challenge, int disc_size)
{
    integer D = ::CreateDiscriminant(const_cast<types::Bytes&>(challenge), disc_size);
    return types::Integer(D.to_bytes());
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

void CreateAndWriteProofOneWeso(
    uint64_t iters, integer& D, form f, OneWesolowskiCallback* weso, std::atomic<bool>& stopped, types::Proof& out)
{
    Proof proof = ProveOneWesolowski(iters, D, f, weso, stopped);
    if (stopped) {
        spdlog::info("Got stop signal before completing the proof!");
    }
    out.y = proof.y;
    out.proof = proof.proof;
}

Computer::Computer(types::Integer D, types::Bytes initial_form)
    : D_(std::move(D))
    , initial_form_(std::move(initial_form))
{
    fesetround(FE_TOWARDZERO);
}

Computer::~Computer() { }

void Computer::Run(uint64_t iter)
{
    try {
        integer D { D_.ToBytes() };
        integer L = root(D, 4);
        spdlog::info("Discriminant = {}", to_string(D.impl));
        form f = DeserializeForm(D, initial_form_.data(), initial_form_.size());
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
        stopped_ = true;
        vdf_worker.join();
        th_prover.join();
    } catch (std::exception& e) {
        spdlog::error("Exception in thread: {}", to_string(e.what()));
    }
}

void Computer::SetStop(bool stopped) { stopped = stopped; }

} // namespace vdf
