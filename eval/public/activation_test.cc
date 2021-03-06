#include "eval/public/activation.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "eval/public/cel_function.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

namespace {

using ::google::protobuf::Arena;
using testing::_;
using testing::ElementsAre;
using testing::Eq;
using testing::HasSubstr;
using testing::IsEmpty;
using testing::Property;
using testing::Return;

class MockValueProducer : public CelValueProducer {
 public:
  MOCK_METHOD1(Produce, CelValue(Arena*));
};

// Simple function that takes no args and returns an int64_t.
class ConstCelFunction : public CelFunction {
 public:
  explicit ConstCelFunction(absl::string_view name)
      : CelFunction({std::string(name), false, {}}) {}
  explicit ConstCelFunction(const CelFunctionDescriptor& desc)
      : CelFunction(desc) {}

  cel_base::Status Evaluate(absl::Span<const CelValue> args, CelValue* output,
                        google::protobuf::Arena* arena) const override {
    *output = CelValue::CreateInt64(42);
    return cel_base::OkStatus();
  }
};

TEST(ActivationTest, CheckValueInsertFindAndRemove) {
  Activation activation;

  Arena arena;

  activation.InsertValue("value42", CelValue::CreateInt64(42));

  // Test getting unbound value
  EXPECT_FALSE(activation.FindValue("value43", &arena));

  // Test getting bound value
  EXPECT_TRUE(activation.FindValue("value42", &arena));

  CelValue value = activation.FindValue("value42", &arena).value();

  // Test value is correct.
  EXPECT_THAT(value.Int64OrDie(), Eq(42));

  // Test removing unbound value
  EXPECT_FALSE(activation.RemoveValueEntry("value43"));

  // Test removing bound value
  EXPECT_TRUE(activation.RemoveValueEntry("value42"));

  // Now the value is unbound
  EXPECT_FALSE(activation.FindValue("value42", &arena));
}

TEST(ActivationTest, CheckValueProducerInsertFindAndRemove) {
  const std::string kValue = "42";

  auto producer = absl::make_unique<MockValueProducer>();

  google::protobuf::Arena arena;

  ON_CALL(*producer, Produce(&arena))
      .WillByDefault(Return(CelValue::CreateString(&kValue)));

  // ValueProducer is expected to be invoked only once.
  EXPECT_CALL(*producer, Produce(&arena)).Times(1);

  Activation activation;

  activation.InsertValueProducer("value42", std::move(producer));

  // Test getting unbound value
  EXPECT_FALSE(activation.FindValue("value43", &arena));

  // Test getting bound value - 1st pass

  // Access attempt is repeated twice.
  // ValueProducer is expected to be invoked only once.
  for (int i = 0; i < 2; i++) {
    auto opt_value = activation.FindValue("value42", &arena);
    EXPECT_TRUE(opt_value.has_value()) << " for pass " << i;
    CelValue value = opt_value.value();
    EXPECT_THAT(value.StringOrDie().value(), Eq(kValue)) << " for pass " << i;
  }

  // Test removing bound value
  EXPECT_TRUE(activation.RemoveValueEntry("value42"));

  // Now the value is unbound
  EXPECT_FALSE(activation.FindValue("value42", &arena));
}

TEST(ActivationTest, CheckInsertFunction) {
  Activation activation;
  auto insert_status = activation.InsertFunction(
      std::make_unique<ConstCelFunction>("ConstFunc"));
  EXPECT_TRUE(insert_status.ok());

  auto overloads = activation.FindFunctionOverloads("ConstFunc");
  EXPECT_THAT(overloads,
              ElementsAre(Property(
                  &CelFunction::descriptor,
                  Property(&CelFunctionDescriptor::name, Eq("ConstFunc")))));

  cel_base::Status status = activation.InsertFunction(
      std::make_unique<ConstCelFunction>("ConstFunc"));

  EXPECT_THAT(std::string(status.message()),
              HasSubstr("Function with same shape"));

  overloads = activation.FindFunctionOverloads("ConstFunc0");

  EXPECT_THAT(overloads, IsEmpty());
}

TEST(ActivationTest, CheckRemoveFunction) {
  Activation activation;
  auto insert_status =
      activation.InsertFunction(std::make_unique<ConstCelFunction>(
          CelFunctionDescriptor{"ConstFunc", false, {CelValue::Type::kInt64}}));
  EXPECT_TRUE(insert_status.ok());
  insert_status = activation.InsertFunction(std::make_unique<ConstCelFunction>(
      CelFunctionDescriptor{"ConstFunc", false, {CelValue::Type::kUint64}}));
  EXPECT_TRUE(insert_status.ok());

  auto overloads = activation.FindFunctionOverloads("ConstFunc");
  EXPECT_THAT(
      overloads,
      ElementsAre(
          Property(&CelFunction::descriptor,
                   Property(&CelFunctionDescriptor::name, Eq("ConstFunc"))),
          Property(&CelFunction::descriptor,
                   Property(&CelFunctionDescriptor::name, Eq("ConstFunc")))));

  EXPECT_TRUE(activation.RemoveFunctionEntries(
      {"ConstFunc", false, {CelValue::Type::kAny}}));

  overloads = activation.FindFunctionOverloads("ConstFunc");
  EXPECT_THAT(overloads, IsEmpty());
}

TEST(ActivationTest, CheckValueProducerClear) {
  const std::string kValue1 = "42";
  const std::string kValue2 = "43";

  auto producer1 = absl::make_unique<MockValueProducer>();
  auto producer2 = absl::make_unique<MockValueProducer>();

  google::protobuf::Arena arena;

  ON_CALL(*producer1, Produce(&arena))
      .WillByDefault(Return(CelValue::CreateString(&kValue1)));
  ON_CALL(*producer2, Produce(&arena))
      .WillByDefault(Return(CelValue::CreateString(&kValue2)));
  EXPECT_CALL(*producer1, Produce(&arena)).Times(2);
  EXPECT_CALL(*producer2, Produce(&arena)).Times(1);

  Activation activation;
  activation.InsertValueProducer("value42", std::move(producer1));
  activation.InsertValueProducer("value43", std::move(producer2));

  // Produce first value
  auto opt_value = activation.FindValue("value42", &arena);
  EXPECT_TRUE(opt_value.has_value());
  EXPECT_THAT(opt_value.value().StringOrDie().value(), Eq(kValue1));

  // Test clearing bound value
  EXPECT_TRUE(activation.ClearValueEntry("value42"));
  EXPECT_FALSE(activation.ClearValueEntry("value43"));

  // Produce second value
  auto opt_value2 = activation.FindValue("value43", &arena);
  EXPECT_TRUE(opt_value2.has_value());
  EXPECT_THAT(opt_value2.value().StringOrDie().value(), Eq(kValue2));

  // Clear all values
  EXPECT_EQ(1, activation.ClearCachedValues());
  EXPECT_FALSE(activation.ClearValueEntry("value42"));
  EXPECT_FALSE(activation.ClearValueEntry("value43"));

  // Produce first value again
  auto opt_value3 = activation.FindValue("value42", &arena);
  EXPECT_TRUE(opt_value3.has_value());
  EXPECT_THAT(opt_value3.value().StringOrDie().value(), Eq(kValue1));
  EXPECT_EQ(1, activation.ClearCachedValues());
}

}  // namespace

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
