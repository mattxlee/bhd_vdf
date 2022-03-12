#include <gtest/gtest.h>

#include "vdf_net.hpp"

TEST(VDFNet, SerializeUnserialize) {
  net::PacketBuilder builder;
  Ping ping;
  ping.set_id(1000);
  auto packet = builder.build_message(&ping, net::MSGID_PING);
  EXPECT_TRUE(!packet.empty());

  net::PacketAnalyzer analyzer({new net::MsgFactory_Ping});
  analyzer.write(packet.data(), packet.size());
  net::Message* p;
  std::tie(p, std::ignore) = analyzer.analyze();
  std::unique_ptr<net::Message> msg(p);
  EXPECT_TRUE(msg != nullptr);
}

TEST(VDFNet, PingPong) {
  // Run server
  asio::io_context ioc;
  auto addr = asio::ip::address::from_string("127.0.0.1");
  uint16_t port = 9090;
  tcp::endpoint endpoint(addr, port);
  net::Server srv(ioc, endpoint, {new net::MsgFactory_Ping});
  srv.run();
  ioc.run();
}
