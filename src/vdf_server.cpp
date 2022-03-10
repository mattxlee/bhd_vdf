#include <iostream>
#include <string>

#include <boost/asio.hpp>
namespace asio = boost::asio;
using namespace asio::ip;

#include <boost/program_options.hpp>
namespace po = boost::program_options;

class Server
{
    tcp::acceptor acceptor_;

public:
    Server(boost::asio::io_service& ioc, tcp::endpoint const& endpoint)
        : acceptor_(ioc, endpoint)
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

    asio::io_service ioc;
    auto addr = address::from_string(args.listening_addr);
    Server srv(ioc, tcp::endpoint(addr, args.listening_port));
    return srv.run();
}
