package(
    default_visibility = ["//visibility:public"],
)

cc_library(
    name = "btree_set",
    hdrs = ["include/tlx/tlx/container/btree_set.hpp"],
)

cc_library(
    name = "btree_map",
    hdrs = ["include/tlx/tlx/container/btree_map.hpp"],
)

cc_library(
  name = "terrace_graph",
  hdrs = ["include/terrace_graph.h"],
  deps = [
    "btree_set",
    "btree_map",
  ]
)
