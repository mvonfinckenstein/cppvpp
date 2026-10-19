#ifndef CPPVPP_VPP_WIRE_H_
#define CPPVPP_VPP_WIRE_H_

#include <array>
#include <cstdint>
#include <cstring>

#include "vpp/codec.h"

/// \file
/// Overloaded WirePut/WireGet helpers used by the generated API headers.
/// Generated structs and enums provide their own overloads (via Encode/Decode
/// members and generated inline functions).

namespace vpp {

/// Encodes a byte.
inline void WirePut(Encoder& e, uint8_t v) { e.PutU8(v); }
/// Encodes a signed byte.
inline void WirePut(Encoder& e, int8_t v) { e.PutU8(static_cast<uint8_t>(v)); }
/// Encodes a boolean as one byte.
inline void WirePut(Encoder& e, bool v) { e.PutU8(v ? 1 : 0); }
/// Encodes a 16-bit value.
inline void WirePut(Encoder& e, uint16_t v) { e.PutU16(v); }
/// Encodes a signed 16-bit value.
inline void WirePut(Encoder& e, int16_t v) { e.PutU16(static_cast<uint16_t>(v)); }
/// Encodes a 32-bit value.
inline void WirePut(Encoder& e, uint32_t v) { e.PutU32(v); }
/// Encodes a signed 32-bit value.
inline void WirePut(Encoder& e, int32_t v) { e.PutI32(v); }
/// Encodes a 64-bit value.
inline void WirePut(Encoder& e, uint64_t v) { e.PutU64(v); }
/// Encodes a signed 64-bit value.
inline void WirePut(Encoder& e, int64_t v) { e.PutU64(static_cast<uint64_t>(v)); }
/// Encodes a double as its IEEE-754 bit pattern.
inline void WirePut(Encoder& e, double v) {
  uint64_t bits;
  std::memcpy(&bits, &v, sizeof(bits));
  e.PutU64(bits);
}

/// Decodes a byte.
inline void WireGet(Decoder& d, uint8_t* v) { *v = d.GetU8(); }
/// Decodes a signed byte.
inline void WireGet(Decoder& d, int8_t* v) { *v = static_cast<int8_t>(d.GetU8()); }
/// Decodes a boolean.
inline void WireGet(Decoder& d, bool* v) { *v = d.GetU8() != 0; }
/// Decodes a 16-bit value.
inline void WireGet(Decoder& d, uint16_t* v) { *v = d.GetU16(); }
/// Decodes a signed 16-bit value.
inline void WireGet(Decoder& d, int16_t* v) { *v = static_cast<int16_t>(d.GetU16()); }
/// Decodes a 32-bit value.
inline void WireGet(Decoder& d, uint32_t* v) { *v = d.GetU32(); }
/// Decodes a signed 32-bit value.
inline void WireGet(Decoder& d, int32_t* v) { *v = d.GetI32(); }
/// Decodes a 64-bit value.
inline void WireGet(Decoder& d, uint64_t* v) { *v = d.GetU64(); }
/// Decodes a signed 64-bit value.
inline void WireGet(Decoder& d, int64_t* v) { *v = static_cast<int64_t>(d.GetU64()); }
/// Decodes a double.
inline void WireGet(Decoder& d, double* v) {
  uint64_t bits = d.GetU64();
  std::memcpy(v, &bits, sizeof(bits));
}

/// Encodes any generated struct through its Encode() member.
template <typename T>
auto WirePut(Encoder& e, const T& v) -> decltype(v.Encode(e), void()) {
  v.Encode(e);
}

/// Decodes any generated struct through its Decode() member.
template <typename T>
auto WireGet(Decoder& d, T* v) -> decltype(v->Decode(d), void()) {
  v->Decode(d);
}

/// Encodes a fixed-size array element by element.
template <typename T, size_t N>
void WirePut(Encoder& e, const std::array<T, N>& v) {
  for (const T& x : v) WirePut(e, x);
}

/// Decodes a fixed-size array element by element.
template <typename T, size_t N>
void WireGet(Decoder& d, std::array<T, N>* v) {
  for (T& x : *v) WireGet(d, &x);
}

}  // namespace vpp

#endif  // CPPVPP_VPP_WIRE_H_
