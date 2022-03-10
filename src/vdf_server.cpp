#include <array>
#include <iostream>
#include <string>
#include <vector>

#include <boost/asio.hpp>
namespace asio = boost::asio;
using namespace asio::ip;

#include <boost/program_options.hpp>
namespace po = boost::program_options;

class Message
{
    int msg_id_ { -1 };

public:
    virtual ~Message() { }

    virtual int get_msg_id() const { return msg_id_; }
};

template <int MAX_SIZE> class PacketAnalyzer
{
    std::array<uint8_t, MAX_SIZE> data_;
    int p_ { 0 };

public:
    int remaining_bytes() const { return p_; }

    int write(uint8_t const* bytes, int size)
    {
        int available = MAX_SIZE - p_;
        int n = std::min<int>(size, available);
        memcpy(data_.data(), bytes, n);
        p_ += n;
        return n;
    }

    std::tuple<Message*, int, bool> analyze() { return std::make_tuple(nullptr, 0, false); }
};

class Session
{
    tcp::socket sck_;

public:
    explicit Session(tcp::socket sck)
        : sck_(std::move(sck))
    {
    }

    tcp::socket& get_socket() { return sck_; }

    void run() { }
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
        acceptor_.async_accept(sck, [this, sck = std::move(sck)](boost::system::error_code ec) mutable {
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
        : ioc_(ioc)
        , acceptor_(ioc, endpoint)
    {
    }

    int run() { return 0; }
};

struct Arguments {
    std::string listening_addr;
    unsigned short listening_port;
};

int main(int argc, const char* argv[])
{
    Arguments args;
    po::options_description desc;
    desc.add_options() // All arguments
        ("help,h", "show help document") // --help
        ("address", po::value(&args.listening_addr)->default_value("127.0.0.1"),
            "vdf will listen to this address") // --address
        ("port", po::value(&args.listening_port)->default_value(9090), "vdf will listen to this port") // --port
        ;

    po::variables_map vars;
    po::store(po::parse_command_line(argc, argv, desc), vars);
    po::notify(vars);

    if (vars.count("help")) {
        std::cout << "Usage:" << std::endl;
        std::cout << desc << std::endl;
        return 0;
    }

    asio::io_context ioc;
    auto addr = address::from_string(args.listening_addr);
    Server srv(ioc, tcp::endpoint(addr, args.listening_port));
    return srv.run();
}
