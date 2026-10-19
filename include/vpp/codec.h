#ifndef CPPVPP_VPP_CODEC_H_
#define CPPVPP_VPP_CODEC_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace vpp {

/// Serializes values using the VPP binary API wire format (big-endian).
class Encoder {
 public:
  /// Appends a single byte.
  void PutU8(uint8_t v);
  /// Appends a big-endian 16-bit value.
  void PutU16(uint16_t v);
  /// Appends a big-endian 32-bit value.
  void PutU32(uint32_t v);
  /// Appends a big-endian 64-bit value.
  void PutU64(uint64_t v);
  /// Appends a big-endian signed 32-bit value.
  void PutI32(int32_t v) { PutU32(static_cast<uint32_t>(v)); }
  /// Appends a fixed-size, NUL padded string field of @p size bytes.
  /// Longer strings are truncated.
  void PutFixedString(const std::string& s, size_t size);
  /// Appends a variable length string: u32 length followed by the bytes.
  void PutVarString(const std::string& s);
  /// Appends raw bytes.
  void PutBytes(const std::vector<uint8_t>& bytes);

  /// Returns the encoded bytes.
  const std::vector<uint8_t>& bytes() const { return buf_; }

 private:
  std::vector<uint8_t> buf_;
};

/// Deserializes values from the VPP binary API wire format (big-endian).
///
/// Reads past the end of the buffer set a sticky failure flag and return 0.
class Decoder {
 public:
  /// Creates a decoder over @p data (not copied; must outlive the decoder).
  Decoder(const uint8_t* data, size_t size) : data_(data), size_(size) {}
  /// Creates a decoder over @p v (not copied; must outlive the decoder).
  explicit Decoder(const std::vector<uint8_t>& v)
      : data_(v.data()), size_(v.size()) {}

  /// Reads a byte.
  uint8_t GetU8();
  /// Reads a big-endian 16-bit value.
  uint16_t GetU16();
  /// Reads a big-endian 32-bit value.
  uint32_t GetU32();
  /// Reads a big-endian 64-bit value.
  uint64_t GetU64();
  /// Reads a big-endian signed 32-bit value.
  int32_t GetI32() { return static_cast<int32_t>(GetU32()); }
  /// Reads a variable length string (u32 length followed by the bytes).
  std::string GetVarString();
  /// Reads a fixed-size string field, trimmed at the first NUL.
  std::string GetFixedString(size_t size);

  /// Returns true if every read so far was in bounds.
  bool ok() const { return ok_; }
  /// Returns the number of unread bytes.
  size_t remaining() const { return size_ - pos_; }

 private:
  bool Need(size_t n);

  const uint8_t* data_;
  size_t size_;
  size_t pos_ = 0;
  bool ok_ = true;
};

}  // namespace vpp

#endif  // CPPVPP_VPP_CODEC_H_
