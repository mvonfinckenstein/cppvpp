#ifndef CPPVPP_VPP_VPE_H_
#define CPPVPP_VPP_VPE_H_

#include <cstdint>
#include <string>

#include "vpp/connection.h"
#include "vpp/status.h"

namespace vpp {

/// Result of the `show_version` API call.
struct VersionInfo {
  std::string program;          ///< Program name, e.g. "vpe".
  std::string version;          ///< Version string.
  std::string build_date;       ///< Build date.
  std::string build_directory;  ///< Build directory.
};

/// Result of the `control_ping` API call.
struct PingReply {
  int32_t retval = 0;      ///< VPP return code.
  uint32_t client_index = 0;  ///< Client index as seen by VPP.
  uint32_t vpe_pid = 0;    ///< Process id of VPP.
};

/// Calls `show_version` on @p conn and fills @p out.
Status ShowVersion(Connection* conn, VersionInfo* out);

/// Calls `control_ping` on @p conn and fills @p out.
Status ControlPing(Connection* conn, PingReply* out);

}  // namespace vpp

#endif  // CPPVPP_VPP_VPE_H_
