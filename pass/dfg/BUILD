#  This file is distributed under the BSD 3-Clause License. See LICENSE for details.
cc_library(
    name = "pass_dfg",
    srcs = glob(["*.cpp"], exclude=["lgdfg.cpp", "cf2df*"]),
    hdrs = glob(["*.hpp"]),
    visibility = ["//visibility:public"],
    includes = ["."],
    deps = [
        "//core:core",
        "//inou/cfg:inou_cfg",
    ]
)

