#include "vpp/vpe.h"

#include <gtest/gtest.h>

#include <cstdlib>

#include "fake_vpp.h"

namespace vpp {
namespace {

TEST(VpeTest, ControlPing) {
  testing::FakeVpp fake;
  std::unique_ptr<Connection> conn;
  ASSERT_TRUE(Connection::FromFd(fake.ReleaseClientFd(), "t", &conn).ok());
  PingReply r;
  ASSERT_TRUE(ControlPing(conn.get(), &r).ok());
  EXPECT_EQ(r.retval, 0);
  EXPECT_EQ(r.client_index, 7u);
  EXPECT_EQ(r.vpe_pid, 4242u);
}

TEST(VpeTest, ShowVersion) {
  testing::FakeVpp fake;
  std::unique_ptr<Connection> conn;
  ASSERT_TRUE(Connection::FromFd(fake.ReleaseClientFd(), "t", &conn).ok());
  VersionInfo v;
  ASSERT_TRUE(ShowVersion(conn.get(), &v).ok());
  EXPECT_EQ(v.program, "vpe");
  EXPECT_EQ(v.version, "v-fake");
  EXPECT_EQ(v.build_directory, "/build");
}

// Runs only when VPP_API_SOCKET points at a live VPP socket.
TEST(VpeIntegrationTest, RealShowVersion) {
  const char* path = std::getenv("VPP_API_SOCKET");
  if (path == nullptr) GTEST_SKIP() << "VPP_API_SOCKET not set";
  std::unique_ptr<Connection> conn;
  ASSERT_TRUE(Connection::Connect(path, "cppvpp-test", &conn).ok());
  VersionInfo v;
  Status s = ShowVersion(conn.get(), &v);
  ASSERT_TRUE(s.ok()) << s.message();
  EXPECT_EQ(v.program, "vpe");
  EXPECT_FALSE(v.version.empty());
}

}  // namespace
}  // namespace vpp
