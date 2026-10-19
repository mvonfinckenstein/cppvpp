#include "vpp/connection.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

#include "vpp/codec.h"

namespace vpp {

namespace {

// Socket framing: [u64 unused (queue ptr)][u32 length (BE)][u32 unused (gc mark)] followed by the message.
constexpr size_t kFrameHeaderLen = 16;
constexpr uint16_t kSockclntCreate = 15;
constexpr uint16_t kSockclntCreateReply = 16;
constexpr uint16_t kSockclntDelete = 17;
constexpr size_t kMaxMessageLen = 16 * 1024 * 1024;

constexpr char kControlPing[] = "control_ping_51077d14";
constexpr char kControlPingReply[] = "control_ping_reply_f6b0b8ca";

}  // namespace

Connection::~Connection() { Close(); }

Status Connection::Connect(const std::string& socket_path,
                           const std::string& client_name,
                           std::unique_ptr<Connection>* out) {
  sockaddr_un addr;
  std::memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  if (socket_path.size() >= sizeof(addr.sun_path)) {
    return Status::Error("socket path too long: " + socket_path);
  }
  std::memcpy(addr.sun_path, socket_path.c_str(), socket_path.size() + 1);
  int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) return Status::Error(std::string("socket: ") + strerror(errno));
  if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    std::string err = std::string("connect ") + socket_path + ": " +
                      strerror(errno);
    ::close(fd);
    return Status::Error(err);
  }
  return FromFd(fd, client_name, out);
}

Status Connection::FromFd(int fd, const std::string& client_name,
                          std::unique_ptr<Connection>* out) {
  std::unique_ptr<Connection> conn(new Connection(fd));
  Status s = conn->Handshake(client_name);
  if (!s.ok()) return s;
  *out = std::move(conn);
  return Status();
}

Status Connection::Handshake(const std::string& client_name) {
  Encoder e;
  e.PutU16(kSockclntCreate);
  e.PutU32(0);  // context
  e.PutFixedString(client_name, 64);
  Status s = Send(e.bytes());
  if (!s.ok()) return s;

  std::vector<uint8_t> msg;
  s = Receive(&msg);
  if (!s.ok()) return s;
  Decoder d(msg);
  if (d.GetU16() != kSockclntCreateReply) {
    return Status::Error("unexpected reply to sockclnt_create");
  }
  d.GetU32();  // client_index (VPP internal handle)
  d.GetU32();  // context
  int32_t response = d.GetI32();
  client_index_ = d.GetU32();  // "index": the id to use in requests
  uint16_t count = d.GetU16();
  if (!d.ok()) return Status::Error("truncated sockclnt_create_reply");
  if (response != 0) {
    return Status::Error("sockclnt_create failed, response=" +
                         std::to_string(response));
  }
  for (uint16_t i = 0; i < count; ++i) {
    uint16_t id = d.GetU16();
    std::string name = d.GetFixedString(64);
    if (!d.ok()) return Status::Error("truncated message table");
    ids_[name] = id;
    names_[id] = name;
  }
  return Status();
}

void Connection::Close() {
  std::lock_guard<std::mutex> lock(mu_);
  if (fd_ < 0) return;
  Encoder e;
  e.PutU16(kSockclntDelete);
  e.PutU32(client_index_);
  e.PutU32(0);
  Send(e.bytes());  // Best effort.
  ::close(fd_);
  fd_ = -1;
}

int Connection::MessageId(const std::string& full_name) const {
  auto it = ids_.find(full_name);
  return it == ids_.end() ? -1 : it->second;
}

Status Connection::ReadFull(uint8_t* buf, size_t n) {
  size_t got = 0;
  while (got < n) {
    ssize_t r = ::read(fd_, buf + got, n - got);
    if (r < 0 && errno == EINTR) continue;
    if (r < 0) return Status::Error(std::string("read: ") + strerror(errno));
    if (r == 0) return Status::Error("connection closed by peer");
    got += static_cast<size_t>(r);
  }
  return Status();
}

Status Connection::Send(const std::vector<uint8_t>& msg) {
  if (fd_ < 0) return Status::Error("connection closed");
  Encoder frame;
  frame.PutU32(0);
  frame.PutU32(0);
  frame.PutU32(static_cast<uint32_t>(msg.size()));
  frame.PutU32(0);
  frame.PutBytes(msg);
  const std::vector<uint8_t>& buf = frame.bytes();
  size_t sent = 0;
  while (sent < buf.size()) {
    ssize_t w = ::send(fd_, buf.data() + sent, buf.size() - sent, 0);
    if (w < 0 && errno == EINTR) continue;
    if (w < 0) return Status::Error(std::string("send: ") + strerror(errno));
    sent += static_cast<size_t>(w);
  }
  return Status();
}

Status Connection::Receive(std::vector<uint8_t>* msg) {
  if (fd_ < 0) return Status::Error("connection closed");
  uint8_t header[kFrameHeaderLen];
  Status s = ReadFull(header, sizeof(header));
  if (!s.ok()) return s;
  Decoder d(header, sizeof(header));
  d.GetU32();
  d.GetU32();
  uint32_t len = d.GetU32();
  if (len > kMaxMessageLen) return Status::Error("oversized message");
  msg->assign(len, 0);
  return len == 0 ? Status() : ReadFull(msg->data(), len);
}

Status Connection::SendRequest(const std::string& name,
                               const std::vector<uint8_t>& body,
                               uint32_t context) {
  int id = MessageId(name);
  if (id < 0) return Status::Error("VPP does not know message " + name);
  Encoder e;
  e.PutU16(static_cast<uint16_t>(id));
  e.PutU32(client_index_);
  e.PutU32(context);
  e.PutBytes(body);
  return Send(e.bytes());
}

Status Connection::Call(const std::string& request_name,
                        const std::vector<uint8_t>& request_body,
                        const std::string& reply_name,
                        std::vector<uint8_t>* reply_body) {
  std::lock_guard<std::mutex> lock(mu_);
  int reply_id = MessageId(reply_name);
  if (reply_id < 0) return Status::Error("VPP does not know message " + reply_name);
  uint32_t context = next_context_++;
  Status s = SendRequest(request_name, request_body, context);
  if (!s.ok()) return s;
  for (;;) {
    std::vector<uint8_t> msg;
    s = Receive(&msg);
    if (!s.ok()) return s;
    Decoder d(msg);
    uint16_t id = d.GetU16();
    uint32_t ctx = d.GetU32();
    if (!d.ok()) return Status::Error("truncated message");
    if (id != reply_id || ctx != context) continue;  // Stale or unrelated.
    reply_body->assign(msg.begin() + 6, msg.end());
    return Status();
  }
}

Status Connection::Dump(const std::string& request_name,
                        const std::vector<uint8_t>& request_body,
                        const std::string& details_name,
                        std::vector<std::vector<uint8_t>>* details) {
  std::lock_guard<std::mutex> lock(mu_);
  int details_id = MessageId(details_name);
  int ping_reply_id = MessageId(kControlPingReply);
  if (details_id < 0) return Status::Error("VPP does not know message " + details_name);
  if (ping_reply_id < 0) return Status::Error("VPP does not know control_ping_reply");
  uint32_t context = next_context_++;
  Status s = SendRequest(request_name, request_body, context);
  if (!s.ok()) return s;
  s = SendRequest(kControlPing, {}, context);
  if (!s.ok()) return s;
  for (;;) {
    std::vector<uint8_t> msg;
    s = Receive(&msg);
    if (!s.ok()) return s;
    Decoder d(msg);
    uint16_t id = d.GetU16();
    uint32_t ctx = d.GetU32();
    if (!d.ok()) return Status::Error("truncated message");
    if (ctx != context) continue;
    if (id == ping_reply_id) return Status();
    if (id == details_id) details->emplace_back(msg.begin() + 6, msg.end());
  }
}

}  // namespace vpp
