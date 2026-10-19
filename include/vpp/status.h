#ifndef CPPVPP_VPP_STATUS_H_
#define CPPVPP_VPP_STATUS_H_

#include <string>
#include <utility>

namespace vpp {

/// Lightweight error type used instead of exceptions.
class Status {
 public:
  /// Creates an OK status.
  Status() = default;

  /// Creates an error status carrying @p message.
  static Status Error(std::string message);

  /// Returns true if no error occurred.
  bool ok() const { return ok_; }

  /// Returns the error message (empty when ok()).
  const std::string& message() const { return message_; }

 private:
  bool ok_ = true;
  std::string message_;
};

}  // namespace vpp

#endif  // CPPVPP_VPP_STATUS_H_
