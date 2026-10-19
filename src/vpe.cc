#include "vpp/vpe.h"

#include "vpp/api/memclnt.api.h"
#include "vpp/api/vpe.api.h"
#include "vpp/rpc.h"

namespace vpp {

Status ShowVersion(Connection* conn, VersionInfo* out) {
  api::ShowVersion req;
  api::ShowVersionReply reply;
  Status s = Invoke(conn, req, &reply);
  if (!s.ok()) return s;
  if (reply.retval != 0) {
    return Status::Error("show_version failed, retval=" +
                         std::to_string(reply.retval));
  }
  out->program = reply.program;
  out->version = reply.version;
  out->build_date = reply.build_date;
  out->build_directory = reply.build_directory;
  return Status();
}

Status ControlPing(Connection* conn, PingReply* out) {
  api::ControlPing req;
  api::ControlPingReply reply;
  Status s = Invoke(conn, req, &reply);
  if (!s.ok()) return s;
  out->retval = reply.retval;
  out->client_index = reply.client_index;
  out->vpe_pid = reply.vpe_pid;
  return Status();
}

}  // namespace vpp
