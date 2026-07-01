FROM rust:slim-bullseye AS chef
RUN apt-get update && apt-get install -y clang lld pkg-config curl tar
RUN cargo install cargo-chef
WORKDIR /app

FROM chef AS planner
COPY . .
RUN cargo chef prepare --recipe-path recipe.json

FROM chef AS builder
ARG MINIFI_SDK_PATH
ENV MINIFI_SDK_PATH=${MINIFI_SDK_PATH}

COPY --from=planner /app/recipe.json recipe.json

# Conditionally copy the local SDK files from the target directory
COPY target/.docker_sd[k] /app/target/.docker_sdk/
COPY target/.docker_sdk.zi[p] /app/target/

RUN cargo chef cook --release --recipe-path recipe.json
COPY . .
RUN cargo build --release

# Export Stage
FROM scratch AS bin-export
COPY --from=builder /app/target/release/libminifi_*.so /
