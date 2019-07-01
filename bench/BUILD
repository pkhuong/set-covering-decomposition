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

cc_library(
    name = "quantile-test",
    srcs = ["quantile-test.cc"],
    hdrs = ["quantile-test.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":test-params",
        "@com_google_absl//absl/container:fixed_array",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/types:span",
        "@csm",
    ],
)

cc_test(
    name = "quantile-test_test",
    srcs = ["quantile-test_test.cc"],
    deps = [
        ":quantile-test",
        ":test-params",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "time",
    srcs = ["time.cc"],
    hdrs = ["time.h"],
    visibility = ["//visibility:public"],
    deps = [
        "@com_google_absl//absl/algorithm:container",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_test(
    name = "time_test",
    srcs = ["time_test.cc"],
    deps = [
        ":time",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "compare-functions",
    hdrs = ["compare-functions.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":constructable-array",
        ":meta",
        ":test-params",
        ":time",
        "//:prng",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "compare-functions_test",
    srcs = ["compare-functions_test.cc"],
    deps = [
        ":compare-functions",
        ":test-params",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "compare-functions-sign_test",
    srcs = ["compare-functions-sign_test.cc"],
    deps = [
        ":compare-functions",
        ":sign-test",
        ":test-params",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "compare-functions-quantile_test",
    size = "large",
    srcs = ["compare-functions-quantile_test.cc"],
    deps = [
        ":compare-functions",
        ":quantile-test",
        ":test-params",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "constructable-array",
    hdrs = ["internal/constructable-array.h"],
    visibility = ["//visibility:private"],
    deps = [],
)

cc_test(
    name = "constructable-array_test",
    srcs = ["internal/constructable-array_test.cc"],
    deps = [
        ":constructable-array",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "meta",
    hdrs = ["internal/meta.h"],
    visibility = ["//visibility:private"],
    deps = [
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/meta:type_traits",
        "@com_google_absl//absl/types:span",
        "@com_google_absl//absl/utility",
    ],
)
