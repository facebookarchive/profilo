#!/bin/bash

set -e

mkdir -p /opt/android-{sdk,ndk}

cd /opt/android-ndk/
wget -q https://dl.google.com/android/repository/android-ndk-r15c-linux-x86_64.zip
unzip -q -d . android-ndk-r15c-linux-x86_64.zip

cd /opt/android-sdk/
wget -q https://dl.google.com/android/repository/sdk-tools-linux-3859397.zip
unzip -q -d . sdk-tools-linux-3859397.zip

yes | tools/bin/sdkmanager --licenses
tools/bin/sdkmanager 'platforms;android-23'
tools/bin/sdkmanager 'build-tools;27.0.1'
