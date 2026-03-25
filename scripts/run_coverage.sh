#!/usr/bin/env bash
# scripts/run_coverage.sh
# 构建 + 跑测试 + 生成 lcov HTML 覆盖率报告
# 用法: bash scripts/run_coverage.sh [build_dir]
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${1:-${PROJECT_ROOT}/build_coverage}"
REPORT_DIR="${PROJECT_ROOT}/coverage_report"

echo "=================================================="
echo "  AutoSAR AP Coverage Report Generator"
echo "  Project : ${PROJECT_ROOT}"
echo "  Build   : ${BUILD_DIR}"
echo "  Report  : ${REPORT_DIR}"
echo "=================================================="

# ----------------------------------------------------------
# 1. CMake 配置（Debug + 覆盖率）
# ----------------------------------------------------------
echo ""
echo "[1/5] CMake configure..."
cmake -S "${PROJECT_ROOT}" \
      -B "${BUILD_DIR}" \
      -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_COMPILER="$(xcrun -f clang++)" \
      -DCMAKE_OSX_ARCHITECTURES=arm64 \
      -DGTest_DIR=/opt/homebrew/Cellar/googletest/1.17.0/lib/cmake/GTest \
      -DBUILD_TESTS=ON \
      -DENABLE_COVERAGE=ON

# ----------------------------------------------------------
# 2. 编译
# ----------------------------------------------------------
echo ""
echo "[2/5] Build..."
cmake --build "${BUILD_DIR}" --parallel "$(sysctl -n hw.logicalcpu)"

# ----------------------------------------------------------
# 3. 清理旧的 .gcda 数据
# ----------------------------------------------------------
echo ""
echo "[3/5] Reset coverage counters..."
find "${BUILD_DIR}" -name "*.gcda" -delete 2>/dev/null || true

# ----------------------------------------------------------
# 4. 运行单元测试
# ----------------------------------------------------------
echo ""
echo "[4/5] Run tests..."
cd "${BUILD_DIR}"
ctest --output-on-failure --parallel "$(sysctl -n hw.logicalcpu)"

# ----------------------------------------------------------
# 5. 生成 lcov HTML 报告
#    macOS Apple Clang 的 gcov 包装器路径
# ----------------------------------------------------------
echo ""
echo "[5/5] Generate coverage report..."

LLVM_GCOV="/Library/Developer/CommandLineTools/usr/bin/llvm-cov"
GCOV_TOOL="${PROJECT_ROOT}/scripts/llvm_gcov_wrapper.sh"

# 创建 llvm-cov gcov 包装脚本（lcov 需要调用 gcov 可执行文件）
mkdir -p "$(dirname "${GCOV_TOOL}")"
cat > "${GCOV_TOOL}" << 'EOF'
#!/usr/bin/env bash
exec /Library/Developer/CommandLineTools/usr/bin/llvm-cov gcov "$@"
EOF
chmod +x "${GCOV_TOOL}"

mkdir -p "${REPORT_DIR}"

# 收集覆盖率数据
lcov --capture \
     --directory "${BUILD_DIR}" \
     --output-file "${BUILD_DIR}/coverage_raw.info" \
     --gcov-tool "${GCOV_TOOL}" \
     --ignore-errors inconsistent,unused \
     --rc branch_coverage=0

# 过滤掉第三方代码（GTest / 系统头文件）
lcov --remove "${BUILD_DIR}/coverage_raw.info" \
     '*/googletest/*' \
     '*/gtest/*' \
     '/usr/*' \
     '/Library/*' \
     '/opt/*' \
     '*/tests/*' \
     --output-file "${BUILD_DIR}/coverage.info" \
     --ignore-errors unused

# 生成 HTML
genhtml "${BUILD_DIR}/coverage.info" \
        --output-directory "${REPORT_DIR}" \
        --title "MyAutoSarAp Coverage Report (R25-11)" \
        --legend \
        --show-details \
        --prefix "${PROJECT_ROOT}"

echo ""
echo "=================================================="
echo "  Coverage report generated:"
echo "  ${REPORT_DIR}/index.html"
echo "=================================================="

# 打印摘要
lcov --summary "${BUILD_DIR}/coverage.info" 2>&1 | grep -E 'lines|functions|branches' || true
