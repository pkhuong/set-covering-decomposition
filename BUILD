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

cc_binary(
  name = "random-set-cover",
  srcs = ["random-set-cover.cc"],
  deps = [
    "@com_google_absl//absl/flags:flag",
    "@com_google_absl//absl/flags:parse",
    "@com_google_absl//absl/types:span",
    ":driver",
    ":random-set-cover-flags",
    ":random-set-cover-instance",
    ":set-cover-solver",
    ":solution-stats",
  ],
)


cc_binary(
  name = "visualizer",
  srcs = select({
    ":gui": ["visualizer.cc"],
    "//conditions:default": ["dummy-visualizer.c"],
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
  linkopts = select({
    ":osx_gui": ["-framework OpenGL", "-lglfw", "-lm", "-ldl"],
    ":linux_gui": ["-lGL", "-lglfw", "-lrt", "-lm", "-ldl"],
    "//conditions:default": [],
  }),
)

cc_library(
  name = "random-set-cover-flags",
  hdrs = ["random-set-cover-flags.h"],
  srcs = ["random-set-cover-flags.cc"],
  deps = [
    "@com_google_absl//absl/flags:flag",
  ],
)

cc_library(
  name = "random-set-cover-instance",
  hdrs = ["random-set-cover-instance.h"],
  srcs = ["random-set-cover-instance.cc"],
  deps = [
    ":cover-constraint",
  ],
)

cc_library(
  name = "set-cover-solver",
  hdrs = ["set-cover-solver.h"],
  srcs = ["set-cover-solver.cc"],
  deps = [
    ":cover-constraint",
    ":driver",
    "@com_google_absl//absl/base:core_headers",
    "@com_google_absl//absl/synchronization",
    "@com_google_absl//absl/time:time",
    "@com_google_absl//absl/types:span",
  ],
)

cc_library(
  name = "solution-stats",
  hdrs = ["solution-stats.h"],
  srcs = ["solution-stats.cc"],
  deps = [
    "@com_google_absl//absl/strings:str_format",
    "@com_google_absl//absl/types:span",
  ],
)

cc_library(
  name = "driver",
  hdrs = ["driver.h"],
  srcs = ["driver.cc"],
  deps = [
    "@com_google_absl//absl/time:time",
    "@com_google_absl//absl/types:span",
    ":big-vec",
    ":cover-constraint",
    ":knapsack",
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
  hdrs = ["cover-constraint.h"],
  srcs = ["cover-constraint.cc"],
  deps = [
    "@com_google_absl//absl/algorithm:container",
    "@com_google_absl//absl/container:fixed_array",
    "@com_google_absl//absl/memory:memory",
    "@com_google_absl//absl/types:optional",
    "@com_google_absl//absl/types:span",
    ":vec",
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
    ":big-vec",
    ":prng",
    "@com_google_absl//absl/algorithm:container",
    "@com_google_absl//absl/types:optional",
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
    ":big-vec",
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

cc_library(
  name = "vec",
  hdrs = ["vec.h"],
  srcs = ["vec.cc", "avx_mathfun.h"],
  copts = ["-O3", "-march=native", "-mtune=native"],
  deps = [
    "@com_google_absl//absl/types:span",
  ],
)

cc_library(
  name = "prng",
  hdrs = ["prng.h"],
  srcs = ["prng.cc"],
  deps = [
    "@com_google_absl//absl/base:core_headers",
    "@com_google_absl//absl/synchronization",
    "@com_google_absl//absl/types:optional",
  ],
)

cc_library(
  name = "big-vec",
  hdrs = ["big-vec.h"],
  srcs = ["big-vec.cc"],
  deps = [
    "@com_google_absl//absl/base:core_headers",
    "@com_google_absl//absl/container:flat_hash_map",
    "@com_google_absl//absl/container:inlined_vector",
    "@com_google_absl//absl/synchronization",
  ],
)
