set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${PROJECT_ROOT}/build}"

# Test parameters
DATA_DIR="/tmp/woved_test"
KILL_POINTS=("wal_append" "segment_flush" "compaction_merge" "buffer_drain")
ITERATIONS="${ITERATIONS:-10}"

log_info() {
    echo "[INFO] $1"
}

log_error() {
    echo "[ERROR] $1"
}

# Setup test environment
setup_test() {
    rm -rf "${DATA_DIR}"
    mkdir -p "${DATA_DIR}"
    
    # Start wovedd with fault injection enabled
    WOVED_FAULT_INJECTION=1 \
    WOVED_DATA_DIR="${DATA_DIR}" \
    "${BUILD_DIR}/wovedd" \
        --config "${PROJECT_ROOT}/configs/woved-dev.yaml" &
    
    WOVED_PID=$!
    sleep 2
}

# Kill at specific point
inject_fault() {
    local kill_point=$1
    log_info "Injecting fault at: ${kill_point}"
    
    # Send signal to trigger fault
    kill -USR1 ${WOVED_PID} 2>/dev/null || true
    
    # Force kill
    sleep 0.5
    kill -9 ${WOVED_PID} 2>/dev/null || true
    
    wait ${WOVED_PID} 2>/dev/null || true
}

# Verify recovery
verify_recovery() {
    log_info "Verifying recovery..."
    
    # Start server again
    WOVED_RECOVERY_CHECK=1 \
    WOVED_DATA_DIR="${DATA_DIR}" \
    timeout 30 "${BUILD_DIR}/wovedd" \
        --config "${PROJECT_ROOT}/configs/woved-dev.yaml" \
        --recovery-only
    
    if [[ $? -eq 0 ]]; then
        log_info "Recovery successful!"
        return 0
    else
        log_error "Recovery failed!"
        return 1
    fi
}

# Run fault injection tests
for kill_point in "${KILL_POINTS[@]}"; do
    for i in $(seq 1 ${ITERATIONS}); do
        log_info "Test iteration ${i}/${ITERATIONS} for ${kill_point}"
        
        setup_test
        
        # Insert some data
        "${BUILD_DIR}/tools/woved-bench/woved-bench" \
            --mode insert \
            --count 1000 \
            --batch 100 &
        
        BENCH_PID=$!
        
        # Wait random time then inject fault
        sleep $((RANDOM % 3 + 1))
        inject_fault "${kill_point}"
        
        kill ${BENCH_PID} 2>/dev/null || true
        
        # Verify recovery
        if ! verify_recovery; then
            log_error "Recovery failed at ${kill_point}, iteration ${i}"
            exit 1
        fi
    done
done

log_info "All fault injection tests passed!"