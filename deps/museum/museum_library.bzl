"""Wrapper for a Museum import library."""

load("//buck_imports:profilo_path.bzl", "profilo_path")
load("//build_defs:fb_xplat_cxx_library.bzl", "fb_xplat_cxx_library")


def museum_library(
        museum_deps=[],
        exported_preprocessor_flags=[],
        compiler_flags=[],
        extra_visibility=[],
        exported_headers=None,
        exported_platform_headers=None):
    """
    Wrapper for a Museum library.

    This wrapper defines two targets - a header-only target, and an implementation/trampoline
    target - and sets up common flags and necessary macros. The implementation target exports
    the header target so that consumers need only depend on one. This wrapper is location-sensitive:
    It takes advantage of its definition's path to determine names and versions so as to make
    the code's path the Single Source of Truth for naming/versioning.

    Args:
      museum_deps: MUSEUM-VERSION-RELATIVE paths to depended-upon libraries. These will get
                   translated automatically into full Buck paths.
                   Example:
                     "bionic/libc" becomes "//deps/museum/<version>/bionic/libc:libc"

      exported_preprocessor_flags: Any extra preprocessor flags consuming libraries need. Only
                                   used by the headers target (but affects the implementation
                                   target as well, as it is a consuming library).

      compiler_flags: Any extra flags needed to compile the implementation code. Only used by the
                      implementation target.

      extra_visibility: Any targets that this library should be visible to beyond the Museum
                        defaults.

      exported_headers: Custom header exports. If not set, uses `glob(["**/*.h"])`. Only used by
                        the headers target.

      exported_platform_headers: Custom platform-specific header exports. Only used by the headers
                                 target.
    """

    path = get_base_path().split("/")
    if path[0] != "deps" or path[1] != "museum":
        fail("museum_library declaration at unexpected path: " + get_base_path())

    museum_version = path[2]
    name = path[-1]

    fb_xplat_cxx_library(
        name="headers",
        header_namespace="museum/" + "/".join(path[2:]),
        exported_headers=exported_headers if exported_headers else glob([
            "**/*.h",
        ]),
        exported_platform_headers=exported_platform_headers,
        exported_deps=[
            profilo_path("deps/museum/{}/{}:headers".format(museum_version, museum_dep)) for museum_dep in museum_deps
        ],
        exported_preprocessor_flags=[
            "-D__STDC_FORMAT_MACROS",
            "-DMUSEUM_VERSION=v" + museum_version.replace(".", "_"),
            "-DMUSEUM_VERSION_NUM=" + museum_version.replace(".", ""),
            "-DMUSEUM_VERSION_DOT=" + museum_version,
            "-DMUSEUM_VERSION_" + museum_version.replace(".", "_"),
            "-Wno-inconsistent-missing-override",
            "-Wno-unknown-attributes",
        ] + exported_preprocessor_flags,
        visibility=[
            profilo_path("..."),
        ] + extra_visibility,
    )

    fb_xplat_cxx_library(
        name=name,
        header_namespace="museum/{}/{}".format(museum_version, name),
        srcs=glob(["**/*.c", "**/*.cc", "**/*.cpp"]),
        compiler_flags=[
            "-std=c++11",
            "-fvisibility=hidden",
            "-fno-exceptions",
            "-fpack-struct=4",
            "-fno-rtti",
            "-g",
            "-Os",
            "-Wextra",
            "-Wno-unused-parameter",
            "-Wno-format-security",
            "-Wno-unused",
            "-Wno-missing-field-initializers",
            "-Wno-pmf-conversions",
            "-Wno-unknown-warning-option",
        ] + compiler_flags,
        deps=[
            profilo_path("deps/museum/{}/{}:{}".format(museum_version, museum_dep, museum_dep.split("/")[-1])) for museum_dep in museum_deps
        ] + [profilo_path("deps/museum:museum")],
        exported_deps=[
            ":headers",
        ],
        force_static=True,  # TODO: how to get rid of this?
        visibility=[
            profilo_path("deps/museum/..."),
        ] + extra_visibility,
    )
