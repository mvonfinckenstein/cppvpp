#ifndef CPPVPP_VPP_RPC_H_
#define CPPVPP_VPP_RPC_H_

#include <vector>

#include "vpp/codec.h"
#include "vpp/connection.h"
#include "vpp/status.h"
#include "vpp/wire.h"

namespace vpp {

/// Sends generated request @p req and decodes its reply into @p reply.
///
/// @tparam Req  Generated request message type with a `Reply` alias.
template <typename Req>
Status Invoke(Connection* conn, const Req& req, typename Req::Reply* reply) {
  Encoder e;
  req.Encode(e);
  std::vector<uint8_t> body;
  Status s = conn->Call(Req::kName, e.bytes(), Req::Reply::kName, &body);
  if (!s.ok()) return s;
  Decoder d(body);
  reply->Decode(d);
  if (!d.ok()) return Status::Error(std::string("truncated ") + Req::Reply::kName);
  return Status();
}

/// Sends generated dump request @p req and decodes all details messages.
///
/// @tparam Req  Generated request message type with a `Details` alias.
template <typename Req>
Status InvokeDump(Connection* conn, const Req& req,
                  std::vector<typename Req::Details>* out) {
  Encoder e;
  req.Encode(e);
  std::vector<std::vector<uint8_t>> raw;
  Status s = conn->Dump(Req::kName, e.bytes(), Req::Details::kName, &raw);
  if (!s.ok()) return s;
  for (const auto& body : raw) {
    Decoder d(body);
    typename Req::Details item;
    item.Decode(d);
    if (!d.ok()) return Status::Error(std::string("truncated ") + Req::Details::kName);
    out->push_back(std::move(item));
  }
  return Status();
}

}  // namespace vpp

#endif  // CPPVPP_VPP_RPC_H_
