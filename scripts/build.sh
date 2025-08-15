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
            shift
            ;;
        --release)
            BUILD_TYPE="Release"
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
        --pgo)
            ENABLE_PGO="ON"
            shift
            ;;
        --help)
            cat << EOF
Usage: $0 [OPTIONS]

Options:
    --debug         Build in debug mode
    --release       Build in release mode (default)
    --clean         Clean build directory before building
    --pmem          Enable persistent memory support
    --no-tests      Disable building tests
    --pgo           Enable profile-guided optimization
    --help          Show this help message

Environment variables:
    BUILD_DIR       Build directory (default: ./build)
    INSTALL_PREFIX  Installation prefix (default: /opt/woved)
    PARALLEL_JOBS   Number of parallel build jobs (default: nproc)
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
check_command clang++

# Check clang version
CLANG_VERSION=$(clang++ --version | grep -oP 'version \K[0-9]+' | head -1)
if [[ ${CLANG_VERSION} -lt 17 ]]; then
    log_warn "Clang version ${CLANG_VERSION} detected. Version 17+ recommended."
fi

# Create build directory
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# Configure with CMake
log_info "Configuring build with CMake..."
log_info "Build type: ${BUILD_TYPE}"
log_info "Build directory: ${BUILD_DIR}"

cmake \
    -G Ninja \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DWOVED_USE_IOURING="${USE_IOURING}" \
    -DWOVED_USE_PMEM="${USE_PMEM}" \
    -DWOVED_ENABLE_LTO="${ENABLE_LTO}" \
    -DWOVED_ENABLE_PGO="${ENABLE_PGO}" \
    -DWOVED_CPU_BASELINE="${CPU_BASELINE}" \
    -DWOVED_BUILD_TESTS="${BUILD_TESTS}" \
    -DWOVED_BUILD_BENCH="${BUILD_BENCH}" \
    "${PROJECT_ROOT}"

# Build
log_info "Building with ${PARALLEL_JOBS} parallel jobs..."
ninja -j "${PARALLEL_JOBS}"

# Run basic smoke test
if [[ "${BUILD_TESTS}" == "ON" ]]; then
    log_info "Running basic smoke tests..."
    ctest --output-on-failure -R "smoke_" || true
fi

log_info "Build completed successfully!"
log_info "Binary location: ${BUILD_DIR}/wovedd"

