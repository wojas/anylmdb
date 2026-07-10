# Test anylmdb on Alpine (musl). ASan is not usable on musl, so only the
# regular suite and the drop-in check run here.
FROM alpine:latest
RUN apk add --no-cache build-base
WORKDIR /src
COPY . .
RUN make clean && make test && make test-dropin
