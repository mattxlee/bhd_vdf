#include <array>
#include <boost/asio.hpp>
#include <iostream>
#include <string>
#include <vector>
namespace asio = boost::asio;
using namespace asio::ip;
using asio::detail::socket_ops::host_to_network_short;
using asio::detail::socket_ops::network_to_host_short;

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <plog/Appenders/ConsoleAppender.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Init.h>
#include <plog/Log.h>

#include "vdf_msgs.pb.h"

using Message = google::protobuf::Message;

class MessageFactory
{
public:
  virtual ~MessageFactory() {}

  virtual int get_msg_id() const = 0;

  virtual Message* parse(uint8_t const* data, int& size) const = 0;
};

template <int ID, typename T>
class MessageFactoryTmpl : public MessageFactory
{
public:
  int get_msg_id() const override { return ID; }

  Message* parse(uint8_t const* data, int& size) const override
  {
    T* msg = new T();
    if (!msg->ParseFromArray(data, size)) {
      // The message cannot be parsed
      delete msg;
      return nullptr;
    }
    size = msg->ByteSizeLong();
    return msg;
  }
};

using MessageFactoryVec = std::vector<MessageFactory const*>;

int const MSGID_PING = 1;
int const MSGID_PONG = 2;
int const MSGID_REQUESTVDF = 10;
int const MSGID_REQUESTVDF_REPLY = 11;
int const MSGID_VDFRESULT = 20;

using MsgFactory_Ping = MessageFactoryTmpl<MSGID_PING, Ping>;
using MsgFactory_Pong = MessageFactoryTmpl<MSGID_PONG, Pong>;
using MsgFactory_RequestVDF = MessageFactoryTmpl<MSGID_REQUESTVDF, RequestVDF>;
using MsgFactory_RequestVDFReply =
    MessageFactoryTmpl<MSGID_REQUESTVDF_REPLY, RequestVDFReply>;
using MsgFactory_VDFResult = MessageFactoryTmpl<MSGID_VDFRESULT, VDFResult>;

template <uint16_t MAX_SIZE>
class PacketAnalyzer
{
  MessageFactoryVec factories_;
  std::array<uint8_t, MAX_SIZE> data_;
  int p_{0};

public:
  ~PacketAnalyzer()
  {
    for (auto factory : factories_) {
      delete factory;
    }
  }

  template <typename T>
  void register_factory()
  {
    factories_.push_back(new T);
  }

  int remaining_bytes() const { return p_; }

  int write(uint8_t const* bytes, int size)
  {
    int available = MAX_SIZE - p_;
    int n = std::min<int>(size, available);
    memcpy(data_.data() + p_, bytes, n);
    p_ += n;
    return n;
  }

  Message* analyze()
  {
    // Analyze the data and create messages
    if (p_ < 3) {
      // There are not enough bytes for the message header
      return nullptr;
    }

    // First byte is message-id
    uint8_t msg_id = data_[0];
    auto i = std::find_if(std::begin(factories_), std::end(factories_),
                          [msg_id](MessageFactory const* factory) {
                            return factory->get_msg_id() == msg_id;
                          });
    if (i == std::end(factories_)) {
      return nullptr;
    }

    // Second word(2-byte) is the size of the message
    uint16_t size_network;
    memcpy(&size_network, data_.data() + 1, 2);
    int size = network_to_host_short(size_network);
    auto msg = (*i)->parse(data_.data() + 3, size);
    if (msg == nullptr) {
      return nullptr;
    }

    int bytes_to_copy = p_ - size;
    if (bytes_to_copy > 0) {
      memcpy(data_.data(), data_.data() + size, bytes_to_copy);
      p_ -= size;
    } else {
      p_ = 0;
    }

    return msg;
  }
};

class Session
{
  PacketAnalyzer<65535> analyzer_;

  static int const BUF_SIZE = 4096;
  tcp::socket sck_;
  uint8_t read_buf_[BUF_SIZE];

  void read_next()
  {
    sck_.async_read_some(
        asio::buffer(read_buf_, BUF_SIZE),
        [this](boost::system::error_code ec, std::size_t bytes) {
          if (ec) {
            // TODO Cannot read from peer
            return;
          }
          analyzer_.write(read_buf_, bytes);
          // Analyze message until no message can be found
          while (1) {
            auto msg = analyzer_.analyze();
            if (msg == nullptr) {
              break;
            }
            // TODO Notify there is a message has been received
          }
        });
  }

public:
  explicit Session(tcp::socket sck) : sck_(std::move(sck))
  {
    analyzer_.register_factory<MsgFactory_Ping>();
    analyzer_.register_factory<MsgFactory_RequestVDF>();
  }

  tcp::socket& get_socket() { return sck_; }

  void run() {}
};

using SessionVec = std::vector<Session>;

class Server
{
  asio::io_context& ioc_;
  tcp::acceptor acceptor_;
  SessionVec session_vec_;

  void accept_next_socket()
  {
    tcp::socket sck(ioc_);
    acceptor_.async_accept(sck, [this, sck = std::move(sck)](
                                    boost::system::error_code ec) mutable {
      if (ec) {
        // TODO Something is wrong
        return;
      }
      Session session(std::move(sck));
      session.run();

      session_vec_.push_back(std::move(session));
    });
  }

public:
  Server(boost::asio::io_context& ioc, tcp::endpoint const& endpoint)
      : ioc_(ioc), acceptor_(ioc, endpoint)
  {
  }

  int run()
  {
    PLOG_INFO << "exit VDF service.";
    return 0;
  }
};

struct Arguments {
  std::string listening_addr;
  unsigned short listening_port;
};

int main(int argc, const char* argv[])
{
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
  Server srv(ioc, tcp::endpoint(addr, args.listening_port));
  return srv.run();
}
