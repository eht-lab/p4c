"""Load dependencies needed to compile p4c as a 3rd-party consumer."""

"""Load dependencies needed to compile p4c as a 3rd-party consumer."""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def p4c_bzlmod_deps(name = "com_github_p4lang_p4c_extension"):
    """Loads dependencies need to compile p4c.

    Args:
        name: The name of the repository.
    Third party projects can define the target
    @com_github_p4lang_p4c_extension:ir_extension with a `filegroup`
    containing their custom .def files.
    """
    if not native.existing_rule("com_github_p4lang_p4c_extension"):
        # By default, no IR extensions.
        native.new_local_repository(
            name = name,
            path = ".",
            build_file_content = """
filegroup(
    name = "ir_extension",
    srcs = [],
    visibility = ["//visibility:public"],
)
            """,
        )
