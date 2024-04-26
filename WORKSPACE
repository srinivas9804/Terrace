load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:git.bzl", "new_git_repository")
load("@bazel_tools//tools/cpp:cc_configure.bzl", "cc_configure")

cc_configure()

local_repository(
    name = "cpma",
    path = "./external/Packed-Memory-Array/include/",
)

local_repository(
    name = "CPAM",
    path = "./external/CPAM/include",
)

local_repository(
    name="parlaylib",
    path="./external/parlaylib/include",
)
local_repository(
    name="soa",
    path="./external/Packed-Memory-Array/StructOfArrays/include",
)
local_repository(
    name="ParallelTools",
    path="./external/Packed-Memory-Array/ParallelTools/",
)

