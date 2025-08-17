set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# Build configuration
BUILD_TYPE="${BUILD_TYPE:-Release}"
BUILD_DIR="${BUILD_DIR:-${PROJECT_ROOT}/build}"
INSTALL_PREFIX="${INSTALL_PREFIX:-/opt/woved}"
PARALLEL_JOBS="${PARALLEL_JOBS:-$(nproc)}"

# Features
USE_IOURING="${USE_IOURING:-ON}"
USE_PMEM="${USE_PMEM:-OFF}"
ENABLE_LTO="${ENABLE_LTO:-ON}"
ENABLE_PGO="${ENABLE_PGO:-OFF}"
CPU_BASELINE="${CPU_BASELINE:-x86-64-v3}"
BUILD_TESTS="${BUILD_TESTS:-ON}"
BUILD_BENCH="${BUILD_BENCH:-ON}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --debug)
            BUILD_TYPE="Debug"
            ENABLE_LTO="OFF"
            shift
            ;;
        --release)
            BUILD_TYPE="Release"
            shift
            ;;
        --relwithdebinfo)
            BUILD_TYPE="RelWithDebInfo"
            shift
            ;;
        --clean)
            log_info "Cleaning build directory..."
            rm -rf "${BUILD_DIR}"
            shift
            ;;
        --pmem)
            USE_PMEM="ON"
            shift
            ;;
        --no-tests)
            BUILD_TESTS="OFF"
            shift
            ;;
        --no-bench)
            BUILD_BENCH="OFF"
            shift
            ;;
        --pgo)
            ENABLE_PGO="ON"
            shift
            ;;
        --prefix)
            INSTALL_PREFIX="$2"
            shift 2
            ;;
        --jobs|-j)
            PARALLEL_JOBS="$2"
            shift 2
            ;;
        --help)
            cat << EOF
Usage: $0 [OPTIONS]

Options:
    --debug             Build in debug mode
    --release           Build in release mode (default)
    --relwithdebinfo    Build with release optimizations and debug info
    --clean             Clean build directory before building
    --pmem              Enable persistent memory support
    --no-tests          Disable building tests
    --no-bench          Disable building benchmarks
    --pgo               Enable profile-guided optimization
    --prefix PATH       Installation prefix (default: /opt/woved)
    --jobs N, -j N      Number of parallel build jobs (default: nproc)
    --help              Show this help message

Environment variables:
    BUILD_DIR           Build directory (default: ./build)
    CC                  C compiler (default: auto-detect)
    CXX                 C++ compiler (default: auto-detect)
EOF
            exit 0
            ;;
        *)
            log_error "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Check dependencies
log_info "Checking build dependencies..."

check_command() {
    if ! command -v "$1" &> /dev/null; then
        log_error "$1 is not installed"
        exit 1
    fi
}

check_command cmake
check_command ninja

# Detect compiler
if [ -z "${CC:-}" ] || [ -z "${CXX:-}" ]; then
    if command -v clang-17 &> /dev/null; then
        export CC=clang-17
        export CXX=clang++-17
        log_info "Using Clang 17"
    elif command -v clang &> /dev/null; then
        export CC=clang
        export CXX=clang++
        CLANG_VERSION=$(clang --version | grep -oP 'version \K[0-9]+' | head -1)
        if [[ ${CLANG_VERSION} -lt 14 ]]; then
            log_warn "Clang version ${CLANG_VERSION} detected. Version 14+ recommended."
        fi
    elif command -v gcc &> /dev/null; then
        export CC=gcc
        export CXX=g++
        GCC_VERSION=$(gcc -dumpversion | cut -d. -f1)
        if [[ ${GCC_VERSION} -lt 11 ]]; then
            log_warn "GCC version ${GCC_VERSION} detected. Version 11+ recommended."
        fi
    else
        log_error "No suitable C++ compiler found"
        exit 1
    fi
fi

# Check for io_uring support
if [ "${USE_IOURING}" == "ON" ]; then
    if ! pkg-config --exists liburing; then
        log_warn "liburing not found, disabling io_uring support"
        USE_IOURING="OFF"
    fi
fi

# Create build directory
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# Generate FlatBuffers schemas before CMake
log_info "Generating FlatBuffers schemas..."
if [ -d "${PROJECT_ROOT}/schemas" ]; then
    for fbs_file in "${PROJECT_ROOT}"/schemas/*.fbs; do
        if [ -f "$fbs_file" ]; then
            flatc --cpp -o "${PROJECT_ROOT}/src/cpp/generated" "$fbs_file" || true
        fi
    done
fi

# Configure with CMake
log_info "Configuring build with CMake..."
log_info "Build type: ${BUILD_TYPE}"
log_info "Build directory: ${BUILD_DIR}"
log_info "Install prefix: ${INSTALL_PREFIX}"

cmake_args=(
    -G Ninja
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
    -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}"
    -DWOVED_USE_IOURING="${USE_IOURING}"
    -DWOVED_USE_PMEM="${USE_PMEM}"
    -DWOVED_ENABLE_LTO="${ENABLE_LTO}"
    -DWOVED_ENABLE_PGO="${ENABLE_PGO}"
    -DWOVED_CPU_BASELINE="${CPU_BASELINE}"
    -DWOVED_BUILD_TESTS="${BUILD_TESTS}"
    -DWOVED_BUILD_BENCH="${BUILD_BENCH}"
)

# Add compiler flags for release builds
if [ "${BUILD_TYPE}" == "Release" ] || [ "${BUILD_TYPE}" == "RelWithDebInfo" ]; then
    cmake_args+=(
        -DCMAKE_CXX_FLAGS="-march=native -mtune=native"
    )
fi

cmake "${cmake_args[@]}" "${PROJECT_ROOT}"

# Build
log_info "Building with ${PARALLEL_JOBS} parallel jobs..."
ninja -j "${PARALLEL_JOBS}"

# Run basic smoke test if tests are built
if [[ "${BUILD_TESTS}" == "ON" ]]; then
    log_info "Running basic smoke tests..."
    if [ -f "./tests/unit/test_woved" ]; then
        ./tests/unit/test_woved --gtest_filter="*Smoke*" || true
    fi
fi

log_info "Build completed successfully!"
log_info "Binary location: ${BUILD_DIR}/wovedd"
log_info "To install: ninja -C ${BUILD_DIR} install"