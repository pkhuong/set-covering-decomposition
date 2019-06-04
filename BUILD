cc_binary(
  name = "random-set-cover",
  srcs = ["random-set-cover.cc"],
  deps = [
    "@com_google_absl//absl/strings:str_format",
    "@com_google_absl//absl/time:time",
    "@com_google_absl//absl/types:span",
    ":driver",
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

cc_library(
  name = "imgui",
  hdrs = ["imgui/imgui.h"],
  srcs = ["imgui/imgui.cpp", "imgui/imconfig.h", "imgui/imgui_demo.cpp", "imgui/imgui_draw.cpp",
          "imgui/imgui_internal.h", "imgui/imgui_widgets.cpp", "imgui/imstb_rectpack.h",
          "imgui/imstb_textedit.h", "imgui/imstb_truetype.h"],
  copts = ["-DIMGUI_DISABLE_OBSOLETE_FUNCTIONS"],
)

cc_library(
  name = "imgui-impl",
  hdrs = ["imgui/examples/imgui_impl_glfw.h", "imgui/examples/imgui_impl_opengl3.h",],
  srcs = ["imgui/examples/imgui_impl_glfw.cpp", "imgui/examples/imgui_impl_opengl3.cpp"],
  copts = ["'-DIMGUI_IMPL_OPENGL_LOADER_CUSTOM=\"external/gl3w/GL/gl3w.h\"'",
           "-DIMGUI_DISABLE_OBSOLETE_FUNCTIONS", "-Iimgui"],
  deps = [":imgui", "@gl3w//:gl3w"],
)

cc_binary(
  name = "imgui-demo",
  srcs = ["imgui/examples/example_glfw_opengl3/main.cpp"],
  deps = [":imgui", ":imgui-impl", "@gl3w//:gl3w"],
  copts = ["'-DIMGUI_IMPL_OPENGL_LOADER_CUSTOM=\"GL/gl3w.h\"'",
           "-DIMGUI_DISABLE_OBSOLETE_FUNCTIONS","-Iimgui", "-Iimgui/examples"],
  linkopts = ["-lGL", "-lglfw", "-lrt", "-lm", "-ldl"],
)
