FROM ubuntu:16.04
RUN apt-get update
RUN apt-get install -y --no-install-recommends ant git default-jdk python wget unzip

RUN mkdir -p /scripts/

COPY build/setup_android.sh /scripts/
RUN bash /scripts/setup_android.sh

COPY build/setup_buck.sh /scripts/
RUN bash /scripts/setup_buck.sh

ENV PATH="/buck/bin:${PATH}"
ENV ANDROID_SDK="/opt/android-sdk"
ENV ANDROID_NDK_REPOSITORY="/opt/android-ndk"

RUN mkdir -p /repo
COPY . /repo/

WORKDIR /repo
