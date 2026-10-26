// End-to-end test of loopback/bridge/address APIs against a live VPP. Runs only
// when VPP_API_SOCKET points at a VPP binary API socket.
#include <gtest/gtest.h>

#include <cstdlib>

#include "vpp/api/interface.api.h"
#include "vpp/api/ip.api.h"
#include "vpp/api/l2.api.h"
#include "vpp/connection.h"
#include "vpp/rpc.h"

namespace vpp {
namespace {

using namespace vpp::api;  // NOLINT

TEST(LiveVppTest, LoopbackBridgeAndAddress) {
  const char* path = std::getenv("VPP_API_SOCKET");
  if (path == nullptr) GTEST_SKIP() << "VPP_API_SOCKET not set";
  std::unique_ptr<Connection> conn;
  ASSERT_TRUE(Connection::Connect(path, "cppvpp-itest", &conn).ok());

  CreateLoopbackReply lo;
  ASSERT_TRUE(Invoke(conn.get(), CreateLoopback(), &lo).ok());
  ASSERT_EQ(lo.retval, 0);

  BridgeDomainAddDelV2 bd;
  bd.bd_id = 777;
  bd.flood = bd.uu_flood = bd.forward = bd.learn = true;
  bd.is_add = true;
  BridgeDomainAddDelV2Reply bdr;
  ASSERT_TRUE(Invoke(conn.get(), bd, &bdr).ok());
  ASSERT_EQ(bdr.retval, 0);

  SwInterfaceSetL2Bridge join;
  join.rx_sw_if_index = lo.sw_if_index;
  join.bd_id = 777;
  join.enable = true;
  SwInterfaceSetL2BridgeReply jr;
  ASSERT_TRUE(Invoke(conn.get(), join, &jr).ok());
  EXPECT_EQ(jr.retval, 0);

  std::vector<BridgeDomainDetails> bds;
  BridgeDomainDump dump;
  dump.bd_id = 777;
  dump.sw_if_index = 0xffffffff;
  ASSERT_TRUE(InvokeDump(conn.get(), dump, &bds).ok());
  ASSERT_EQ(bds.size(), 1u);
  ASSERT_EQ(bds[0].sw_if_details.size(), 1u);
  EXPECT_EQ(bds[0].sw_if_details[0].sw_if_index, lo.sw_if_index);

  std::vector<SwInterfaceDetails> ifaces;
  SwInterfaceDump idump;
  idump.sw_if_index = 0xffffffff;
  ASSERT_TRUE(InvokeDump(conn.get(), idump, &ifaces).ok());
  bool found = false;
  for (const auto& i : ifaces) found |= i.sw_if_index == lo.sw_if_index;
  EXPECT_TRUE(found);

  join.enable = false;
  ASSERT_TRUE(Invoke(conn.get(), join, &jr).ok());
  DeleteLoopback del;
  del.sw_if_index = lo.sw_if_index;
  DeleteLoopbackReply dr;
  ASSERT_TRUE(Invoke(conn.get(), del, &dr).ok());
  bd.is_add = false;
  ASSERT_TRUE(Invoke(conn.get(), bd, &bdr).ok());
}

}  // namespace
}  // namespace vpp
