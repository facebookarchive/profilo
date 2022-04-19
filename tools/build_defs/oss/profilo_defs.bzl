"""Provides OSS compatibile macros."""

load("//tools/build_defs/android:fb_xplat_android_cxx_library.bzl", "fb_xplat_android_cxx_library")

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
        scope = "compiled",
        deps = []):
    """
    Creates remote_file and prebuilt_jar rules for a maven artifact.
    """
    _ignore = scope
    remote_file_name = "{}-remote".format(name)
    native.remote_file(
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
            deps = deps,
        )
    else:
        native.android_prebuilt_aar(
            name = name,
            aar = ":{}".format(remote_file_name),
            visibility = visibility,
            deps = deps,
        )

def profilo_oss_xplat_cxx_library(**kwargs):
    fb_xplat_android_cxx_library(**kwargs)

def profilo_maybe_hidden_visibility():
    return ["-fvisibility=hidden"]
