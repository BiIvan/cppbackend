FROM ubuntu:24.04

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        ninja-build \
        libboost-all-dev \
        libpqxx-dev \
        pkg-config \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

COPY CMakeLists.txt ./
COPY src/ ./src/

RUN cmake -S . -B /build \
        -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_TESTING=OFF \
    && cmake --build /build --target game_server --parallel 2 \
    && install -D /build/bin/game_server /app/game_server

COPY data/ /app/data/
COPY static/ /app/static/

WORKDIR /app

EXPOSE 8080

ENTRYPOINT ["/app/game_server"]
CMD ["--config-file", "/app/data/config.json", "--www-root", "/app/static/"]
