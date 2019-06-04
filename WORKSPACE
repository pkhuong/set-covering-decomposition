load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
  name = "com_google_googletest",
  urls = ["https://github.com/google/googletest/archive/f71fb4f9a912ec945401cc49a287a759b6131026.zip"],  # 2019-05-17
  strip_prefix = "googletest-f71fb4f9a912ec945401cc49a287a759b6131026",
  sha256 = "3830627f6394706fc09a9a53ca8b2ff90e1d0402a7041198b9bf54e66340e069",
)

http_archive(
  name = "com_google_absl",
  urls = ["https://github.com/abseil/abseil-cpp/archive/8a394b19c149cab50534b04c5e21d42bc2217a7d.zip"],  # 2019-05-17
  strip_prefix = "abseil-cpp-8a394b19c149cab50534b04c5e21d42bc2217a7d",
  sha256 = "b7b5fbd2e0b727ef156e73f184aac7edbeeea413ebd81c45b2aae6f9bda125a0",
)

## gl3w: generate the files, patch up <> -> "", then build libgl3w.
GL3W_BUILD_CONTENTS = """
genrule(
  name = "gl3w_gen",
  visibility = ["//visibility:private"],
  srcs = [],
  outs = [
    "include/GL/gl3w.h",
    "include/GL/glcorearb.h",
    "include/KHR/khrplatform.h",
    "GL/gl3w.h",
    "GL/glcorearb.h",
    "KHR/khrplatform.h",
    "src/gl3w.c",
  ],
  cmd = \"""
external/gl3w/gl3w_gen.py --root $(RULEDIR);
sed -e 's|<GL/glcorearb.h>|"GL/glcorearb.h"|' -i $(location include/GL/gl3w.h);
sed -e 's|<GL/gl3w.h>|"GL/gl3w.h"|' -i $(location src/gl3w.c);
cp -r $(RULEDIR)/include/GL $(RULEDIR);
cp -r $(RULEDIR)/include/KHR $(RULEDIR);
\""",
  local=True,
  message="Generating gl3w headers",
  output_to_bindir=False,
)

cc_library(
  name = "gl3w",
  visibility = ["//visibility:public"],
  hdrs = [
    "GL/gl3w.h",
    "GL/glcorearb.h",
    "KHR/khrplatform.h",
  ],
  srcs = ["src/gl3w.c"],
)
"""

http_archive(
  name = "gl3w",
  urls = ["https://github.com/skaslev/gl3w/archive/5c927b9b7c50cdd9e0e302332a21e8df72c2cb19.zip"],  # 2019-02-20
  strip_prefix = "gl3w-5c927b9b7c50cdd9e0e302332a21e8df72c2cb19",
  sha256 = "d369391ad2685a4bce25b57d3b350fac5831042e79e093f6b201d2724b634d2a",
  build_file_content = GL3W_BUILD_CONTENTS,
)

IMGUI_BUILD_CONTENTS="""
cc_library(
  name = "imgui",
  visibility = ["//visibility:public"],
  hdrs = ["imgui.h"],
  srcs = [
    "imgui.cpp",
    "imconfig.h",
    "imgui_demo.cpp",
    "imgui_draw.cpp",
    "imgui_internal.h",
    "imgui_widgets.cpp",
    "imstb_rectpack.h",
    "imstb_textedit.h",
    "imstb_truetype.h",
  ],
  copts = ["-DIMGUI_DISABLE_OBSOLETE_FUNCTIONS"],
)

genrule(
  name = "relocate-impl",
  visibility = ["//visibility:private"],
  srcs = ["examples/imgui_impl_glfw.h", "examples/imgui_impl_opengl3.h"],
  outs = ["imgui_impl_glfw.h", "imgui_impl_opengl3.h"],
  cmd = \"""
cp $(location examples/imgui_impl_glfw.h) $(location imgui_impl_glfw.h)
cp $(location examples/imgui_impl_opengl3.h) $(location imgui_impl_opengl3.h)
\""",
  local=True,
  message="Generating fixed up demo file",
  output_to_bindir=False,
)

cc_library(
  name = "imgui-impl",
  visibility = ["//visibility:public"],
  hdrs = ["imgui_impl_glfw.h", "imgui_impl_opengl3.h"],
  srcs = ["examples/imgui_impl_glfw.cpp", "examples/imgui_impl_opengl3.cpp"],
  copts = ["'-DIMGUI_IMPL_OPENGL_LOADER_CUSTOM=\\"../gl3w/GL/gl3w.h\\"'",
           "-DIMGUI_DISABLE_OBSOLETE_FUNCTIONS"],
  deps = [":imgui", "@gl3w//:gl3w"],
)

genrule(
  name = "fixedup-demo",
  visibility = ["//visibility:private"],
  srcs = ["examples/example_glfw_opengl3/main.cpp"],
  outs = ["demo.cpp"],
  cmd = \"""
egrep -q 'bool err = false; +//.*IMGUI_IMPL_OPENGL_LOADER_CUSTOM' $(location examples/example_glfw_opengl3/main.cpp) || echo "Unable to find line to fix up";
sed -re 's|bool err = false; +//.*IMGUI_IMPL_OPENGL_LOADER_CUSTOM.*$$|bool err = gl3wInit() != 0;|' < $(location examples/example_glfw_opengl3/main.cpp) > $(location demo.cpp)
\""",
  local=True,
  message="Generating fixed up demo file",
  output_to_bindir=False,
)

cc_binary(
  name = "demo",
  visibility = ["//visibility:public"],
  srcs = ["demo.cpp"],
  deps = [
    "imgui",
    "imgui-impl",
    "@gl3w//:gl3w",
  ],
  copts = ["'-DIMGUI_IMPL_OPENGL_LOADER_CUSTOM=\\"../gl3w/GL/gl3w.h\\"'",
           "-DIMGUI_DISABLE_OBSOLETE_FUNCTIONS"],
  linkopts = ["-lGL", "-lglfw", "-lrt", "-lm", "-ldl"],
)
"""

http_archive(
  name = "imgui",
  urls = ["https://github.com/ocornut/imgui/archive/eb7849b477ff96d8d0cc9f2f4a304b5fc0f3ac1a.zip"],  # 2019-05-30
  strip_prefix = "imgui-eb7849b477ff96d8d0cc9f2f4a304b5fc0f3ac1a",
  sha256 = "d4ef0f18f7bb9f5975b9108b7b5cae12864a9877f20ce2b3aba71012c3326eb7",
  build_file_content = IMGUI_BUILD_CONTENTS,
)