cc_library(
  name = "cover-constraint",
  hdrs = ["cover-constraint.h"],
  srcs = ["cover-constraint.cc"],
  deps = [
    "@com_google_absl//absl/algorithm:container",
    "@com_google_absl//absl/container:fixed_array",
    "@com_google_absl//absl/types:span",
  ]
)

cc_test(
  name = "cover-constraint_test",
  srcs = ["cover-constraint_test.cc"],
  deps = [
    ":cover-constraint",
    "@com_google_googletest//:gtest_main",
  ]
)

cc_library(
  name = "knapsack-impl",
  hdrs = ["knapsack-impl.h"],
  srcs = ["knapsack-impl.cc"],
  deps = [
    "@com_google_absl//absl/container:flat_hash_set",
    "@com_google_absl//absl/types:span",
  ]
)

cc_test(
  name = "knapsack-impl_test",
  srcs = ["knapsack-impl_test.cc"],
  deps = [
    ":knapsack-impl",
    "@com_google_googletest//:gtest",
    "@com_google_googletest//:gtest_main",
  ]
)

cc_library(
  name = "knapsack",
  hdrs = ["knapsack.h"],
  srcs = ["knapsack.cc"],
  deps = [
    ":knapsack-impl",
    "@com_google_absl//absl/container:flat_hash_set",
    "@com_google_absl//absl/types:span",
  ]
)

cc_test(
  name = "knapsack_test",
  srcs = ["knapsack_test.cc"],
  deps = [
    ":knapsack",
    "@com_google_googletest//:gtest",
    "@com_google_googletest//:gtest_main",
  ]
)
