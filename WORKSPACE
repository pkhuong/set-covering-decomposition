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

http_archive(
  name = "gl3w",
  urls = ["https://github.com/skaslev/gl3w/archive/5c927b9b7c50cdd9e0e302332a21e8df72c2cb19.zip"],  # 2019-02-20
  strip_prefix = "gl3w-5c927b9b7c50cdd9e0e302332a21e8df72c2cb19",
  sha256 = "d369391ad2685a4bce25b57d3b350fac5831042e79e093f6b201d2724b634d2a",
  build_file = "//:BUILD.gl3w",
)

http_archive(
  name = "imgui",
  urls = ["https://github.com/ocornut/imgui/archive/eb7849b477ff96d8d0cc9f2f4a304b5fc0f3ac1a.zip"],  # 2019-05-30
  strip_prefix = "imgui-eb7849b477ff96d8d0cc9f2f4a304b5fc0f3ac1a",
  sha256 = "d4ef0f18f7bb9f5975b9108b7b5cae12864a9877f20ce2b3aba71012c3326eb7",
  build_file = "//:BUILD.imgui"
)