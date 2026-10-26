// Lists all interfaces with their state, MAC, MTU and IP addresses using the
// sw_interface_dump and ip_address_dump streaming APIs.
#include <iostream>

#include "example_util.h"
#include "vpp/api/interface.api.h"
#include "vpp/api/ip.api.h"
#include "vpp/rpc.h"

int main(int argc, char** argv) {
  auto conn = example::ConnectOrDie(argc, argv);

  vpp::api::SwInterfaceDump req;
  req.sw_if_index = 0xffffffff;  // All interfaces.
  std::vector<vpp::api::SwInterfaceDetails> ifaces;
  example::Check(vpp::InvokeDump(conn.get(), req, &ifaces), "sw_interface_dump");

  for (const auto& i : ifaces) {
    bool up = static_cast<uint32_t>(i.flags) &
              static_cast<uint32_t>(vpp::api::IfStatusFlags::IF_STATUS_API_FLAG_ADMIN_UP);
    std::cout << i.sw_if_index << ": " << i.interface_name
              << " mac=" << example::FormatMac(i.l2_address)
              << " mtu=" << i.mtu[0] << " admin=" << (up ? "up" : "down")
              << "\n";
    for (bool v6 : {false, true}) {
      vpp::api::IpAddressDump areq;
      areq.sw_if_index = i.sw_if_index;
      areq.is_ipv6 = v6;
      std::vector<vpp::api::IpAddressDetails> addrs;
      example::Check(vpp::InvokeDump(conn.get(), areq, &addrs), "ip_address_dump");
      for (const auto& a : addrs) {
        std::cout << "    " << example::FormatPrefix(a.prefix) << "\n";
      }
    }
  }
  return EXIT_SUCCESS;
}
