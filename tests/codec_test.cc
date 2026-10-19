#include "vpp/codec.h"

#include <gtest/gtest.h>

namespace vpp {
namespace {

TEST(CodecTest, RoundTrip) {
  Encoder e;
  e.PutU8(1);
  e.PutU16(0x0203);
  e.PutU32(0x04050607);
  e.PutI32(-2);
  e.PutFixedString("abc", 5);
  EXPECT_EQ(e.bytes()[1], 2);  // Big-endian.
  EXPECT_EQ(e.bytes().size(), 1u + 2 + 4 + 4 + 5);

  Decoder d(e.bytes());
  EXPECT_EQ(d.GetU8(), 1);
  EXPECT_EQ(d.GetU16(), 0x0203);
  EXPECT_EQ(d.GetU32(), 0x04050607u);
  EXPECT_EQ(d.GetI32(), -2);
  EXPECT_EQ(d.GetFixedString(5), "abc");
  EXPECT_TRUE(d.ok());
  EXPECT_EQ(d.remaining(), 0u);
}

TEST(CodecTest, FixedStringTruncates) {
  Encoder e;
  e.PutFixedString("abcdef", 3);
  EXPECT_EQ(e.bytes().size(), 3u);
}

TEST(CodecTest, ShortReadFails) {
  Encoder e;
  e.PutU16(1);
  Decoder d(e.bytes());
  EXPECT_EQ(d.GetU32(), 0u);
  EXPECT_FALSE(d.ok());
}

}  // namespace
}  // namespace vpp
