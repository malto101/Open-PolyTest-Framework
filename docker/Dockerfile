FROM rust:1.83-bookworm

RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake \
    gcc \
    g++ \
    qemu-system-arm \
    gcc-arm-none-eabi \
    python3 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .
RUN python3 scripts/amalgamate.py \
 && cargo build -p polyontest \
 && cmake -S examples/host_c -B build/host_c -DPOLYONTEST_MINIMAL_PRINT=OFF \
 && cmake --build build/host_c

CMD ["cargo", "run", "-p", "polyontest", "--", "run", "--target", "host", "--config", "examples/host_c/polyontest.toml"]
