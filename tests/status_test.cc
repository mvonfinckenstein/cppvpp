#include "vpp/status.h"

#include <gtest/gtest.h>

namespace vpp {
namespace {

TEST(StatusTest, DefaultIsOk) {
  Status s;
  EXPECT_TRUE(s.ok());
  EXPECT_TRUE(s.message().empty());
}

TEST(StatusTest, ErrorCarriesMessage) {
  Status s = Status::Error("boom");
  EXPECT_FALSE(s.ok());
  EXPECT_EQ(s.message(), "boom");
}

}  // namespace
}  // namespace vpp
