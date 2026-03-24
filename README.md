# MyAutoSarAp

基于 **AUTOSAR Adaptive Platform（AP）R25-11** 规范的应用开发框架。

## 项目结构

```
MyAutoSarAp/
├── include/                    # 头文件（公共接口）
│   └── ara/
│       ├── com/                # Communication Management
│       │   ├── types.h         # 基础类型
│       │   ├── proxy_base.h    # Proxy 基类（Consumer 端）
│       │   └── skeleton_base.h # Skeleton 基类（Provider 端）
│       ├── exec/               # Execution Management
│       │   └── execution_client.h
│       ├── log/                # Logging and Tracing
│       │   └── logger.h
│       ├── diag/               # Diagnostics
│       │   └── diag_error_domain.h
│       ├── per/                # Persistency
│       │   └── key_value_storage.h
│       ├── crypto/             # Cryptography
│       ├── nm/                 # Network Management
│       ├── iam/                # Identity and Access Management
│       └── tsync/              # Time Synchronization
│
├── src/                        # 源文件（实现）
│   ├── ara/
│   │   ├── com/                # ara::com 实现
│   │   ├── exec/               # ara::exec 实现
│   │   ├── log/                # ara::log 实现
│   │   ├── diag/               # ara::diag 实现
│   │   ├── per/                # ara::per 实现
│   │   ├── crypto/             # ara::crypto 实现
│   │   └── nm/                 # ara::nm 实现
│   └── application/
│       └── main.cpp            # 应用入口
│
├── config/                     # 配置文件
│   ├── someip/                 # SOME/IP 服务配置
│   ├── execution/              # 执行管理配置
│   ├── diag/                   # 诊断配置
│   ├── log/                    # 日志配置
│   └── per/                    # 持久化存储配置
│
├── manifest/                   # AP Application Manifest
│   └── application_manifest.json
│
├── tests/                      # 测试
│   ├── unit/                   # 单元测试
│   └── integration/            # 集成测试
│
├── doc/                        # 文档
│   ├── design/                 # 设计文档
│   └── interface/              # 接口文档
│
├── cmake/                      # CMake 辅助脚本
├── scripts/                    # 构建/部署脚本
└── CMakeLists.txt              # 根构建文件
```

## 功能集群（Functional Clusters）

| 模块 | 命名空间 | 说明 |
|------|---------|------|
| Communication Management | `ara::com` | SOME/IP 服务通信，Proxy/Skeleton 模式 |
| Execution Management | `ara::exec` | 应用生命周期管理，ExecutionState 上报 |
| Logging and Tracing | `ara::log` | 结构化日志，DLT 协议支持 |
| Diagnostics | `ara::diag` | UDS 诊断，DTC 管理 |
| Persistency | `ara::per` | KV 存储，文件代理 |
| Cryptography | `ara::crypto` | 密钥管理，TLS/SecOC |
| Network Management | `ara::nm` | 网络状态管理 |
| Identity and Access Management | `ara::iam` | 身份认证与访问控制 |
| Time Synchronization | `ara::tsync` | 时间同步（IEEE 802.1AS） |

## 构建方法

### 依赖

- CMake >= 3.16
- GCC >= 9 或 Clang >= 10
- C++17

### 编译

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
```

### 带测试编译

```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON
make -j$(nproc)
ctest --output-on-failure
```

## 应用启动流程

```
1. 初始化 ara::log
2. 初始化 ara::com
3. 向 Execution Management 上报 ExecutionState::kRunning
4. 主循环（处理业务逻辑）
5. 捕获 SIGTERM/SIGINT，优雅退出
```

## 规范参考

- AUTOSAR AP R25-11
- AUTOSAR_SWS_CommunicationManagement (R25-11)
- AUTOSAR_SWS_ExecutionManagement (R25-11)
- AUTOSAR_SWS_LogAndTrace (R25-11)
- AUTOSAR_SWS_Diagnostics (R25-11)
- AUTOSAR_SWS_Persistency (R25-11)
- AUTOSAR_SWS_Cryptography (R25-11)
- AUTOSAR_SWS_NetworkManagement (R25-11)
- AUTOSAR_SWS_IdentityAndAccessManagement (R25-11)
- AUTOSAR_SWS_TimeSynchronization (R25-11)

## License

MIT
