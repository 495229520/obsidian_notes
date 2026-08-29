#include <algorithm>
#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// ==============================================================================
// 1. 类型定义与全局状态
// ==============================================================================

// 基础类型定义：资源数量与向量/矩阵
using Count = int;                                      // 资源数量类型（通常非负）
using ResourceVector = std::vector<Count>;              // 资源向量：维度为 m（m 类资源），如 Available 或 Request
using ResourceMatrix = std::vector<ResourceVector>;      // 资源矩阵：维度为 n x m（n 个进程，m 类资源），如 Allocation 或 Max

// 银行家算法全局状态结构体
struct State {
    ResourceMatrix allocation;  // Allocation[n][m]：每个进程当前已持有的各类资源数量
    ResourceMatrix maximum;     // Max[n][m]：每个进程事先声明的最大资源需求量
    ResourceVector available;   // Available[m]：系统当前剩余未分配的各类可用资源数量
};

// 安全性检查推演结果
struct SafetyResult {
    bool safe{};                             // 是否存在安全序列（true: 安全状态；false: 非安全状态）
    std::vector<std::size_t> sequence;       // 推导出的安全序列 <P_i1, P_i2, ...>（按进程索引排列）
};

// 资源请求裁决结果枚举
enum class RequestDecision {
    Granted,                // 批准：请求合法且试分配后系统仍处于安全状态
    MustWait,               // 等待：请求合法但当前可用资源不足（Request > Available）
    ExceedsDeclaredMaximum, // 拒绝：请求非法，超过了事先声明的最大需求（Request > Need）
    UnsafeRolledBack        // 回滚：可用资源足够，但试分配后系统进入非安全状态（可能死锁）
};

// 资源请求结果结构体
struct RequestResult {
    RequestDecision decision{};  // 裁决决策
    SafetyResult trial_safety;   // 试分配后的安全性检查结果（若批准或回滚时附带推演结果）
};

// ==============================================================================
// 2. 向量操作与状态校验
// ==============================================================================

// 逐分量比较向量 lhs <= rhs
// 银行家算法中要求每类资源都不超限（\forall j, lhs[j] <= rhs[j]），不可使用字典序或求和比较
[[nodiscard]] bool componentwise_leq(const ResourceVector& lhs,
                                     const ResourceVector& rhs) {
    if (lhs.size() != rhs.size()) {
        return false;  // 维度不一致视为无法比较/不满足
    }
    for (std::size_t j = 0; j < lhs.size(); ++j) {
        if (lhs[j] > rhs[j]) {
            return false;  // 只要有任意一类资源超限，即不满足 <=
        }
    }
    return true;  // 所有资源分量均满足 lhs[j] <= rhs[j]
}

// 校验系统状态矩阵与向量的合法性不变量
void validate_state(const State& state) {
    const std::size_t process_count = state.allocation.size();
    const std::size_t resource_count = state.available.size();

    // 基础维度非空校验
    if (process_count == 0 || resource_count == 0 ||
        state.maximum.size() != process_count) {
        throw std::invalid_argument("invalid matrix dimensions");
    }

    // 检查矩阵行宽一致性及 Allocation <= Maximum
    for (std::size_t i = 0; i < process_count; ++i) {
        if (state.allocation[i].size() != resource_count ||
            state.maximum[i].size() != resource_count) {
            throw std::invalid_argument("inconsistent row width");
        }
        for (std::size_t j = 0; j < resource_count; ++j) {
            if (state.allocation[i][j] < 0 || state.maximum[i][j] < 0 ||
                state.allocation[i][j] > state.maximum[i][j]) {
                throw std::invalid_argument("invalid allocation/maximum");
            }
        }
    }

    // 可用资源向量不能存在负数
    if (std::any_of(state.available.begin(), state.available.end(),
                    [](Count value) { return value < 0; })) {
        throw std::invalid_argument("available resources cannot be negative");
    }
}

// ==============================================================================
// 3. 核心算法：Need 计算、安全性检查与请求判定
// ==============================================================================

// 计算剩余需求矩阵：Need[i][j] = Max[i][j] - Allocation[i][j]
// 表示每个进程未来还可能申请的最大资源数量
[[nodiscard]] ResourceMatrix calculate_need(const State& state) {
    ResourceMatrix need = state.maximum;  // 以 maximum 为基准进行拷贝
    for (std::size_t i = 0; i < need.size(); ++i) {
        for (std::size_t j = 0; j < need[i].size(); ++j) {
            need[i][j] -= state.allocation[i][j];  // 逐元素相减得到 Need
        }
    }
    return need;
}

// 安全性检查算法（Safety Algorithm）：推演是否存在一条让所有进程顺利完成的安全序列
[[nodiscard]] SafetyResult check_safety(const State& state) {
    validate_state(state);

    const ResourceMatrix need = calculate_need(state);  // 计算当前各进程的剩余需求 Need
    ResourceVector work = state.available;              // Work 向量：推演过程中系统可支配的资源，初始为 Available
    std::vector<bool> finished(state.allocation.size(), false); // Finish 标志：标记进程是否已在推演中完成
    std::vector<std::size_t> sequence;                  // 记录推演出的安全序列
    sequence.reserve(state.allocation.size());

    // 循环推进：每次尝试找出一个可满足并完成的进程
    while (sequence.size() < state.allocation.size()) {
        bool progressed = false;  // 记录本轮扫描是否有进程顺利完成推进

        for (std::size_t i = 0; i < state.allocation.size(); ++i) {
            // 条件：进程未完成 且 其剩余需求不超过当前可支配资源 (Need[i] <= Work)
            if (!finished[i] && componentwise_leq(need[i], work)) {
                // 假设进程 P_i 拿到所需资源运行结束，释放其持有的全部资源
                // 净归还量为 allocation[i]，更新 Work = Work + Allocation[i]
                for (std::size_t j = 0; j < work.size(); ++j) {
                    work[j] += state.allocation[i][j];
                }
                finished[i] = true;         // 标记 P_i 已完成
                sequence.push_back(i);      // 加入安全序列
                progressed = true;          // 本轮有进展
                break;                      // 资源池扩大，跳出并从头开始新一轮扫描
            }
        }

        // 若本轮扫描未找到任何可推进的进程，说明不存在可行安全序列，提前终止
        if (!progressed) {
            break;
        }
    }

    // 若安全序列包含了所有进程，则系统处于安全状态
    return {sequence.size() == state.allocation.size(), std::move(sequence)};
}

// 资源请求算法（Resource-Request Algorithm）：响应进程 process_id 提出的 request 资源申请
[[nodiscard]] RequestResult request_resources(State& state,
                                              std::size_t process_id,
                                              const ResourceVector& request) {
    validate_state(state);
    // 检查请求参数合法性
    if (process_id >= state.allocation.size() ||
        request.size() != state.available.size() ||
        std::any_of(request.begin(), request.end(),
                    [](Count value) { return value < 0; })) {
        throw std::invalid_argument("invalid resource request");
    }

    const ResourceMatrix need = calculate_need(state);

    // 【第一道门】合法性检查：申请量不能超过其最大声明的剩余需求 (Request <= Need[i])
    if (!componentwise_leq(request, need[process_id])) {
        return {RequestDecision::ExceedsDeclaredMaximum, {}};
    }

    // 【第二道门】可用性检查：申请量不能超过当前系统可用资源 (Request <= Available)
    if (!componentwise_leq(request, state.available)) {
        return {RequestDecision::MustWait, {}};
    }

    // 【试分配】：临时修改系统状态（假设满足该请求）
    for (std::size_t j = 0; j < request.size(); ++j) {
        state.available[j] -= request[j];                  // 临时扣减可用资源
        state.allocation[process_id][j] += request[j];     // 临时增加进程持有资源
    }

    // 【第三道门】安全性检查：推演试分配后的状态是否存在安全序列
    SafetyResult trial = check_safety(state);
    if (trial.safe) {
        // 安全：正式批准请求，保留修改后的状态并返回推导出的安全序列
        return {RequestDecision::Granted, std::move(trial)};
    }

    // 非安全：必须撤销试分配，完全回滚状态（恢复原状）并让进程等待
    for (std::size_t j = 0; j < request.size(); ++j) {
        state.available[j] += request[j];                  // 归还扣减的可用资源
        state.allocation[process_id][j] -= request[j];     // 扣回增加的持有资源
    }
    return {RequestDecision::UnsafeRolledBack, std::move(trial)};
}

// ==============================================================================
// 4. 辅助展示函数与测试驱动
// ==============================================================================

// 将裁决枚举转为人类可读的中文字符串
[[nodiscard]] std::string decision_text(RequestDecision decision) {
    switch (decision) {
    case RequestDecision::Granted:
        return "已批准";
    case RequestDecision::MustWait:
        return "当前可用资源不足，进入等待";
    case RequestDecision::ExceedsDeclaredMaximum:
        return "超过事先声明的最大需求";
    case RequestDecision::UnsafeRolledBack:
        return "试分配后不安全，已回滚";
    }
    throw std::logic_error("unknown request decision");
}

// 将安全序列索引转换为进程序列字符串（如 "D -> A -> B -> C -> E"）
[[nodiscard]] std::string sequence_text(
    const std::vector<std::size_t>& sequence,
    const std::vector<std::string>& process_names) {
    std::string text;
    for (std::size_t k = 0; k < sequence.size(); ++k) {
        if (k != 0) {
            text += " -> ";
        }
        text += process_names.at(sequence[k]);
    }
    return text;
}

int main() {
    try {
        // 构造《Modern Operating Systems》图 6-12 示例状态
        // 5 个进程 A~E，4 类资源 (磁带机, 绘图仪, 打印机, 蓝光机)
        State state{
            // Allocation 矩阵 (5x4)
            {{3, 0, 1, 1},   // A
             {0, 1, 0, 0},   // B
             {1, 1, 1, 0},   // C
             {1, 1, 0, 1},   // D
             {0, 0, 0, 0}},  // E
            // Maximum 矩阵 (5x4)
            {{4, 1, 1, 1},   // A
             {0, 2, 1, 2},   // B
             {4, 2, 1, 0},   // C
             {1, 1, 1, 1},   // D
             {2, 1, 1, 0}},  // E
            // Available 向量 (4)
            {1, 0, 2, 0}
        };
        const std::vector<std::string> names{"A", "B", "C", "D", "E"};

        // 1. 检查初始状态安全性
        const SafetyResult initial = check_safety(state);
        std::cout << "初始状态：" << (initial.safe ? "安全" : "不安全")
                  << "，安全序列 " << sequence_text(initial.sequence, names) << '\n';

        // 2. 请求一：B 申请 1 台打印机 (0, 0, 1, 0) -> 试分配后安全，批准
        const RequestResult b_request =
            request_resources(state, 1, {0, 0, 1, 0});
        std::cout << "B 请求 1 台打印机："
                  << decision_text(b_request.decision) << '\n';
        if (b_request.decision == RequestDecision::Granted) {
            std::cout << "批准后的安全序列 "
                      << sequence_text(b_request.trial_safety.sequence, names) << '\n';
        }

        // 3. 请求二：E 申请最后 1 台打印机 (0, 0, 1, 0) -> 试分配后不安全，回滚并等待
        const RequestResult e_request =
            request_resources(state, 4, {0, 0, 1, 0});
        std::cout << "E 请求最后 1 台打印机："
                  << decision_text(e_request.decision) << '\n';
    } catch (const std::exception& error) {
        std::cerr << "输入状态非法：" << error.what() << '\n';
        return 1;
    }
    return 0;
}
