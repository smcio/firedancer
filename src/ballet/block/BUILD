load("//bazel:fd_build_system.bzl", "fd_cc_library")

package(default_visibility = ["//src/ballet:__subpackages__"])

fd_cc_library(
    name = "block",
    hdrs = [
        "fd_block.h",
    ],
    deps = [
        "//src/ballet:base_lib",
        "//src/ballet/sha256",
    ],
)
