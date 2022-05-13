#include <thread>

#include <gtest/gtest.h>
#include <plog/Log.h>

#include "vdf_net.hpp"

#include "vdf_types.h"
#include "vdf_utils.h"
#include "vdf_computer.h"
#include "vdf_computer_thread.hpp"

plog::ConsoleAppender<plog::TxtFormatter> console_appender;

TEST(VDFNet, InitialLog) { plog::init(plog::Severity::info, &console_appender); }

TEST(VDFNet, SerializeUnserialize) {
    net::PacketBuilder builder;
    Ping ping;
    ping.set_id(1000);
    auto packet = builder.build_message(&ping, net::MSGID_PING);
    EXPECT_TRUE(!packet.empty());

    net::PacketAnalyzer<> analyzer({std::make_shared<net::MsgFactory_Ping>()});
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

TEST_F(ServerTest, VdfCalculation) {
    srv->set_session_message_handler([](net::Message const* pmsg, uint8_t msg_id, net::SessionPtr psession) {
        PLOG_INFO << "received new message from server, msg_id=" << msg_id;
        if (msg_id == net::MSGID_REQUESTVDF) {
            // start new Vdf computer
            auto pmsg_request_vdf = static_cast<RequestVDF const*>(pmsg);
            Bytes infusion = to_bytes(pmsg_request_vdf->infusion());
            Bytes x = to_bytes(pmsg_request_vdf->x());
            uint64_t iters = pmsg_request_vdf->iters();
            auto pthread = std::make_unique<VDFComputerThread>(std::move(infusion), std::move(x), iters,
                    [](std::optional<VDFComputerThread::Result> result){});
        }
    });
    PLOG_INFO << "starting the client";
    asio::io_context ioc;
    net::Client client(
        ioc, {std::make_shared<net::MsgFactory_RequestVDFReply>(), std::make_shared<net::MsgFactory_VDFResult>()});
    client.set_message_handler([&client](net::Message const* pmsg, uint8_t msg_id) {
        if (msg_id == net::MSGID_VDFRESULT) {
            auto pmsg_result = static_cast<VDFResult const*>(pmsg);
            Bytes infusion = to_bytes(pmsg_result->infusion());
            auto D = vdf::utils::CreateDiscriminant(infusion);
            Bytes x = to_bytes(pmsg_result->x());
            uint64_t iters = pmsg_result->iters();
            double duration = pmsg_result->duration();
            vdf::types::Proof proof_s;
            proof_s.y = to_bytes(pmsg_result->y());
            proof_s.proof = to_bytes(pmsg_result->proof());
            Bytes proof = vdf::utils::SerializeProof(proof_s);
            EXPECT_TRUE(vdf::utils::VerifyProof(D, proof, iters, 0, x));
            // Now we close the peer
            PLOG_INFO << "Vdf result has been received and verified";
            client.close();
        }
    });
    client.set_connect_handler([&client](bool succ) {
        PLOG_INFO << "connected to server, succ=" << succ;
        EXPECT_TRUE(succ);
        // send message to request new Vdf computing
        auto req = std::make_unique<RequestVDF>();
        Bytes infusion(32, '\0'), x(32, '\0');
        req->set_infusion(to_string(infusion));
        req->set_x(to_string(x));
        req->set_iters(10000);
        client.write_message(req.get(), net::MSGID_REQUESTVDF);
        PLOG_INFO << "Vdf is required";
    });
    client.set_error_handler([](boost::system::error_code ec, net::ActionType type) { PLOG_ERROR << ec.message(); });
    PLOG_INFO << "establishing a new connection to server";
    client.connect(tcp::endpoint(asio::ip::address::from_string(SZ_LOCAL_HOST), TEST_PORT));
    ioc.run();
    PLOG_INFO << "client ioc is exited";
}
