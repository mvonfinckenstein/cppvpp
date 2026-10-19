#include "vpp/connection.h"

#include <gtest/gtest.h>

#include <cstdlib>

#include "fake_vpp.h"
#include "vpp/codec.h"

namespace vpp {
namespace {

TEST(ConnectionTest, HandshakeLoadsMessageTable) {
  testing::FakeVpp fake;
  std::unique_ptr<Connection> conn;
  ASSERT_TRUE(Connection::FromFd(fake.ReleaseClientFd(), "test", &conn).ok());
  EXPECT_EQ(conn->MessageCount(), 6u);
  EXPECT_EQ(conn->MessageId("control_ping_51077d14"), 100);
  EXPECT_EQ(conn->MessageId("nope"), -1);
}

TEST(ConnectionTest, UnknownMessageFails) {
  testing::FakeVpp fake;
  std::unique_ptr<Connection> conn;
  ASSERT_TRUE(Connection::FromFd(fake.ReleaseClientFd(), "test", &conn).ok());
  std::vector<uint8_t> reply;
  EXPECT_FALSE(conn->Call("nope", {}, "control_ping_reply_f6b0b8ca", &reply).ok());
}

TEST(ConnectionTest, DumpCollectsDetails) {
  testing::FakeVpp fake;
  std::unique_ptr<Connection> conn;
  ASSERT_TRUE(Connection::FromFd(fake.ReleaseClientFd(), "test", &conn).ok());
  std::vector<std::vector<uint8_t>> details;
  ASSERT_TRUE(
      conn->Dump("thing_dump_00000000", {}, "thing_details_00000000", &details)
          .ok());
  ASSERT_EQ(details.size(), 2u);
  EXPECT_EQ(Decoder(details[1]).GetU32(), 2u);
}

TEST(ConnectionTest, ConnectToMissingSocketFails) {
  std::unique_ptr<Connection> conn;
  EXPECT_FALSE(Connection::Connect("/nonexistent/api.sock", "t", &conn).ok());
}

TEST(ConnectionTest, CloseThenCallFails) {
  testing::FakeVpp fake;
  std::unique_ptr<Connection> conn;
  ASSERT_TRUE(Connection::FromFd(fake.ReleaseClientFd(), "test", &conn).ok());
  conn->Close();
  std::vector<uint8_t> reply;
  EXPECT_FALSE(conn->Call("control_ping_51077d14", {},
                          "control_ping_reply_f6b0b8ca", &reply).ok());
}

// Runs only when VPP_API_SOCKET points at a live VPP socket.
TEST(ConnectionIntegrationTest, RealVpp) {
  const char* path = std::getenv("VPP_API_SOCKET");
  if (path == nullptr) GTEST_SKIP() << "VPP_API_SOCKET not set";
  std::unique_ptr<Connection> conn;
  Status s = Connection::Connect(path, "cppvpp-test", &conn);
  ASSERT_TRUE(s.ok()) << s.message();
  EXPECT_GT(conn->MessageCount(), 0u);
}

}  // namespace
}  // namespace vpp
