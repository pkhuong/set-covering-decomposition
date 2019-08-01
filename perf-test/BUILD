cc_library(
    name = "find-min-value-interface",
    hdrs = ["find-min-value.h"],
    copts = ["-fvisibility=hidden"],
    linkstatic = True,
    deps = [
        "//bench:stable-unique-ptr",
        "@com_google_absl//absl/types:span",
    ],
)

cc_binary(
    name = "libbase-find-min-value.so",
    srcs = ["base-find-min-value.cc"],
    copts = ["-fvisibility=hidden"],
    linkshared = True,
    linkstatic = True,
    deps = [
        ":find-min-value-interface",
        "//:vec",
        "//bench:timing-function",
    ],
)

cc_binary(
    name = "find-min-value_test",
    srcs = [
        "find-min-value.cc",
        "find-min-value.h",
    ],
    copts = ["-fvisibility=hidden"],
    linkstatic = True,
    deps = [
        ":libbase-find-min-value.so",
        "//bench:bounded-mean-test",
        "//bench:compare-functions",
        "//bench:extract-timing-function",
        "//bench:kolmogorov-smirnov-test",
        "//bench:quantile-test",
        "//bench:stable-unique-ptr",
        "//bench:timing-function",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/types:span",
    ],
)
