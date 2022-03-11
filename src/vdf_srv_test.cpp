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
  net::Message* msg = analyzer.analyze();
  EXPECT_TRUE(msg != nullptr);
  if (msg) {
    delete msg;
  }
}
