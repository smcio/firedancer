load("//bazel:fd_build_system.bzl", "fd_cc_library", "fd_cc_test")

package(default_visibility = ["//src/ballet:__subpackages__"])

fd_cc_library(
    name = "block",
    srcs = [
        "fd_block.c",
    ],
    hdrs = [
        "fd_block.h",
    ],
    deps = [
        "//src/ballet:base_lib",
        "//src/ballet/bmtree",
        "//src/ballet/sha256",
        "//src/ballet/txn",
    ],
)

fd_cc_test(
    srcs = ["test_block.c"],
    compile_data = ["//src/ballet/shred:fixtures/localnet-slot0-batch0.bin"],
    deps = ["//src/ballet"],
)
