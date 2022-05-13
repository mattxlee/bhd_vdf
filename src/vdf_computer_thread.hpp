#ifndef VDF_COMPUTER_THREAD_HPP
#define VDF_COMPUTER_THREAD_HPP

#include <cstdint>

#include <functional>
#include <optional>

class VDFComputerThread {
public:
    struct Result {
        Bytes infusion;
        Bytes x;
        uint64_t iters;
        Bytes y;
        Bytes proof;
        double duration;
    };
    using VDFResultHandler = std::function<void(std::optional<Result>)>;

    VDFComputerThread(Bytes infusion, Bytes x, uint64_t iters, VDFResultHandler handler)
        : infusion_(std::move(infusion)), x_(std::move(x)), iters_(iters), result_handler_(std::move(handler)) {}

    void run() {
        assert(pthread_ == nullptr);
        pthread_.reset(new std::thread([this]() { thread_proc(); }));
    }

    void stop() {
        if (pthread_ == nullptr || pcomputer_ == nullptr) {
            // The computer or the thread doesn't exist
            return;
        }
        require_interruption_ = true;
        stop_flag_ = true;

        // Join the thread
        pthread_->join();
    }

private:
    void thread_proc() {
        require_interruption_ = false;  // reset the flag to `false`
        auto D = vdf::utils::CreateDiscriminant(infusion_);
        pcomputer_.reset(new vdf::Computer(D, x_));
        auto begin_time = std::chrono::system_clock::now();
        pcomputer_->Run(iters_, stop_flag_);
        auto end_time = std::chrono::system_clock::now();
        if (require_interruption_) {
            // We do not provide proof on this status
            result_handler_({});
            return;
        }
        // Retrieve the proof and prepare the result
        vdf::types::Proof proof = pcomputer_->GetProof();
        Result res;
        res.infusion = infusion_;
        res.x = x_;
        res.iters = iters_;
        res.y = proof.y;
        res.proof = proof.proof;
        auto duration = end_time - begin_time;
        res.duration = duration.count();
        // Invoke the callback
        assert(result_handler_);
        result_handler_(res);
    }

private:
    Bytes infusion_;
    Bytes x_;
    uint64_t iters_;
    std::unique_ptr<std::thread> pthread_;
    std::atomic_bool stop_flag_;
    std::atomic_bool require_interruption_{false};
    VDFResultHandler result_handler_;
    std::unique_ptr<vdf::Computer> pcomputer_;
};
using VDFComputerThreadPtr = std::shared_ptr<VDFComputerThread>;

#endif
