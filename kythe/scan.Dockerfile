FROM gcr.io/kythe-public/bazel-extractor

# See https://github.com/kythe/kythe/tree/master/kythe/extractors/bazel
# for the setup we're trying to cache.
# `pkg-config zip g++ zlib1g-dev unzip python` are needed by bazel (and
# probably already present).

RUN apt-get update && \
    apt-get install -y $KYTHE_SYSTEM_DEPS libglfw3-dev \
                       wget pkg-config zip g++ zlib1g-dev unzip python && \
    apt-get clean

# Replace bazelisk with a specific bazel release for more reproducibility.
RUN wget https://github.com/bazelbuild/bazel/releases/download/0.26.1/bazel-0.26.1-installer-linux-x86_64.sh

RUN echo "488d6b7223a1b3db965b8831c562aba61229a690aad38d75cd8f43012fc04fe4  bazel-0.26.1-installer-linux-x86_64.sh" | sha256sum -c -

RUN chmod +x bazel-0.26.1-installer-linux-x86_64.sh; ./bazel-0.26.1-installer-linux-x86_64.sh

# The entrypoint script uses bazelisk, but we point it at our real bazel.
RUN rm /kythe/bazelisk && ln -s `which bazel` /kythe/bazelisk

RUN echo "startup --output_user_root=/cache/bazel" >> /kythe/bazelrc

COPY scan-wrapper.sh /scan-wrapper.sh

ENTRYPOINT ["/scan-wrapper.sh"]
