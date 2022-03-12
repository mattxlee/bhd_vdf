#include <iostream>

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include "vdf_net.hpp"

struct Arguments {
  std::string listening_addr;
  unsigned short listening_port;
};

void handle_session_message(
    net::Message const* msg, uint8_t msg_id, net::Session& session) {
  // TODO handle message here
}

void handle_connect_error(boost::system::error_code ec) {
  PLOG_ERROR << "error on accept a new connection - " << ec;
}

void handle_session_error(
    boost::system::error_code ec, net::ActionType action_type,
    net::Session& session) {
  PLOG_ERROR << "error on session - on " << to_string(action_type) << " - "
             << ec;
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
      {new net::MsgFactory_Ping, new net::MsgFactory_RequestVDF});
  srv.set_session_message_handler(handle_session_message);
  srv.set_connect_error_handler(handle_connect_error);
  srv.set_session_error_handler(handle_session_error);
  int ret = srv.run();
  ioc.run();
  return ret;
}
