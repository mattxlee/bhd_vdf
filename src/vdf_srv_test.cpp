#include <thread>

#include <gtest/gtest.h>
#include <plog/Log.h>

#include "vdf_net.hpp"

plog::ConsoleAppender<plog::TxtFormatter> console_appender;

TEST(VDFNet, InitialLog) { plog::init(plog::Severity::info, &console_appender); }

TEST(VDFNet, SerializeUnserialize) {
    net::PacketBuilder builder;
    Ping ping;
    ping.set_id(1000);
    auto packet = builder.build_message(&ping, net::MSGID_PING);
    EXPECT_TRUE(!packet.empty());

    net::PacketAnalyzer analyzer({std::make_shared<net::MsgFactory_Ping>()});
    analyzer.write(packet.data(), packet.size());
    net::Message* p;
    std::tie(p, std::ignore) = analyzer.analyze();
    std::unique_ptr<net::Message> msg(p);
    EXPECT_TRUE(msg != nullptr);
}

char const* SZ_LOCAL_HOST = "127.0.0.1";
uint16_t TEST_PORT = 9090;
int PING_VAL = 10102;

class ServerTest : public ::testing::Test {
    asio::io_context ioc_;
    std::thread th_;

protected:
    std::unique_ptr<net::Server> srv;

    void SetUp() override {
        srv.reset(new net::Server(
            ioc_, tcp::endpoint(asio::ip::address::from_string(SZ_LOCAL_HOST), TEST_PORT),
            {std::make_shared<net::MsgFactory_Ping>(), std::make_shared<net::MsgFactory_RequestVDF>()}));
        srv->run();
        // Run the server in dividual thread
        th_ = std::thread([this]() { ioc_.run(); });
        PLOG_INFO << "server is running";
    }

    void TearDown() override {
        PLOG_INFO << "prepare to exit";
        srv->stop();
        th_.join();
        PLOG_INFO << "thread is closed";
    }
};

TEST_F(ServerTest, PingPong) {
    srv->set_session_message_handler([](net::Message const* msg, uint8_t msg_id, net::SessionPtr session) {
        PLOG_INFO << "received new message from server, msg_id=" << msg_id;
        if (msg_id == net::MSGID_PING) {
            auto ping = static_cast<Pong const*>(msg);
            Pong pong;
            pong.set_reply_id(ping->reply_id());
            session->write_message(&pong, net::MSGID_PONG);
            PLOG_INFO << "pong has been wrote from server";
        }
    });
    PLOG_INFO << "starting the client";
    // Need to start a client
    asio::io_context ioc;
    net::Client client(ioc, {std::make_shared<net::MsgFactory_Pong>()});
    client.set_message_handler([&client](net::Message const* msg, uint8_t msg_id) {
        EXPECT_EQ(msg_id, net::MSGID_PONG);
        if (msg_id == net::MSGID_PONG) {
            auto pong = static_cast<Pong const*>(msg);
            EXPECT_EQ(pong->reply_id(), PING_VAL);
            // Now we close the peer
            PLOG_INFO << "trying to close client since the pong has been received";
            client.close();
        }
    });
    client.set_connect_handler([&client](bool succ) {
        PLOG_INFO << "connected to server, succ=" << succ;
        EXPECT_TRUE(succ);
        Ping ping;
        ping.set_id(PING_VAL);
        client.write_message(&ping, net::MSGID_PING);
        PLOG_INFO << "ping has been wrote";
    });
    client.set_error_handler([](boost::system::error_code ec, net::ActionType type) { PLOG_ERROR << ec.message(); });
    PLOG_INFO << "establishing a new connection to server";
    client.connect(tcp::endpoint(asio::ip::address::from_string(SZ_LOCAL_HOST), TEST_PORT));
    ioc.run();
    PLOG_INFO << "client ioc is exited";
}
