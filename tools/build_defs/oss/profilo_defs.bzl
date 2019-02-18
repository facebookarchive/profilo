"""Provides OSS compatibile macros."""

load("//tools/build_defs/android:fb_xplat_cxx_library.bzl", "fb_xplat_cxx_library")
load("//tools/build_defs:glob_defs.bzl", "subdir_glob")

def profilo_path(dep):
    return "//" + dep

def profilo_import_path(dep):
    return dep

def profilo_cxx_binary(**kwargs):
    """Delegates to the native cxx_test rule."""
    native.cxx_binary(**kwargs)

def profilo_cxx_test(**kwargs):
    """Delegates to the native cxx_test rule."""
    native.cxx_test(**kwargs)

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
        sha1 = sha1,
        url = ":".join(["mvn", group, artifact, packaging, version]),
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

def profilo_maybe_hidden_visibility():
    return ["-fvisibility=hidden"]

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
