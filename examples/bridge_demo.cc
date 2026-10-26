// Creates two loopback interfaces, brings them up, puts them into a new L2
// bridge domain (one as the BVI with an IPv4 address), prints the resulting
// state, and then removes everything again.
#include <iostream>

#include "example_util.h"
#include "vpp/api/interface.api.h"
#include "vpp/api/ip.api.h"
#include "vpp/api/l2.api.h"
#include "vpp/rpc.h"

using namespace vpp::api;  // NOLINT: example brevity.

namespace {

uint32_t NewLoopback(vpp::Connection* c) {
  CreateLoopback req;
  CreateLoopbackReply reply;
  example::Check(vpp::Invoke(c, req, &reply), "create_loopback");
  example::CheckRetval(reply.retval, "create_loopback");
  return reply.sw_if_index;
}

void SetUp(vpp::Connection* c, uint32_t sw_if_index) {
  SwInterfaceSetFlags req;
  req.sw_if_index = sw_if_index;
  req.flags = IfStatusFlags::IF_STATUS_API_FLAG_ADMIN_UP;
  SwInterfaceSetFlagsReply reply;
  example::Check(vpp::Invoke(c, req, &reply), "sw_interface_set_flags");
  example::CheckRetval(reply.retval, "sw_interface_set_flags");
}

void JoinBridge(vpp::Connection* c, uint32_t sw_if_index, uint32_t bd_id,
                L2PortType type) {
  SwInterfaceSetL2Bridge req;
  req.rx_sw_if_index = sw_if_index;
  req.bd_id = bd_id;
  req.port_type = type;
  req.enable = true;
  SwInterfaceSetL2BridgeReply reply;
  example::Check(vpp::Invoke(c, req, &reply), "sw_interface_set_l2_bridge");
  example::CheckRetval(reply.retval, "sw_interface_set_l2_bridge");
}

}  // namespace

int main(int argc, char** argv) {
  auto conn = example::ConnectOrDie(argc, argv);
  vpp::Connection* c = conn.get();
  const uint32_t kBridgeId = 4242;

  BridgeDomainAddDelV2 bd;
  bd.bd_id = kBridgeId;
  bd.flood = bd.uu_flood = bd.forward = bd.learn = true;
  bd.bd_tag = "cppvpp-demo";
  bd.is_add = true;
  BridgeDomainAddDelV2Reply bd_reply;
  example::Check(vpp::Invoke(c, bd, &bd_reply), "bridge_domain_add_del_v2");
  example::CheckRetval(bd_reply.retval, "bridge_domain_add_del_v2");
  std::cout << "created bridge domain " << kBridgeId << "\n";

  uint32_t bvi = NewLoopback(c);
  uint32_t member = NewLoopback(c);
  std::cout << "created loopbacks " << bvi << " (bvi) and " << member << "\n";
  SetUp(c, bvi);
  SetUp(c, member);
  JoinBridge(c, bvi, kBridgeId, L2PortType::L2_API_PORT_TYPE_BVI);
  JoinBridge(c, member, kBridgeId, L2PortType::L2_API_PORT_TYPE_NORMAL);

  SwInterfaceAddDelAddress addr;
  addr.sw_if_index = bvi;
  addr.is_add = true;
  addr.prefix = example::MakeIp4Prefix(10, 99, 0, 1, 24);
  SwInterfaceAddDelAddressReply addr_reply;
  example::Check(vpp::Invoke(c, addr, &addr_reply), "sw_interface_add_del_address");
  example::CheckRetval(addr_reply.retval, "sw_interface_add_del_address");

  BridgeDomainDump dump;
  dump.bd_id = kBridgeId;
  dump.sw_if_index = 0xffffffff;
  std::vector<BridgeDomainDetails> bds;
  example::Check(vpp::InvokeDump(c, dump, &bds), "bridge_domain_dump");
  for (const auto& d : bds) {
    std::cout << "bridge " << d.bd_id << " tag=" << d.bd_tag
              << " bvi=" << d.bvi_sw_if_index << " members:";
    for (const auto& m : d.sw_if_details) std::cout << " " << m.sw_if_index;
    std::cout << "\n";
  }

  IpAddressDump adump;
  adump.sw_if_index = bvi;
  std::vector<IpAddressDetails> addrs;
  example::Check(vpp::InvokeDump(c, adump, &addrs), "ip_address_dump");
  for (const auto& a : addrs) {
    std::cout << "interface " << a.sw_if_index << " has "
              << example::FormatPrefix(a.prefix) << "\n";
  }

  // Cleanup: leave the bridge, then delete loopbacks and the bridge domain.
  for (uint32_t i : {bvi, member}) {
    SwInterfaceSetL2Bridge leave;
    leave.rx_sw_if_index = i;
    leave.bd_id = kBridgeId;
    leave.enable = false;
    SwInterfaceSetL2BridgeReply lr;
    example::Check(vpp::Invoke(c, leave, &lr), "leave bridge");
    DeleteLoopback del;
    del.sw_if_index = i;
    DeleteLoopbackReply dr;
    example::Check(vpp::Invoke(c, del, &dr), "delete_loopback");
  }
  bd.is_add = false;
  example::Check(vpp::Invoke(c, bd, &bd_reply), "bridge_domain_add_del_v2 (del)");
  std::cout << "cleaned up\n";
  return EXIT_SUCCESS;
}
