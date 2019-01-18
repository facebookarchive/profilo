load("//tools/build_defs/android:fb_xplat_cxx_library.bzl", "fb_xplat_cxx_library")
load("//tools/build_defs/oss:profilo_defs.bzl", "profilo_path")

def unwindc_tracer_library(version):
    version_num = version.replace(".", "")
    android_version = "android_{}".format(version_num)

    fb_xplat_cxx_library(
        name = "unwindc-tracer-{}".format(version),
        headers = [
            "ArtUnwindcTracer.h",
            "unwindc/runtime.h",
        ],
        header_namespace = "profiler",
        exported_headers = {
            "ArtUnwindcTracer_{}.h".format(version_num): "ArtUnwindcTracer.h",
        },
        compiler_flags = [
            "-fexceptions",
            "-std=gnu++14",
            '-DLOG_TAG="Profilo/Unwindc"',
            "-O3",
            "-Wno-self-assign",
            "-Wno-parentheses-equality",
            "-Wno-unused-variable",
        ],
        exported_preprocessor_flags = [
            "-DANDROID_VERSION_{}".format(version_num),
        ],
        force_static = True,
        platform_headers = [
            (".*x86.*", {
                "unwindc/unwinder.h": "unwindc/{}/x86/unwinder.h".format(android_version),
            }),
            (".*arm.*", {
                "unwindc/unwinder.h": "unwindc/{}/arm/unwinder.h".format(android_version),
            }),
        ],
        srcs = [
            "ArtUnwindcTracer.cpp",
        ],
        preprocessor_flags = [
            "-DANDROID_NAMESPACE=android_{}".format(version_num),
            "-DANDROID_VERSION_NUM={}".format(version_num),
        ],
        soname = "libprofiloprofilerunwindc{version_num}.$(ext)".format(version_num = version_num),
        visibility = [
            profilo_path("..."),
        ],
        deps = [
            profilo_path("deps/fb:fb"),
            profilo_path("cpp/logger:logger"),
            profilo_path("cpp/profiler:base_tracer"),
        ],
    )
