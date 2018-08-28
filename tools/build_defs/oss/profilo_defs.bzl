"""Provides OSS compatibile macros."""

load("//build_defs:glob_defs.bzl", "subdir_glob")
load("//build_defs:fb_xplat_cxx_library.bzl", "fb_xplat_cxx_library")

def profilo_path(dep):
    return "//" + dep

def profilo_oss_android_library(**kwargs):
    """Delegates to the native android_library rule."""
    native.android_library(**kwargs)

def profilo_oss_cxx_library(**kwargs):
    """Delegates to the native cxx_library rule."""
    native.cxx_library(**kwargs)

def profilo_oss_java_library(**kwargs):
    """Delegates to the native java_library rule."""
    native.java_library(**kwargs)

def profilo_oss_only_java_library(**kwargs):
    profilo_oss_java_library(**kwargs)

def profilo_oss_maven_library(
        name,
        group,
        artifact,
        version,
        sha1,
        visibility,
        packaging = "jar",
        scope = "compiled"):
    """
    Creates remote_file and prebuilt_jar rules for a maven artifact.
    """
    _ignore = scope
    remote_file_name = "{}-remote".format(name)
    remote_file(
        name = remote_file_name,
        out = "{}-{}.{}".format(name, version, packaging),
        url = ":".join(["mvn", group, artifact, packaging, version]),
        sha1 = sha1,
    )

    if packaging == "jar":
        native.prebuilt_jar(
            name = name,
            binary_jar = ":{}".format(remote_file_name),
            visibility = visibility,
        )
    else:
        native.android_prebuilt_aar(
            name = name,
            aar = ":{}".format(remote_file_name),
            visibility = visibility,
        )

def profilo_oss_xplat_cxx_library(**kwargs):
    fb_xplat_cxx_library(**kwargs)

def setup_profilo_oss_xplat_cxx_library():
    profilo_oss_xplat_cxx_library(
        name = "fbjni",
        srcs = glob([
            "cxx/fbjni/**/*.cpp",
        ]),
        header_namespace = "",
        exported_headers = subdir_glob([
            ("cxx", "fbjni/**/*.h"),
        ]),
        compiler_flags = [
            "-fexceptions",
            "-fno-omit-frame-pointer",
            "-frtti",
            "-ffunction-sections",
        ],
        exported_platform_headers = [
            (
                "^(?!android-arm$).*$",
                subdir_glob([
                    ("cxx", "lyra/**/*.h"),
                ]),
            ),
        ],
        exported_platform_linker_flags = [
            (
                "^android",
                ["-llog"],
            ),
            # There is a bug in ndk that would make linking fail for android log
            # lib. This is a workaround for older ndk, because newer ndk would
            # use a linker without bug.
            # See https://github.com/android-ndk/ndk/issues/556
            (
                "^android-arm64",
                ["-fuse-ld=gold"],
            ),
            (
                "^android-x86",
                ["-latomic"],
            ),
        ],
        platform_srcs = [
            (
                "^(?!android-arm$).*$",
                glob([
                    "cxx/lyra/*.cpp",
                ]),
            ),
        ],
        soname = "libfbjni.$(ext)",
        visibility = [
            "PUBLIC",
        ],
    )

def setup_profilo_oss_cxx_library():
    profilo_oss_cxx_library(
        name = "procmaps",
        srcs = [
            "procmaps.c",
        ],
        header_namespace = "",
        exported_headers = subdir_glob([
            ("", "*.h"),
        ]),
        compiler_flags = [
            "-std=gnu99",
        ],
        force_static = True,
        visibility = [
            "PUBLIC",
        ],
    )

def museum_library(
        museum_deps = [],
        exported_preprocessor_flags = [],
        compiler_flags = [],
        extra_visibility = [],
        exported_headers = None,
        exported_platform_headers = None):
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

      exported_headers: Custom header exports. If not set, uses `native.glob(["**/*.h"])`. Only used by
                        the headers target.

      exported_platform_headers: Custom platform-specific header exports. Only used by the headers
                                 target.
    """

    path = native.package_name().split("/")
    if path[0] != "deps" or path[1] != "museum":
        fail("museum_library declaration at unexpected path: " + native.package_name())

    museum_version = path[2]
    name = path[-1]

    fb_xplat_cxx_library(
        name = "headers",
        header_namespace = "museum/" + "/".join(path[2:]),
        exported_headers = exported_headers if exported_headers else native.glob([
            "**/*.h",
        ]),
        exported_platform_headers = exported_platform_headers,
        exported_deps = [
            profilo_path("deps/museum/{}/{}:headers".format(museum_version, museum_dep))
            for museum_dep in museum_deps
        ],
        exported_preprocessor_flags = [
            "-D__STDC_FORMAT_MACROS",
            "-DMUSEUM_VERSION=v" + museum_version.replace(".", "_"),
            "-DMUSEUM_VERSION_NUM=" + museum_version.replace(".", ""),
            "-DMUSEUM_VERSION_DOT=" + museum_version,
            "-DMUSEUM_VERSION_" + museum_version.replace(".", "_"),
            "-Wno-inconsistent-missing-override",
            "-Wno-unknown-attributes",
        ] + exported_preprocessor_flags,
        visibility = [
            profilo_path("..."),
        ] + extra_visibility,
    )

    fb_xplat_cxx_library(
        name = name,
        header_namespace = "museum/{}/{}".format(museum_version, name),
        srcs = native.glob(["**/*.c", "**/*.cc", "**/*.cpp"]),
        compiler_flags = [
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
        deps = [
            profilo_path("deps/museum/{}/{}:{}".format(museum_version, museum_dep, museum_dep.split("/")[-1]))
            for museum_dep in museum_deps
        ] + [profilo_path("deps/museum:museum")],
        exported_deps = [
            ":headers",
        ],
        force_static = True,  # TODO: how to get rid of this?
        visibility = [
            profilo_path("deps/museum/..."),
        ] + extra_visibility,
    )
