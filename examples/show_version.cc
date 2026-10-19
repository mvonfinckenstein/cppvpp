// Minimal example: connect to VPP's binary API socket and query its version.
#include <cstdlib>
#include <iostream>
#include <memory>

#include "vpp/connection.h"
#include "vpp/vpe.h"

int main(int argc, char** argv) {
  std::string path = argc > 1 ? argv[1] : "/run/vpp/api.sock";
  std::unique_ptr<vpp::Connection> conn;
  vpp::Status s = vpp::Connection::Connect(path, "cppvpp-example", &conn);
  if (!s.ok()) {
    std::cerr << "connect failed: " << s.message() << "\n";
    return EXIT_FAILURE;
  }
  std::cout << "connected; " << conn->MessageCount() << " messages known" << std::endl;

  vpp::PingReply ping;
  s = vpp::ControlPing(conn.get(), &ping);
  if (!s.ok()) {
    std::cerr << "control_ping failed: " << s.message() << "\n";
    return EXIT_FAILURE;
  }
  std::cout << "control_ping: retval=" << ping.retval
            << " vpe_pid=" << ping.vpe_pid << "\n";

  vpp::VersionInfo v;
  s = vpp::ShowVersion(conn.get(), &v);
  if (!s.ok()) {
    std::cerr << "show_version failed: " << s.message() << "\n";
    return EXIT_FAILURE;
  }
  std::cout << "program=" << v.program << " version=" << v.version
            << " build_date=" << v.build_date << "\n";
  return EXIT_SUCCESS;
}
