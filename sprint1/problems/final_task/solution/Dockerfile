FROM gcc:11.3 AS build

RUN apt update && apt install -y \
    python3-pip \
    git \
    wget \
    build-essential \
    && rm -rf /var/lib/apt/lists/*

RUN wget -O cmake.tar.gz https://github.com/Kitware/CMake/releases/download/v3.28.3/cmake-3.28.3-linux-x86_64.tar.gz && \
    tar -xzf cmake.tar.gz --strip-components=1 -C /usr/local && \
    rm cmake.tar.gz

RUN pip3 install "conan==1.*"

WORKDIR /app
COPY conanfile.txt .
COPY CMakeLists.txt .
COPY ./src src

RUN mkdir -p build && cd build && \
    conan install .. --build=missing && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build .

FROM ubuntu:22.04 AS run

RUN groupadd -r www && useradd -r -g www www

COPY --from=build /app/build/bin/game_server /app/
COPY ./data /app/data

RUN chown -R www:www /app

USER www
WORKDIR /app

ENTRYPOINT ["/app/game_server", "/app/data/config.json"]
