#!/usr/bin/env bash
# =============================================================
# ci.sh — MyAutoSarAp 一键 CI 脚本
# 用法:
#   bash scripts/ci.sh            # 完整流程（构建+测试+覆盖率）
#   bash scripts/ci.sh build      # 仅构建 Release
#   bash scripts/ci.sh test       # 构建 Debug + 运行测试
#   bash scripts/ci.sh coverage   # 构建 Debug + 测试 + 覆盖率报告
# =============================================================
set -euo pipefail

# ── 路径 ──────────────────────────────────────────────────────
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_RELEASE="${ROOT}/build"
BUILD_DEBUG="${ROOT}/build_coverage"
REPORT_DIR="${ROOT}/coverage_report"
JOBS="$(sysctl -n hw.logicalcpu)"

# ── 工具 ──────────────────────────────────────────────────────
CXX_COMPILER="$(xcrun -f clang++)"
GTEST_DIR="/opt/homebrew/lib/cmake/GTest"
LCOV_IGNORE_ERRORS="inconsistent,unused,empty,format,mismatch,category"

# ── 颜色输出 ──────────────────────────────────────────────────
info()  { echo -e "\033[1;34m[CI]\033[0m $*"; }
ok()    { echo -e "\033[1;32m[OK]\033[0m $*"; }
fail()  { echo -e "\033[1;31m[FAIL]\033[0m $*" >&2; exit 1; }
step()  { echo ""; echo -e "\033[1;33m── $* ──\033[0m"; }

# =============================================================
# 阶段函数
# =============================================================

do_build_release() {
    step "Build (Release)"
    cmake -S "${ROOT}" -B "${BUILD_RELEASE}" -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_CXX_COMPILER="${CXX_COMPILER}" \
          -DCMAKE_OSX_ARCHITECTURES=arm64 \
          -DBUILD_TESTS=OFF \
          -DENABLE_COVERAGE=OFF \
          -DCMAKE_INSTALL_PREFIX="${BUILD_RELEASE}/install" \
          > /dev/null
    cmake --build "${BUILD_RELEASE}" --parallel "${JOBS}"
    ok "Release build complete → ${BUILD_RELEASE}/src/application/MyAutoSarAp"
}

do_build_debug() {
    step "Build (Debug + Coverage)"
    cmake -S "${ROOT}" -B "${BUILD_DEBUG}" -DCMAKE_BUILD_TYPE=Debug \
          -DCMAKE_CXX_COMPILER="${CXX_COMPILER}" \
          -DCMAKE_OSX_ARCHITECTURES=arm64 \
          -DGTest_DIR="${GTEST_DIR}" \
          -DBUILD_TESTS=ON \
          -DENABLE_COVERAGE=ON \
          > /dev/null
    cmake --build "${BUILD_DEBUG}" --parallel "${JOBS}"
    ok "Debug build complete"
}

do_test() {
    step "Unit Tests"
    # 清理旧 gcda，避免累积数据影响覆盖率
    find "${BUILD_DEBUG}" -name "*.gcda" -delete 2>/dev/null || true
    cd "${BUILD_DEBUG}"
    ctest --output-on-failure --parallel "${JOBS}"
    cd "${ROOT}"
    ok "All tests passed"
}

do_coverage() {
    step "Coverage Report (lcov)"

    local RAW="${BUILD_DEBUG}/coverage_raw.info"
    local FILTERED="${BUILD_DEBUG}/coverage.info"

    # 收集数据
    lcov --capture \
         --directory "${BUILD_DEBUG}" \
         --source-directory "${ROOT}" \
         --output-file "${RAW}" \
         --ignore-errors "${LCOV_IGNORE_ERRORS}" \
         --rc branch_coverage=0 \
         --quiet

    # 过滤第三方代码
    lcov --remove "${RAW}" \
         '*/googletest/*' '*/gtest/*' \
         '/usr/*' '/Library/*' '/opt/*' \
         '*/tests/unit/*' \
         --output-file "${FILTERED}" \
         --ignore-errors "${LCOV_IGNORE_ERRORS}" \
         --quiet

    # 生成 HTML
    rm -rf "${REPORT_DIR}"
    genhtml "${FILTERED}" \
            --output-directory "${REPORT_DIR}" \
            --title "MyAutoSarAp Coverage (AUTOSAR AP R25-11)" \
            --legend --show-details \
            --prefix "${ROOT}" \
            --ignore-errors "${LCOV_IGNORE_ERRORS},unsupported" \
            --quiet

    ok "Coverage report → ${REPORT_DIR}/index.html"
    # 打印摘要
    lcov --summary "${FILTERED}" 2>&1 | grep -E 'lines|functions' || true
}

# =============================================================
# 主流程
# =============================================================
MODE="${1:-all}"

info "MyAutoSarAp CI  |  mode=${MODE}  |  jobs=${JOBS}"
info "Root: ${ROOT}"

case "${MODE}" in
    build)
        do_build_release
        ;;
    test)
        do_build_debug
        do_test
        ;;
    coverage)
        do_build_debug
        do_test
        do_coverage
        ;;
    all|"")
        do_build_release
        do_build_debug
        do_test
        do_coverage
        ;;
    *)
        echo "Usage: $0 [build|test|coverage|all]"
        exit 1
        ;;
esac

echo ""
ok "CI finished ✓"
