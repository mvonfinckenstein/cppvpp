#include "vpp/codec.h"

#include <algorithm>

namespace vpp {

void Encoder::PutU8(uint8_t v) { buf_.push_back(v); }

void Encoder::PutU16(uint16_t v) {
  buf_.push_back(static_cast<uint8_t>(v >> 8));
  buf_.push_back(static_cast<uint8_t>(v));
}

void Encoder::PutU32(uint32_t v) {
  for (int shift = 24; shift >= 0; shift -= 8) {
    buf_.push_back(static_cast<uint8_t>(v >> shift));
  }
}

void Encoder::PutU64(uint64_t v) {
  PutU32(static_cast<uint32_t>(v >> 32));
  PutU32(static_cast<uint32_t>(v));
}

void Encoder::PutVarString(const std::string& s) {
  PutU32(static_cast<uint32_t>(s.size()));
  buf_.insert(buf_.end(), s.begin(), s.end());
}

void Encoder::PutFixedString(const std::string& s, size_t size) {
  size_t n = std::min(s.size(), size);
  buf_.insert(buf_.end(), s.begin(), s.begin() + n);
  buf_.insert(buf_.end(), size - n, 0);
}

void Encoder::PutBytes(const std::vector<uint8_t>& bytes) {
  buf_.insert(buf_.end(), bytes.begin(), bytes.end());
}

bool Decoder::Need(size_t n) {
  if (!ok_ || size_ - pos_ < n) {
    ok_ = false;
    return false;
  }
  return true;
}

uint8_t Decoder::GetU8() {
  if (!Need(1)) return 0;
  return data_[pos_++];
}

uint16_t Decoder::GetU16() {
  if (!Need(2)) return 0;
  uint16_t v = static_cast<uint16_t>((data_[pos_] << 8) | data_[pos_ + 1]);
  pos_ += 2;
  return v;
}

uint32_t Decoder::GetU32() {
  if (!Need(4)) return 0;
  uint32_t v = 0;
  for (int i = 0; i < 4; ++i) v = (v << 8) | data_[pos_ + i];
  pos_ += 4;
  return v;
}

uint64_t Decoder::GetU64() {
  uint64_t hi = GetU32();
  return (hi << 32) | GetU32();
}

std::string Decoder::GetVarString() {
  uint32_t n = GetU32();
  if (!Need(n)) return std::string();
  std::string s(reinterpret_cast<const char*>(data_ + pos_), n);
  pos_ += n;
  return s;
}

std::string Decoder::GetFixedString(size_t size) {
  if (!Need(size)) return std::string();
  const char* p = reinterpret_cast<const char*>(data_ + pos_);
  pos_ += size;
  size_t len = 0;
  while (len < size && p[len] != '\0') ++len;
  return std::string(p, len);
}

}  // namespace vpp
