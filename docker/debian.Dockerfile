# Test anylmdb on Debian (glibc). Everything builds from the vendored
# sources in the repo — no network access needed beyond apt.
FROM debian:stable-slim
RUN apt-get update \
    && apt-get install -y --no-install-recommends build-essential \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
RUN make clean && make test && make test-dropin
RUN make test-asan
