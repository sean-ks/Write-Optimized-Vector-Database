# Build stage
FROM ubuntu:22.04 AS builder

# Install build dependencies
RUN apt-get update && apt-get install -y \
    clang-17 \
    lld-17 \
    cmake \
    ninja-build \
    git \
    pkg-config \
    liburing-dev \
    libnuma-dev \
    libhwloc-dev \
    libomp-dev \
    libprotobuf-dev \
    protobuf-compiler \
    libgrpc++-dev \
    libflatbuffers-dev \
    flatbuffers-compiler \
    libgtest-dev \
    libbenchmark-dev \
    libfaiss-dev \
    libroaring-dev \
    liblz4-dev \
    libzstd-dev \
    libprometheus-cpp-dev \
    libspdlog-dev \
    libfmt-dev \
    libxxhash-dev \
    && rm -rf /var/lib/apt/lists/*

# Optional: Install PMDK if using persistent memory
ARG USE_PMEM=OFF
RUN if [ "$USE_PMEM" = "ON" ]; then \
    apt-get update && apt-get install -y libpmem-dev libpmem2-dev \
    && rm -rf /var/lib/apt/lists/*; \
    fi

# Set up build environment
ENV CC=clang-17
ENV CXX=clang++-17
ENV LD=lld-17

# Copy source code
WORKDIR /build
COPY . .

# Build WOVeD
RUN cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DWOVED_USE_IOURING=ON \
    -DWOVED_USE_PMEM=${USE_PMEM} \
    -DWOVED_ENABLE_LTO=ON \
    -DWOVED_CPU_BASELINE=x86-64-v3 \
    -DWOVED_BUILD_TESTS=OFF \
    -DWOVED_BUILD_BENCH=OFF \
    && ninja -C build -j$(nproc) \
    && ninja -C build install DESTDIR=/install

# Runtime stage
FROM ubuntu:22.04

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    liburing2 \
    libnuma1 \
    libhwloc15 \
    libomp5 \
    libprotobuf23 \
    libgrpc++1 \
    libfaiss \
    libroaring0 \
    liblz4-1 \
    libzstd1 \
    libspdlog1 \
    libfmt8 \
    libxxhash0 \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Create woved user
RUN groupadd -r woved && useradd -r -g woved woved

# Copy installed files
COPY --from=builder /install/opt/woved /opt/woved

# Create data directories
RUN mkdir -p /var/lib/woved/wal \
                /var/lib/woved/segments \
                /var/lib/woved/manifest \
                /var/log/woved \
                /etc/woved \
    && chown -R woved:woved /var/lib/woved /var/log/woved

# Copy default config
COPY configs/woved-default.yaml /etc/woved/woved.yaml
RUN chown woved:woved /etc/woved/woved.yaml

# Set up environment
ENV PATH="/opt/woved/bin:${PATH}"
ENV LD_LIBRARY_PATH="/opt/woved/lib:${LD_LIBRARY_PATH}"

# Expose ports
EXPOSE 8080 9090 9091

# Health check
HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
    CMD curl -f http://localhost:8080/healthz || exit 1

# Run as woved user
USER woved
WORKDIR /var/lib/woved

# Default command
ENTRYPOINT ["/opt/woved/bin/wovedd"]
CMD ["--config", "/etc/woved/woved.yaml"]