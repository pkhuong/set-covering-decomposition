config_setting(
    name = "gui",
    values = {"define": "gui=yes"},
)

config_setting(
    name = "osx_gui",
    constraint_values = ["@bazel_tools//platforms:osx"],
    values = {"define": "gui=yes"},
)

config_setting(
    name = "linux_gui",
    constraint_values = ["@bazel_tools//platforms:linux"],
    values = {"define": "gui=yes"},
)

alias(
    name = "buildifier",
    actual = "@com_github_bazelbuild_buildtools//buildifier:buildifier",
)

load("@bazel-compilation-database//:aspects.bzl", "compilation_database")

compilation_database(
    name = "compile_commands",
    targets = [
        ":random-set-cover",
        ":visualizer",
    ],
)

cc_binary(
    name = "random-set-cover",
    srcs = ["random-set-cover.cc"],
    deps = [
        ":driver",
        ":random-set-cover-flags",
        ":random-set-cover-instance",
        ":set-cover-solver",
        ":solution-stats",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/types:span",
    ],
)

cc_binary(
    name = "visualizer",
    srcs = select({
        ":gui": ["visualizer.cc"],
        "//conditions:default": ["dummy-visualizer.c"],
    }),
    linkopts = select({
        ":osx_gui": [
            "-framework OpenGL",
            "-lglfw",
            "-lm",
            "-ldl",
        ],
        ":linux_gui": [
            "-lGL",
            "-lglfw",
            "-lrt",
            "-lm",
            "-ldl",
        ],
        "//conditions:default": [],
    }),
    deps = select({
        ":gui": [
            ":driver",
            ":random-set-cover-flags",
            ":random-set-cover-instance",
            ":set-cover-solver",
            ":solution-stats",
            "@com_google_absl//absl/flags:flag",
            "@com_google_absl//absl/flags:parse",
            "@com_google_absl//absl/types:span",
            "@gl3w//:gl3w",
            "@imgui//:imgui",
            "@imgui//:imgui-impl",
        ],
        "//conditions:default": [],
    }),
)

cc_library(
    name = "random-set-cover-flags",
    srcs = ["random-set-cover-flags.cc"],
    hdrs = ["random-set-cover-flags.h"],
    deps = [
        "@com_google_absl//absl/flags:flag",
    ],
)

cc_library(
    name = "random-set-cover-instance",
    srcs = ["random-set-cover-instance.cc"],
    hdrs = ["random-set-cover-instance.h"],
    deps = [
        ":cover-constraint",
    ],
)

cc_library(
    name = "set-cover-solver",
    srcs = ["set-cover-solver.cc"],
    hdrs = ["set-cover-solver.h"],
    deps = [
        ":cover-constraint",
        ":driver",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "solution-stats",
    srcs = ["solution-stats.cc"],
    hdrs = ["solution-stats.h"],
    deps = [
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "driver",
    srcs = ["driver.cc"],
    hdrs = ["driver.h"],
    deps = [
        ":big-vec",
        ":cover-constraint",
        ":knapsack",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "driver_test",
    srcs = ["driver_test.cc"],
    deps = [
        ":driver",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "cover-constraint",
    srcs = ["cover-constraint.cc"],
    hdrs = ["cover-constraint.h"],
    deps = [
        ":vec",
        "@com_google_absl//absl/algorithm:container",
        "@com_google_absl//absl/container:fixed_array",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "cover-constraint_test",
    srcs = ["cover-constraint_test.cc"],
    deps = [
        ":cover-constraint",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "knapsack-impl",
    srcs = ["knapsack-impl.cc"],
    hdrs = ["knapsack-impl.h"],
    deps = [
        ":big-vec",
        ":prng",
        "@com_google_absl//absl/algorithm:container",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "knapsack-impl_test",
    srcs = ["knapsack-impl_test.cc"],
    deps = [
        ":knapsack-impl",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "knapsack",
    srcs = ["knapsack.cc"],
    hdrs = ["knapsack.h"],
    deps = [
        ":big-vec",
        ":knapsack-impl",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "knapsack_test",
    srcs = ["knapsack_test.cc"],
    deps = [
        ":knapsack",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "vec",
    srcs = [
        "avx_mathfun.h",
        "vec.cc",
    ],
    hdrs = ["vec.h"],
    copts = [
        "-O3",
        "-march=native",
        "-mtune=native",
    ],
    deps = [
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "prng",
    srcs = ["prng.cc"],
    hdrs = ["prng.h"],
    visibility = ["//:__subpackages__"],
    deps = [
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_library(
    name = "big-vec",
    srcs = ["big-vec.cc"],
    hdrs = ["big-vec.h"],
    deps = [
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:inlined_vector",
        "@com_google_absl//absl/synchronization",
    ],
)
