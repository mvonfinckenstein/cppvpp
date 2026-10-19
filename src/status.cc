#include "vpp/status.h"

namespace vpp {

Status Status::Error(std::string message) {
  Status s;
  s.ok_ = false;
  s.message_ = std::move(message);
  return s;
}

}  // namespace vpp
