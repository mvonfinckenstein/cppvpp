#ifndef CPPVPP_EXAMPLES_EXAMPLE_UTIL_H_
#define CPPVPP_EXAMPLES_EXAMPLE_UTIL_H_

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "vpp/api/ethernet_types.api.h"
#include "vpp/api/ip_types.api.h"
#include "vpp/connection.h"

/// Helpers shared by the examples.
namespace example {

/// Connects to the socket given as argv[1] (default /run/vpp/api.sock) or
/// exits with an error message.
inline std::unique_ptr<vpp::Connection> ConnectOrDie(int argc, char** argv) {
  std::string path = argc > 1 ? argv[1] : "/run/vpp/api.sock";
  std::unique_ptr<vpp::Connection> conn;
  vpp::Status s = vpp::Connection::Connect(path, "cppvpp-example", &conn);
  if (!s.ok()) {
    std::cerr << "connect failed: " << s.message() << "\n";
    std::exit(EXIT_FAILURE);
  }
  return conn;
}

/// Exits with @p what and the status message if @p s is an error.
inline void Check(const vpp::Status& s, const char* what) {
  if (!s.ok()) {
    std::cerr << what << " failed: " << s.message() << "\n";
    std::exit(EXIT_FAILURE);
  }
}

/// Exits if the VPP return code @p retval is non-zero.
inline void CheckRetval(int32_t retval, const char* what) {
  if (retval != 0) {
    std::cerr << what << " failed: retval=" << retval << "\n";
    std::exit(EXIT_FAILURE);
  }
}

/// Formats a MAC address as aa:bb:cc:dd:ee:ff.
inline std::string FormatMac(const vpp::api::MacAddress& mac) {
  char buf[18];
  std::snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x", mac[0],
                mac[1], mac[2], mac[3], mac[4], mac[5]);
  return buf;
}

/// Formats an address prefix as "a.b.c.d/len" or "x:x::x/len".
inline std::string FormatPrefix(const vpp::api::Prefix& p) {
  std::ostringstream os;
  if (p.address.af == vpp::api::AddressFamily::ADDRESS_IP4) {
    for (int i = 0; i < 4; ++i) os << (i ? "." : "") << int(p.address.un.raw[i]);
  } else {
    char buf[8];
    for (int i = 0; i < 16; i += 2) {
      std::snprintf(buf, sizeof(buf), "%x",
                    (p.address.un.raw[i] << 8) | p.address.un.raw[i + 1]);
      os << (i ? ":" : "") << buf;
    }
  }
  os << "/" << int(p.len);
  return os.str();
}

/// Builds an IPv4 prefix from four octets and a length.
inline vpp::api::Prefix MakeIp4Prefix(uint8_t a, uint8_t b, uint8_t c,
                                      uint8_t d, uint8_t len) {
  vpp::api::Prefix p;
  p.address.af = vpp::api::AddressFamily::ADDRESS_IP4;
  p.address.un.raw[0] = a;
  p.address.un.raw[1] = b;
  p.address.un.raw[2] = c;
  p.address.un.raw[3] = d;
  p.len = len;
  return p;
}

}  // namespace example

#endif  // CPPVPP_EXAMPLES_EXAMPLE_UTIL_H_
