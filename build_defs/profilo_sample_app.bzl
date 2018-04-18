"""
Provides some helpers for defining Profilo sample apps.

profilo_sample_app - Helper wrapper for defining a sample app with a given set of providers

PROVIDER_TO_RULE - Map that links provider "short names" to the provider class and dependency.
                   Can be extended in individual BUCK files to add more providers specific to
                   those files.
"""

load("//buck_imports:profilo_path.bzl", "profilo_path")


PROVIDER_TO_RULE = {
    "atrace": profilo_path("java/main/com/facebook/profilo/provider/atrace:atrace"),
    "mappingdensity": profilo_path("java/main/com/facebook/profilo/provider/mappingdensity:mappingdensity"),
    "processmetadata": profilo_path("java/main/com/facebook/profilo/provider/processmetadata:processmetadata"),
    "stacktrace": profilo_path("java/main/com/facebook/profilo/provider/stacktrace:stacktrace"),
    "systemcounters": profilo_path("java/main/com/facebook/profilo/provider/systemcounters:systemcounters"),
    "threadmetadata": profilo_path("java/main/com/facebook/profilo/provider/threadmetadata:threadmetadata"),
    "yarn": profilo_path("java/main/com/facebook/profilo/provider/yarn:yarn"),
}


def profilo_sample_app(srcs, manifest, providers, deps=[]):
    """
    Defines a sample app based on a list of provider short names.

    Creates AAR and APK rules named `sample-[aar-]{providers[0]}-{providers[1]}-{etc}`.
    If providers is empty or None, creates `sample-[aar-]-none`.

    Args:
        srcs:          Top-level java files. Should include, at least, a concrete
                       implementation of BaseSampleAppMainActivity.
        manifest:      The manifest file, or target, to use for your sample app.
        providers:     A list of valid entry keys in PROVIDER_TO_RULE, or None.
        deps:          Any extra dependencies your java files use. Provider dependencies, as well
                       as `profilo_path("java/main/com/facebook/profilo/core:core")` (for
                       `TraceOrchestrator.TraceProvider`), will be automatically added and do not
                       need to be listed.
    """
    if providers:
        providers = sorted(providers)
        providers_string = "-".join(providers)
        provider_deps = [PROVIDER_TO_RULE[provider] for provider in providers]
    else:
        providers_string = "none"
        provider_deps = []

    android_build_config(
        name="sample-buildconfig-{}".format(providers_string),
        package="com.facebook.profilo",
        values=[
            "String[] PROVIDERS = new String[] {{ {} }}".format(
                ",".join(["\"{}\"".format(provider) for provider in (providers if providers else [])])
            ),
        ],
    )

    # We want all providers on the compile path but only package the ones that
    # the current config enables. However, the provided_deps take precedence
    # and targets specified in both provided and normal deps will be elided.
    #
    # Therefore, calculate the difference between the full set and the enabled
    # set and put only that in the provided list.

    provided_deps = [x for x in PROVIDER_TO_RULE.values()
                     if x not in provider_deps]

    android_library(
        name="sample-activity-{}".format(providers_string),
        srcs=srcs,
        provided_deps=provided_deps,
        visibility=[
            profilo_path("..."),
        ],
        deps=[
            profilo_path("java/main/com/facebook/profilo/core:core"),
            profilo_path("java/main/com/facebook/profilo/sample:sample-lib"),
        ] + provider_deps + deps,
    )

    android_aar(
        name="sample-aar-{}".format(providers_string),
        manifest_skeleton=manifest,
        deps=[
            ":sample-buildconfig-{}".format(providers_string),
            ":sample-activity-{}".format(providers_string),
        ],
        visibility=[
            "PUBLIC",
        ],
    )

    android_binary(
        name="sample-{}".format(providers_string),
        keystore=profilo_path("java/main/com/facebook/profilo/sample:keystore"),
        manifest=manifest,
        deps=[
            ":sample-buildconfig-{}".format(providers_string),
            ":sample-activity-{}".format(providers_string),
        ],
        aapt_mode="aapt2",
    )
