load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

# Support for go
http_archive(
    name = "io_bazel_rules_go",
    sha256 = "a82a352bffae6bee4e95f68a8d80a70e87f42c4741e6a448bec11998fcc82329",
    urls = ["https://github.com/bazelbuild/rules_go/releases/download/0.18.5/rules_go-0.18.5.tar.gz"],
)

# in order to install buildifier.
http_archive(
    name = "com_github_bazelbuild_buildtools",
    sha256 = "0b91ee315743a210af49e78bb81c0b037981e43409be12433f7456c9331f9997",
    strip_prefix = "buildtools-eb1a85ca787f0f5f94ba66f41ee66fdfd4c49b70",  # 2019-06-06
    url = "https://github.com/bazelbuild/buildtools/archive/eb1a85ca787f0f5f94ba66f41ee66fdfd4c49b70.zip",
)

load("@io_bazel_rules_go//go:deps.bzl", "go_register_toolchains", "go_rules_dependencies")
load("@com_github_bazelbuild_buildtools//buildifier:deps.bzl", "buildifier_dependencies")

go_rules_dependencies()

go_register_toolchains()

buildifier_dependencies()

http_archive(
    name = "com_google_googletest",
    sha256 = "3830627f6394706fc09a9a53ca8b2ff90e1d0402a7041198b9bf54e66340e069",
    strip_prefix = "googletest-f71fb4f9a912ec945401cc49a287a759b6131026",
    urls = ["https://github.com/google/googletest/archive/f71fb4f9a912ec945401cc49a287a759b6131026.zip"],  # 2019-05-17
)

http_archive(
    name = "com_google_absl",
    sha256 = "b7b5fbd2e0b727ef156e73f184aac7edbeeea413ebd81c45b2aae6f9bda125a0",
    strip_prefix = "abseil-cpp-8a394b19c149cab50534b04c5e21d42bc2217a7d",
    urls = ["https://github.com/abseil/abseil-cpp/archive/8a394b19c149cab50534b04c5e21d42bc2217a7d.zip"],  # 2019-05-17
)

http_archive(
    name = "gl3w",
    build_file = "//:BUILD.gl3w",
    sha256 = "d369391ad2685a4bce25b57d3b350fac5831042e79e093f6b201d2724b634d2a",
    strip_prefix = "gl3w-5c927b9b7c50cdd9e0e302332a21e8df72c2cb19",
    urls = ["https://github.com/skaslev/gl3w/archive/5c927b9b7c50cdd9e0e302332a21e8df72c2cb19.zip"],  # 2019-02-20
)

http_archive(
    name = "imgui",
    build_file = "//:BUILD.imgui",
    sha256 = "d4ef0f18f7bb9f5975b9108b7b5cae12864a9877f20ce2b3aba71012c3326eb7",
    strip_prefix = "imgui-eb7849b477ff96d8d0cc9f2f4a304b5fc0f3ac1a",
    urls = ["https://github.com/ocornut/imgui/archive/eb7849b477ff96d8d0cc9f2f4a304b5fc0f3ac1a.zip"],  # 2019-05-30
)

http_archive(
    name = "bazel-compilation-database",
    sha256 = "9c7f1f4060c6c0ccfa0e7c2b8cd9c774b232c1b762386bfc063e7c4919be0458",
    strip_prefix = "bazel-compilation-database-d5a0ee259aa356886618eafae17ca05ebf79d6c2",
    urls = ["https://github.com/grailbio/bazel-compilation-database/archive/d5a0ee259aa356886618eafae17ca05ebf79d6c2.zip"],  # 2019-06-07
)

http_archive(
    name = "csm",
    sha256 = "af816ca7911d836e86ff35983012eff9b091d00c071ca51fb5d1f8413b5cd9ca",
    strip_prefix = "csm-463915bdabb94e474ed2e491b14d817b959e1a3e",
    urls = ["https://github.com/pkhuong/csm/archive/463915bdabb94e474ed2e491b14d817b959e1a3e.zip"],  # 2019-07-14
)

http_archive(
    name = "martingale-cs",
    sha256 = "23a8ae5b6917aa43fb813665042e3b57a4df3b236223a6dd7dcdbfd4743545bf",
    strip_prefix = "martingale-cs-ace480696c0d7b749824cd0b4f9ed51438a8d2e8",
    urls = ["https://github.com/pkhuong/martingale-cs/archive/ace480696c0d7b749824cd0b4f9ed51438a8d2e8.zip"],  # 2019-07-14
)

http_archive(
    name = "one-sided-ks",
    sha256 = "848563c6a34ec5b34f8747c3cf23c781d657e3095b949089229d5b11132bc598",
    strip_prefix = "one-sided-ks-30cae6c134196bb42b292746177f35c065d42852",
    urls = ["https://github.com/pkhuong/one-sided-ks/archive/30cae6c134196bb42b292746177f35c065d42852.zip"],  # 2019-07-20
)

http_archive(
    name = "com_github_nelhage_rules_boost",
    sha256 = "b24dd149c0cc9f7ff5689d91f99aaaea0d340baf9911439b573f02074148533a",
    strip_prefix = "rules_boost-417642961150e987bc1ac78c7814c617566ffdaa",
    urls = ["https://github.com/nelhage/rules_boost/archive/417642961150e987bc1ac78c7814c617566ffdaa.zip"],  # 2019-06-29
)

load("@com_github_nelhage_rules_boost//:boost/boost.bzl", "boost_deps")

boost_deps()
