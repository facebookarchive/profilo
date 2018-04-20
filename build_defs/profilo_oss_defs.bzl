"""Provides OSS compatibile macros."""
load("//build_defs:fb_xplat_cxx_library.bzl", "fb_xplat_cxx_library")

IS_OSS_BUILD = True


def profilo_oss_android_library(**kwargs):
    """Delegates to the native android_library rule."""
    native.android_library(**kwargs)


def profilo_oss_cxx_library(**kwargs):
    """Delegates to the native cxx_library rule."""
    native.cxx_library(**kwargs)


def profilo_oss_java_library(**kwargs):
    """Delegates to the native java_library rule."""
    native.java_library(**kwargs)


def profilo_oss_maven_library(
        name,
        group,
        artifact,
        version,
        sha1,
        visibility,
        packaging='jar',
        scope='compiled'):
    """
    Creates remote_file and prebuilt_jar rules for a maven artifact.
    """
    remote_file_name = '{}-remote'.format(name)
    remote_file(
        name=remote_file_name,
        out='{}-{}.{}'.format(name, version, packaging),
        url=':'.join(['mvn', group, artifact, packaging, version]),
        sha1=sha1
    )

    if packaging == 'jar':
        native.prebuilt_jar(
            name=name,
            binary_jar=':{}'.format(remote_file_name),
            visibility=visibility
        )
    else:
        native.android_prebuilt_aar(
            name=name,
            aar=':{}'.format(remote_file_name),
            visibility=visibility
        )


def profilo_oss_xplat_cxx_library(**kwargs):
    fb_xplat_cxx_library(**kwargs)
