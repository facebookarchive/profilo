load("@fbsource//tools/build_defs:lazy.bzl", "lazy")

"""Provides OSS compatibility layer with internal FB macros."""

def fb_xplat_android_cxx_library(name, **kwargs):
    """Compatibility adapter for internal FB cxx_library macros."""

    # Prune values specific to fb code
    if "allow_jni_merging" in kwargs:
        kwargs.pop("allow_jni_merging")

    # We don't need asset packing in the OSS build
    if "can_be_asset" in kwargs:
        kwargs.pop("can_be_asset")

    # Add standard if none selected
    compiler_flags = kwargs.get("compiler_flags", [])
    has_std = lazy.is_any(lambda opt: opt.startswith("-std"), compiler_flags)
    if not has_std:
        compiler_flags.append("-std=c++14")
    kwargs["compiler_flags"] = compiler_flags

    return native.cxx_library(name = name, **kwargs)
