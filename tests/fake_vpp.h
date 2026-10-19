#ifndef CPPVPP_TESTS_FAKE_VPP_H_
#define CPPVPP_TESTS_FAKE_VPP_H_

#include <sys/socket.h>
#include <unistd.h>

#include <thread>
#include <vector>

#include "vpp/codec.h"

namespace vpp {
namespace testing {

/// In-process fake of the VPP socket API server speaking over a socketpair.
class FakeVpp {
 public:
  FakeVpp() {
    int fds[2];
    socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
    client_fd_ = fds[0];
    server_fd_ = fds[1];
    thread_ = std::thread([this] { Serve(); });
  }
  ~FakeVpp() {
    if (client_fd_ >= 0) ::close(client_fd_);
    thread_.join();
    ::close(server_fd_);
  }

  /// Returns the client end of the socket; ownership passes to the caller.
  int ReleaseClientFd() {
    int fd = client_fd_;
    client_fd_ = -1;
    return fd;
  }

 private:
  bool ReadFull(uint8_t* b, size_t n) {
    size_t got = 0;
    while (got < n) {
      ssize_t r = ::read(server_fd_, b + got, n - got);
      if (r <= 0) return false;
      got += static_cast<size_t>(r);
    }
    return true;
  }

  void Send(const Encoder& msg) {
    Encoder f;
    f.PutU32(0); f.PutU32(0);
    f.PutU32(static_cast<uint32_t>(msg.bytes().size()));
    f.PutU32(0);
    f.PutBytes(msg.bytes());
    ::write(server_fd_, f.bytes().data(), f.bytes().size());
  }

  void Serve() {
    for (;;) {
      uint8_t hdr[16];
      if (!ReadFull(hdr, 16)) return;
      Decoder hd(hdr, 16);
      hd.GetU32();
      hd.GetU32();
      std::vector<uint8_t> msg(hd.GetU32());
      if (!msg.empty() && !ReadFull(msg.data(), msg.size())) return;
      Decoder d(msg);
      uint16_t id = d.GetU16();
      if (id == 15) {
        HandleCreate(d);
        continue;
      }
      if (id == 17) return;
      d.GetU32();  // client index
      uint32_t ctx = d.GetU32();
      Encoder r;
      if (id == 100) {  // control_ping
        r.PutU16(101); r.PutU32(ctx); r.PutI32(0); r.PutU32(7); r.PutU32(4242);
        Send(r);
      } else if (id == 102) {  // show_version
        r.PutU16(103); r.PutU32(ctx); r.PutI32(0);
        r.PutFixedString("vpe", 32); r.PutFixedString("v-fake", 32);
        r.PutFixedString("today", 32); r.PutFixedString("/build", 256);
        Send(r);
      } else if (id == 105) {  // thing_dump
        for (uint32_t i = 1; i <= 2; ++i) {
          Encoder e;
          e.PutU16(104); e.PutU32(ctx); e.PutU32(i);
          Send(e);
        }
      }
    }
  }

  void HandleCreate(Decoder& d) {
    uint32_t ctx = d.GetU32();
    const std::pair<uint16_t, const char*> table[] = {
        {100, "control_ping_51077d14"},
        {101, "control_ping_reply_f6b0b8ca"},
        {102, "show_version_51077d14"},
        {103, "show_version_reply_c919bde1"},
        {104, "thing_details_00000000"},
        {105, "thing_dump_00000000"}};
    Encoder r;
    r.PutU16(16); r.PutU32(7); r.PutU32(ctx); r.PutI32(0); r.PutU32(0);
    r.PutU16(6);
    for (const auto& e : table) {
      r.PutU16(e.first);
      r.PutFixedString(e.second, 64);
    }
    Send(r);
  }

  int client_fd_ = -1;
  int server_fd_ = -1;
  std::thread thread_;
};

}  // namespace testing
}  // namespace vpp

#endif  // CPPVPP_TESTS_FAKE_VPP_H_
