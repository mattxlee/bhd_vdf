#include <iostream>
#include <chrono>
#include <sstream>

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <plog/Init.h>
#include <plog/Appenders/ConsoleAppender.h>

#include "vdf_net.hpp"
#include "vdf_computer.h"
#include "vdf_utils.h"

using vdf::types::Bytes;

struct Arguments {
    std::string listening_addr;
    unsigned short listening_port;
};

class VDFComputerThread {
public:
    VDFComputerThread(
        Bytes infusion, Bytes x, uint64_t iters, net::SessionPtr psession, std::function<void()> session_end_handler)
        : infusion_(std::move(infusion)), x_(std::move(x)), iters_(iters), psession_(psession) {}

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
            return;
        }
        vdf::types::Proof proof = pcomputer_->GetProof();
        // Prepare the reply message
        auto result = std::make_unique<VDFResult>();
        result->set_infusion(to_string(infusion_));
        result->set_x(to_string(x_));
        result->set_iters(iters_);
        result->set_y(to_string(proof.y));
        result->set_proof(to_string(proof.proof));
        auto duration = end_time - begin_time;
        result->set_duration(duration.count());
        // Send proof to session
        psession_->write_message(result.get(), net::MSGID_VDFRESULT);
        // Mark the thread is end and notify the manager
        session_end_handler_();
    }

private:
    Bytes infusion_;
    Bytes x_;
    uint64_t iters_;
    std::unique_ptr<std::thread> pthread_;
    std::atomic_bool stop_flag_;
    std::atomic_bool require_interruption_{false};
    net::SessionPtr psession_;
    std::function<void()> session_end_handler_;
    std::unique_ptr<vdf::Computer> pcomputer_;
};
using VDFComputerThreadPtr = std::shared_ptr<VDFComputerThread>;

class VDFComputerManager {
public:
    void handle_session_end(net::SessionPtr psession) {}

    void start_new(Bytes infusion, Bytes x, uint64_t iters, net::SessionPtr psession) {
        auto pthread = std::make_shared<VDFComputerThread>(
            std::move(infusion), std::move(x), iters, psession, [this, psession]() { handle_session_end(psession); });
        pthread->run();
        PLOG_ERROR << "new thread " << address_to_string(pthread.get()) << " on session "
                   << address_to_string(psession.get());
        {
            std::lock_guard<std::mutex> __lock_guard(threads_mtx_);
            threads_.push_back(pthread);
        }
    }

private:
    std::mutex threads_mtx_;
    std::vector<VDFComputerThreadPtr> threads_;
};

static VDFComputerManager g_computer_manager;

void handle_session_message(net::Message const* msg, uint8_t msg_id, net::SessionPtr psession, net::Server& srv) {
    PLOG_INFO << "received message " << net::msg_id_to_string(msg_id) << " from session " << std::hex
              << address_to_string(psession.get());
    if (msg_id == net::MSGID_REQUESTVDF) {
        // Start a new thread to calculate VDF proof
        auto request_vdf_msg = static_cast<RequestVDF const*>(msg);
        Bytes infusion = to_bytes(request_vdf_msg->infusion());
        Bytes x = to_bytes(request_vdf_msg->x());
        uint64_t iters = request_vdf_msg->iters();
        // Start a new thread to calculate VDF proof
        g_computer_manager.start_new(std::move(infusion), std::move(x), iters, psession);
    }
}

void handle_connect_error(boost::system::error_code ec) { PLOG_ERROR << "error on accept a new connection - " << ec; }

void handle_session_error(boost::system::error_code ec, net::ActionType action_type, net::SessionPtr psession) {
    PLOG_ERROR << "session " << address_to_string(psession.get()) << " action: " << to_string(action_type)
               << " error: " << ec.message();
}

int main(int argc, const char* argv[]) {
    Arguments args;
    po::options_description desc;
    desc.add_options()                    // All arguments
        ("help,h", "show help document")  // --help
        ("address", po::value(&args.listening_addr)->default_value("127.0.0.1"),
         "vdf will listen to this address")  // --address
        ("port", po::value(&args.listening_port)->default_value(9090),
         "vdf will listen to this port")  // --port
        ;

    po::variables_map vars;
    po::store(po::parse_command_line(argc, argv, desc), vars);
    po::notify(vars);

    if (vars.count("help")) {
        std::cout << "Usage:" << std::endl;
        std::cout << desc << std::endl;
        return 0;
    }

    plog::ConsoleAppender<plog::TxtFormatter> console_appender;
    plog::init(plog::Severity::info, &console_appender);

    asio::io_context ioc;
    auto addr = address::from_string(args.listening_addr);
    PLOG_INFO << "listening on " << addr << ":" << args.listening_port;
    net::Server srv(
        ioc, tcp::endpoint(addr, args.listening_port),
        {std::make_shared<net::MsgFactory_Ping>(), std::make_shared<net::MsgFactory_RequestVDF>()});
    srv.set_session_message_handler([&srv](net::Message const* pmsg, uint8_t msg_id, net::SessionPtr psession) {
        handle_session_message(pmsg, msg_id, psession, srv);
    });
    srv.set_connect_error_handler(handle_connect_error);
    srv.set_session_error_handler(handle_session_error);
    srv.run();
    ioc.run();
    return 0;
}
