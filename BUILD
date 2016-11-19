COMMON_COPTS = ["--std=c++1y", "-std=c++1y"]

cc_library(
    name = "error",
    hdrs = ["error.h"],
    copts = COMMON_COPTS,
)

cc_library(
    name = "value",
    hdrs = ["value.h"],
    copts = COMMON_COPTS,
)

cc_library(
    name = "input",
    hdrs = ["input.h"],
    copts = COMMON_COPTS,
    deps = [
        ":error",
        ":value",
    ],
)

cc_library(
    name = "output",
    hdrs = ["output.h"],
    copts = COMMON_COPTS,
    deps = [
        ":error",
        ":value",
    ],
)

cc_library(
    name = "node",
    srcs = ["node.cc"],
    hdrs = ["node.h"],
    copts = COMMON_COPTS,
    deps = [
        ":error",
        ":input",
        ":output",
    ],
)

cc_library(
    name = "producer_graph",
    hdrs = ["producer_graph.h"],
    copts = COMMON_COPTS,
    deps = [
        ":error",
        ":input",
        ":node",
        ":output",
    ],
)

cc_test(
    name = "producers_integration_test",
    srcs = ["producers_integration_test.cc"],
    copts = COMMON_COPTS + [
        "-Iexternal/gtest/include",
    ],
    deps = [
        ":output",
        ":producer_graph",
        "//third_party/gtest",
    ],
)
