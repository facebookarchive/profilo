"""Provides OSS compatibility layer with internal FB macros."""

def fb_java_library(**kwargs):
    """Delegates to the native java_library rule."""
    native.java_library(**kwargs)
