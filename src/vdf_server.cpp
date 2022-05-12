#include <iostream>
#include <chrono>
#include <sstream>
#include <optional>

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <plog/Init.h>
#include <plog/Appenders/ConsoleAppender.h>

#include "vdf_net.hpp"
#include "vdf_utils.h"

#include "vdf_computer.h"
#include "vdf_computer_thread.hpp"

using vdf::types::Bytes;

struct Arguments {
    std::string listening_addr;
    unsigned short listening_port;
};

void handle_session_message(net::Message const* pmsg, uint8_t msg_id, net::SessionPtr psession, net::Server& srv) {
    PLOG_INFO << "received message " << net::msg_id_to_string(msg_id) << " from session " << std::hex
              << address_to_string(psession.get());
    if (msg_id == net::MSGID_PING) {
        // Send PONG back
        auto pmsg_ping = static_cast<Ping const*>(pmsg);
        int32_t id = pmsg_ping->id();
        auto pmsg_pong = std::make_unique<Pong>();
        pmsg_pong->set_reply_id(id);
        psession->write_message(pmsg_pong.get(), net::MSGID_PONG);
    } else if (msg_id == net::MSGID_REQUESTVDF) {
        // Start a new thread to calculate VDF proof
        auto request_vdf_msg = static_cast<RequestVDF const*>(pmsg);
        Bytes infusion = to_bytes(request_vdf_msg->infusion());
        Bytes x = to_bytes(request_vdf_msg->x());
        uint64_t iters = request_vdf_msg->iters();
        // Start a new thread to calculate VDF proof
        auto pthread = std::make_shared<VDFComputerThread>(
            std::move(infusion), std::move(x), iters, [psession](std::optional<VDFComputerThread::Result> result) {
                if (!result) {
                    // We don't have a valid result, skip
                    PLOG_ERROR << "VDF computer thread is finished without a valid result";
                    return;
                }
                // Prepare the reply message
                auto pmsg_reply = std::make_unique<VDFResult>();
                pmsg_reply->set_infusion(to_string(result->infusion));
                pmsg_reply->set_x(to_string(result->x));
                pmsg_reply->set_iters(result->iters);
                pmsg_reply->set_y(to_string(result->y));
                pmsg_reply->set_proof(to_string(result->proof));
                pmsg_reply->set_duration(result->duration);
                psession->write_message(pmsg_reply.get(), net::MSGID_VDFRESULT);
            });
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
