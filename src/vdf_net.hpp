#include <array>
#include <deque>
#include <iostream>
#include <string>
#include <vector>
#include <functional>

#include <boost/asio.hpp>

namespace asio = boost::asio;
using namespace asio::ip;
using asio::detail::socket_ops::host_to_network_short;
using asio::detail::socket_ops::network_to_host_short;

#include <plog/Appenders/ConsoleAppender.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Init.h>
#include <plog/Log.h>

#include "vdf_msgs.pb.h"

namespace net {

using Message = google::protobuf::Message;

class MessageFactory {
public:
  virtual ~MessageFactory() {}

  virtual int get_msg_id() const = 0;

  virtual Message* parse(uint8_t const* data, int& size) const = 0;
};

template <int ID, typename T>
class MessageFactoryTmpl : public MessageFactory {
public:
  int get_msg_id() const override { return ID; }

  Message* parse(uint8_t const* data, int& size) const override {
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

int const MAX_MSG_SIZE = 65530;
int const PACKET_HEAD_SIZE = 3;

template <uint16_t MAX_SIZE = MAX_MSG_SIZE>
class PacketAnalyzer {
  MessageFactoryVec factories_;
  std::array<uint8_t, MAX_SIZE> data_;
  int p_{0};

public:
  PacketAnalyzer(MessageFactoryVec factories)
      : factories_(std::move(factories)) {}

  ~PacketAnalyzer() {
    for (auto factory : factories_) {
      delete factory;
    }
  }

  int remaining_bytes() const { return p_; }

  int write(uint8_t const* bytes, int size) {
    int available = MAX_SIZE - p_;
    int n = std::min<int>(size, available);
    memcpy(data_.data() + p_, bytes, n);
    p_ += n;
    return n;
  }

  Message* analyze() {
    // Analyze the data and create messages
    if (p_ < PACKET_HEAD_SIZE) {
      // There are not enough bytes for the message header
      return nullptr;
    }

    // First byte is message-id
    uint8_t msg_id = data_[0];
    auto i = std::find_if(
        std::begin(factories_), std::end(factories_),
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
    auto msg = (*i)->parse(data_.data() + PACKET_HEAD_SIZE, size);
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

class PacketBuilder {
public:
  std::vector<uint8_t> build_message(Message const* msg, uint8_t msg_id) {
    if (msg->ByteSizeLong() > MAX_MSG_SIZE) {
      throw std::runtime_error("the message size is too large");
    }
    int packet_size = PACKET_HEAD_SIZE + msg->ByteSizeLong();
    std::vector<uint8_t> packet(packet_size, 0);
    uint16_t size = static_cast<uint16_t>(msg->ByteSizeLong());
    uint16_t size_network = host_to_network_short(size);
    packet[0] = msg_id;
    memcpy(packet.data() + 1, &size_network, sizeof(size_network));
    bool succ = msg->SerializeToArray(packet.data() + PACKET_HEAD_SIZE, size);
    if (!succ) {
      return {};
    }
    return packet;
  }
};

enum class ActionType { Read, Write, Connect };
std::string to_string(ActionType action_type);

using ErrorHandler = std::function<void(boost::system::error_code, ActionType)>;
using MessageHandler = std::function<void(Message const*)>;

class Session {
  PacketAnalyzer<MAX_MSG_SIZE> analyzer_;

  static int const BUF_SIZE = 4096;
  tcp::socket sck_;
  uint8_t read_buf_[BUF_SIZE];

  PacketBuilder packet_builder_;
  std::deque<std::vector<uint8_t>> write_buf_deq_;

  MessageHandler message_handler_;
  ErrorHandler error_handler_;

  void read_next() {
    sck_.async_read_some(
        asio::buffer(read_buf_, BUF_SIZE),
        [this](boost::system::error_code ec, std::size_t bytes) {
          if (ec) {
            if (error_handler_) {
              error_handler_(ec, ActionType::Read);
            }
            return;
          }
          analyzer_.write(read_buf_, bytes);
          // Analyze message until no message can be found
          while (1) {
            std::unique_ptr<Message> msg(analyzer_.analyze());
            if (msg == nullptr) {
              break;
            }
            if (message_handler_) {
              message_handler_(msg.get());
            }
          }
          read_next();
        });
  }

  void write_next() {
    std::vector<uint8_t> const& buf = write_buf_deq_.front();
    asio::async_write(
        sck_, asio::buffer(buf),
        [this](boost::system::error_code ec, std::size_t bytes) {
          if (ec) {
            if (error_handler_) {
              error_handler_(ec, ActionType::Write);
            }
            return;
          }
          write_buf_deq_.pop_front();
          if (!write_buf_deq_.empty()) {
            write_next();
          }
        });
  }

public:
  Session(tcp::socket sck, MessageFactoryVec factories)
      : sck_(std::move(sck)), analyzer_(std::move(factories)) {}

  void set_message_handler(MessageHandler message_handler) {
    message_handler_ = std::move(message_handler);
  }

  void set_error_handler(ErrorHandler error_handler) {
    error_handler_ = std::move(error_handler);
  }

  tcp::socket& get_socket() { return sck_; }

  void write_message(Message* msg, uint8_t msg_id) {
    bool do_write = write_buf_deq_.empty();

    auto packet = packet_builder_.build_message(msg, msg_id);
    write_buf_deq_.push_back(std::move(packet));

    if (do_write) {
      write_next();
    }
  }

  void run() { read_next(); }
};

using SessionVec = std::vector<Session>;
using SessionMessageHandler = std::function<void(Message const*, Session&)>;
using SessionErrorHandler =
    std::function<void(boost::system::error_code, ActionType, Session&)>;
using ConnectErrorHandler = std::function<void(boost::system::error_code ec)>;

class Server {
  asio::io_context& ioc_;
  tcp::acceptor acceptor_;
  tcp::endpoint endpoint_;

  MessageFactoryVec factories_;

  SessionVec session_vec_;

  SessionMessageHandler session_message_handler_;
  ConnectErrorHandler connect_error_handler_;
  SessionErrorHandler session_error_handler_;

  void accept_next_socket() {
    tcp::socket sck(ioc_);
    acceptor_.async_accept(
        sck,
        [this, sck = std::move(sck)](boost::system::error_code ec) mutable {
          if (ec) {
            if (connect_error_handler_) {
              connect_error_handler_(ec);
            }
          }
          Session session(std::move(sck), factories_);
          session.set_message_handler([this, &session](Message const* msg) {
            session_message_handler_(msg, session);
          });
          session.set_error_handler(
              [this, &session](
                  boost::system::error_code ec, ActionType action_type) {
                if (session_error_handler_) {
                  session_error_handler_(ec, action_type, session);
                }
              });
          session.run();
          session_vec_.push_back(std::move(session));

          accept_next_socket();
        });
  }

public:
  Server(
      boost::asio::io_context& ioc, tcp::endpoint const& endpoint,
      MessageFactoryVec factories)
      : ioc_(ioc),
        endpoint_(std::move(endpoint)),
        acceptor_(ioc),
        factories_(std::move(factories)) {}

  void set_session_message_handler(SessionMessageHandler message_handler) {
    session_message_handler_ = std::move(message_handler);
  }

  void set_connect_error_handler(ConnectErrorHandler connect_error_handler) {
    connect_error_handler_ = std::move(connect_error_handler);
  }

  void set_session_error_handler(SessionErrorHandler session_error_handler) {
    session_error_handler_ = std::move(session_error_handler);
  }

  int run() {
    // Prepare acceptor and start  listening
    acceptor_.open(endpoint_.protocol());
    acceptor_.set_option(tcp::acceptor::reuse_address(true));
    acceptor_.bind(endpoint_);
    acceptor_.listen();

    accept_next_socket();
    PLOG_INFO << "exit VDF service.";
    return 0;
  }
};

class Client {
  tcp::socket sck_;
  std::deque<std::vector<uint8_t>> write_buf_deq_;

  static int const BUF_SIZE = 4096;
  uint8_t read_buf_[BUF_SIZE];

  PacketAnalyzer<MAX_MSG_SIZE> packet_analyzer_;
  PacketBuilder packet_builder_;

  MessageHandler message_handler_;
  ErrorHandler error_handler_;

  void read_next() {
    sck_.async_read_some(
        asio::buffer(read_buf_, BUF_SIZE),
        [this](boost::system::error_code ec, std::size_t bytes) {
          if (ec) {
            // TODO need to stop the peer
            if (error_handler_) {
              error_handler_(ec, ActionType::Read);
            }
          }
          packet_analyzer_.write(read_buf_, bytes);
          while (1) {
            std::unique_ptr<Message> msg(packet_analyzer_.analyze());
            if (msg == nullptr) {
              break;
            }
            if (message_handler_) {
              message_handler_(msg.get());
            }
          }
          read_next();
        });
  }

  void write_next() {
    auto const& data = write_buf_deq_.front();
    asio::async_write(
        sck_, asio::buffer(data),
        [this](boost::system::error_code ec, std::size_t bytes) {
          if (ec) {
            if (error_handler_) {
              error_handler_(ec, ActionType::Write);
            }
          }
          write_buf_deq_.pop_front();
          if (!write_buf_deq_.empty()) {
            write_next();
          }
        });
  }

public:
  Client(asio::io_context& ioc, MessageFactoryVec factories)
      : sck_(ioc), packet_analyzer_(std::move(factories)) {}

  void set_message_handler(MessageHandler message_handler) {
    message_handler_ = std::move(message_handler);
  }

  void set_error_handler(ErrorHandler error_handler) {
    error_handler_ = std::move(error_handler);
  }

  void write_message(Message const* msg, uint8_t msg_id) {
    bool do_write = write_buf_deq_.empty();

    auto packet = packet_builder_.build_message(msg, msg_id);
    write_buf_deq_.push_back(std::move(packet));

    if (do_write) {
      write_next();
    }
  }

  void run() { read_next(); }
};

}  // namespace net
