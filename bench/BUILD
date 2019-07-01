# Benchmarking utilities to perform statistical tests, esp for continuous perf testing

cc_library(
    name = "test-params",
    srcs = ["test-params.cc"],
    hdrs = ["test-params.h"],
    visibility = ["//visibility:public"],
    deps = [
        "@com_google_absl//absl/types:optional",
    ],
)

cc_library(
    name = "sign-test",
    srcs = ["sign-test.cc"],
    hdrs = ["sign-test.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":test-params",
        "@com_google_absl//absl/types:span",
        "@csm",
    ],
)

cc_test(
    name = "sign-test_test",
    srcs = ["sign-test_test.cc"],
    deps = [
        ":sign-test",
        ":test-params",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)
