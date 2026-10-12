FROM gcc:11.3 AS build

# Используем архивные репозитории Debian Bullseye.
# Отключаем проверку срока актуальности архивного индекса,
# но сохраняем проверку подписей пакетов и репозиториев.
RUN set -eux; \
    rm -f /etc/apt/sources.list.d/*.list \
          /etc/apt/sources.list.d/*.sources; \
    printf '%s\n' \
        'deb [check-valid-until=no] http://archive.debian.org/debian bullseye main' \
        'deb [check-valid-until=no] http://archive.debian.org/debian-security bullseye-security main' \
        > /etc/apt/sources.list; \
    rm -rf /var/lib/apt/lists/*; \
    apt-get -o Acquire::Retries=3 update; \
    apt-get -o Acquire::Retries=3 install -y --no-install-recommends \
        ca-certificates \
        python3-pip \
        cmake; \
    python3 -m pip install --no-cache-dir conan==1.66.0; \
    conan --version; \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY conanfile.txt /app/

RUN mkdir -p /app/build && \
    cd /app/build && \
    conan install .. \
        --build=missing \
        -s compiler.libcxx=libstdc++11 \
        -s build_type=Release

COPY ./src /app/src
COPY ./tests /app/tests
COPY CMakeLists.txt /app/

RUN cd /app/build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build . --parallel 2

ENTRYPOINT ["/app/build/collision_detection_tests"]
