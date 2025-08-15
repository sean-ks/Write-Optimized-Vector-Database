set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${PROJECT_ROOT}/build}"

# Test configuration
TEST_TYPE="${TEST_TYPE:-all}"
TEST_FILTER="${TEST_FILTER:-*}"
ENABLE_ASAN="${ENABLE_ASAN:-OFF}"
ENABLE_UBSAN="${ENABLE_UBSAN:-OFF}"
FAULT_INJECTION="${FAULT_INJECTION:-OFF}"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --unit)
            TEST_TYPE="unit"
            shift
            ;;
        --integration)
            TEST_TYPE="integration"
            shift
            ;;
        --bench)
            TEST_TYPE="bench"
            shift
            ;;
        --fault-injection)
            FAULT_INJECTION="ON"
            shift
            ;;
        --asan)
            ENABLE_ASAN="ON"
            shift
            ;;
        --filter)
            TEST_FILTER="$2"
            shift 2
            ;;
        *)
            log_error "Unknown option: $1"
            exit 1
            ;;
    esac
done

cd "${BUILD_DIR}"

# Run different test types
case ${TEST_TYPE} in
    unit)
        log_info "Running unit tests..."
        ./tests/unit/test_woved --gtest_filter="${TEST_FILTER}"
        ;;
    integration)
        log_info "Running integration tests..."
        ./tests/integration/test_integration --gtest_filter="${TEST_FILTER}"
        ;;
    bench)
        log_info "Running benchmarks..."
        ./tests/bench/bench_woved --benchmark_filter="${TEST_FILTER}"
        ;;
    fault-injection)
        log_info "Running fault injection tests..."
        "${SCRIPT_DIR}/fault-inject.sh"
        ;;
    all)
        log_info "Running all tests..."
        ctest --output-on-failure
        ;;
esac

log_info "Tests completed!"

