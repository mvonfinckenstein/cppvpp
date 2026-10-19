#ifndef CPPVPP_VPP_CONNECTION_H_
#define CPPVPP_VPP_CONNECTION_H_

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "vpp/status.h"

namespace vpp {

/// A synchronous client for the VPP binary API over its unix socket
/// (the `socksvr` transport, e.g. /run/vpp/api.sock).
///
/// Messages are identified by "<name>_<crc>" (for example
/// "show_version_51077d14"); VPP assigns numeric ids per connection and this
/// class resolves them from the message table received on connect.
/// All public methods are thread-safe; calls are serialized.
class Connection {
 public:
  ~Connection();
  Connection(const Connection&) = delete;
  Connection& operator=(const Connection&) = delete;

  /// Connects to the VPP API socket at @p socket_path and registers as
  /// @p client_name. On success stores the connection in @p out.
  static Status Connect(const std::string& socket_path,
                        const std::string& client_name,
                        std::unique_ptr<Connection>* out);

  /// Wraps an already connected socket fd and performs the VPP handshake.
  /// Takes ownership of @p fd (closed on failure too).
  static Status FromFd(int fd, const std::string& client_name,
                       std::unique_ptr<Connection>* out);

  /// Sends a request and waits for its reply.
  ///
  /// @param request_name  Full request name including CRC suffix.
  /// @param request_body  Encoded fields following the 10 byte request header
  ///                      (message id, client index, context), which is added
  ///                      automatically.
  /// @param reply_name    Full reply name including CRC suffix.
  /// @param reply_body    Receives reply fields after the 6 byte reply header
  ///                      (message id, context).
  Status Call(const std::string& request_name,
              const std::vector<uint8_t>& request_body,
              const std::string& reply_name,
              std::vector<uint8_t>* reply_body);

  /// Sends a dump-style request followed by a control ping and collects all
  /// @p details_name messages (bodies after the 6 byte header) until the ping
  /// reply arrives.
  Status Dump(const std::string& request_name,
              const std::vector<uint8_t>& request_body,
              const std::string& details_name,
              std::vector<std::vector<uint8_t>>* details);

  /// Returns the id VPP assigned to @p full_name, or -1 if unknown.
  int MessageId(const std::string& full_name) const;

  /// Returns the number of messages in the negotiated message table.
  size_t MessageCount() const { return ids_.size(); }

  /// Closes the connection (sends sockclnt_delete best-effort).
  void Close();

 private:
  explicit Connection(int fd) : fd_(fd) {}

  Status Handshake(const std::string& client_name);
  Status Send(const std::vector<uint8_t>& msg);
  Status Receive(std::vector<uint8_t>* msg);
  Status ReadFull(uint8_t* buf, size_t n);
  Status SendRequest(const std::string& name, const std::vector<uint8_t>& body,
                     uint32_t context);

  int fd_;
  uint32_t client_index_ = 0;
  uint32_t next_context_ = 1;
  std::map<std::string, uint16_t> ids_;
  std::map<uint16_t, std::string> names_;
  std::mutex mu_;
};

}  // namespace vpp

#endif  // CPPVPP_VPP_CONNECTION_H_
