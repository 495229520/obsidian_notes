---
generated_by: trellis-obsidian-task-detail
readonly: true
project: "obsidian_notes-main"
title: "AI编译器与NPU编译优化学习（现在开始）"
trellis_status: "in_progress"
cssclasses:
  - trellis-task-detail
---

# AI编译器与NPU编译优化学习（现在开始）

> [!info] 自动生成 · 只读详情
> 此页是 Trellis 源文档在 Obsidian 库内的展示镜像，解决库外 `file://` 链接无法打开的问题。
> 请在 Trellis 项目中修改任务；下一次同步会用权威源重新生成此页。

- 状态：进行中
- 当前阶段：Phase 2 · 执行
- 执行清单：5/42
- 优先级：P1
- 负责人：melene
- 类型：study
- 范围：ai-compiler-npu
- 创建时间：2026-09-30
- Trellis Task：`.trellis/tasks/09-30-npu-compiler-study`

## 摘要

2026-10-18启动：第一周用Mac学习编译流程、IR/SSA与MLIR入门教程；第二周做矩阵参考、测试和重写/lowering阅读。LLVM构建与独立MLIR工程后移到第三周起，主构建机为9700X＋48GB＋WSL2 Ubuntu，PC已购2TB固态，待环境验证后推进；项目A/B按验收继续。

## PRD

来源：`.trellis/tasks/09-30-npu-compiler-study/prd.md`

# AI编译器与NPU编译优化学习

## 目标

从2026-10-18开始学习AI编译器，面向2028届实习准备。先在Mac上学习编译流程、IR/SSA和MLIR基本结构，配合短例子与矩阵参考；第三周起再按环境条件构建LLVM/MLIR，建立可运行的小型张量编译工程，再进入张量优化、片上内存规划和TPU-MLIR/BM1684X工业改动。

## 本轮决定

- 用户明确现在开始，不再等C++精通、力扣熟练或系统课程全部完成。C++、Python、矩阵与编译原理缺口在当前Lab中按需补充。
- 第一周使用Mac完成IR/SSA与MLIR阅读练习，不安装或构建完整LLVM；第二周结合矩阵参考、测试和重写/lowering阅读。LLVM构建、standalone工程与首个C++ Pass后移，具体任务见implement.md。
- xv6/QEMU从本路线移除，不安排并行课时、实验验收或简历替代项目。
- Obsidian阅读入口与本任务详情迁入`16.AI编译器/`。`.trellis/tasks/09-30-npu-compiler-study/`继续作为权威源，沿用任务身份和历史资料。
- 取消11月10日前不学编译器、11月11日才开始的限制；原C++/Linux、算法和MFMS资料按需查阅，不作为本任务的启动门槛。

## 已确认背景与规划假设

- 2028届，倾向NPU相关AI编译器中端、后端规划及底层原理。
- 过去确认每天总学习超过8小时；编译器4–5小时/主要学习日、每周6日仍是待实际记录校准的候选安排。
- 设备为Mac M5 Air、i7/16GB/GTX1660 Super Ubuntu、9700X/48GB/GTX970 PC。PC已购买2TB固态硬盘，后续确定使用9700X＋48GB＋WSL2 Ubuntu作为主构建机；硬盘安装、WSL2配置和实际可用资源待验证。i7 Ubuntu保留为备用。第一周以Mac为学习机，暂不购买显卡。
- 实习、GPU租用和NPU真机访问均不是已经取得的条件。

## 范围与约束

- 项目A限定静态二维f32与Matmul/Add/ReLU，逐步完成Verifier、转换、CPU执行、分块融合及教学片上内存规划。
- 项目B限定固定版本TPU-MLIR及BM1684X小图，完成路径定位、转换/量化验证，研究一项有具体用途的C++规划或诊断改动。
- 默认关闭的教学预算上限保留为L7起步题；深入策略工作超出基础范围时另记工时。
- 固定自研、练习、厂商案例的环境边界；先用现有设备推进，GPU选修，真NPU测量另行安排。
- 暂不购买显卡。L0–L5验收且具备明确GPU实验假设、独立参考、边界测试与测量脚本后，才考虑可选短租；完成课程阶段不自动触发租用或购买。
- 工时按主动实现、阅读、调试和测试记录；先过阶段验收，再推进后续任务。
- 不为这次计划调整安装完整工具链或认定实验已通过；学习状态进入执行，Lab完成情况仍由证据决定。

## 验收要求

1. 项目A的输入契约、自己的C++代码、结构测试和独立数值参考齐全；实际执行的是生成代码。
2. 分块/融合处理尾块、归约顺序和多消费者，能说明合法性与回退条件。
3. 内存规划覆盖物理buffer、对齐、scratch、输出和最后使用时点；有独立计划检查器和CPU回放。
4. 项目B有确定上游基线、自己的补丁及路径命中证据；完成默认行为和启用策略的正确性及边界回归。
5. CPU、CModel与真实NPU结果按验证层次记录；没有真机就不填芯片时延、吞吐和功耗。
6. 每个交付可在干净环境复现，保留失败输入、修复原因、原始数据与限制。
7. 入门阶段能标注简单IR的定义与使用、块参数及Region/Block结构，并解释一次重写和lowering前后的变化；阅读记录与实际构建/执行验证分开。
8. 项目A先形成可展示成果；项目B按实际完成范围加入简历，不强求凑满三个项目。MFMS仅展示本人实际贡献。

## 暂缓范围

完整语言parser、通用ONNX importer、完整LLVM后端、另一套TVM工程、所有Toy章节重写、完整大模型部署、多卡、NPU驱动/固件和GPU专项性能竞赛。xv6已从本路线移除，未来若重启应独立规划。

## 历史与当前状态

2026-09-30原方案和审查记录保存在research中；原始字节及哈希保持不变。2026-10-18按用户决定提前启动并迁移目录，更新当前PRD、设计和实施计划。原稿中的旧日期、并行系统实验和planning状态只代表历史方案。

## 设计

来源：`.trellis/tasks/09-30-npu-compiler-study/design.md`

# NPU编译学习路线设计

学习范围与验收要求见prd.md，排期和Lab见implement.md。本文件说明项目边界、依赖和环境。完整Pro原稿及审查记录按原始字节归档在research/；以下内容经过本轮约束修正。

2026-10-18起提前进入学习执行，Obsidian入口迁入`16.AI编译器/`，xv6/QEMU从当前路线移除。C++、算法和编译理论按Lab需要补充，不再等待11月11日。工具版本沿用固定教学基线；实际构建、wheel安装和CModel仍待逐项验证。

## 二、主线与能力证据

### 主线

```text
编译流程 / AST与IR → SSA、基本块与CFG（Mac阅读与手写练习）
                  ↓
MLIR Operation / Value / Region / Block / Dialect（入门教程）
                  ↓
矩阵参考与测试；按需补 Python / 张量 / 浮点基础
                  ↓
use-def、支配关系、算子语义、副作用、重写与lowering
                  ↓
环境条件确认后构建LLVM/MLIR（第三周起）
                  ↓
一个自己的 MLIR C++ 工程：Dialect + Verifier + Pass
                  ↓
自定义算子 → Linalg；建立未经优化的 CPU 正确性基线
                  ↓
张量级 tiling / fusion → bufferization → 现成循环与 LLVM lowering
                  ↓
固定 schedule 下的张量/缓冲区生命周期、片上容量与数据搬运计划
                  ↓
TPU-MLIR / BM1684X 案例：Top→Tpu→LayerGroup→CodeGen→BModel
                  ↓
一个有回归测试的 C++ 编译优化/规划策略修改
```

这是学习依赖，不是说 TPU-MLIR 必须经过我们自研工程的 Linalg/LLVM 路线。学习时先用未优化的 bufferization 建立执行基线；最终优化流水线通常先在 tensor 层 tiling/fusion，再做 bufferization。不要把学习顺序机械当成生产 Pass 顺序。

第一条线的 CPU 后端是正确性验证工具；目标是学习变换与规划，并非用 CPU 部署替换 NPU 目标。第二条线用于学习真实设备限制和工业工程，但不要求自己实现完整 parser、通用 ONNX importer、LLVM 后端、NPU 驱动或固件。

### 三层证据

| 层次 | 能够证明 | 不能据此声称 |
|---|---|---|
| 入门实验 | 跑通教程、完成 TODO、解释对应 IR | 独立设计过完整编译器、满足岗位要求 |
| 可展示项目 | 自己的实现、正确性边界、复现命令、测试、失败分析 | 生产级性能、全部模型支持、已达到某公司的招聘要求 |
| 岗位适配 | 对目标 JD 逐项提供代码/调试/理论/协作证据，能处理陌生问题 | 按学习时长自动满足岗位、保证录用 |

12 月投递继续依靠真实的 C++/Linux/MFMS能力，不等待编译器项目完成。2027 春季逐步加入已经验收的编译器成果。MFMS只展示可公开的代码/设计和本人完成部分，不上传企业敏感代码。

## 三、版本与资源：同版本源码、同版本说明

### 入门资料与使用顺序

1. [MLIR Beginner-Friendly Tutorial](https://github.com/AlexandreSinger/mlir-beginner-friendly-tutorial)：先看[第一期](https://youtu.be/Uno_XhtkT5E)理解动机并阅读Demo 1–2，IR/SSA结合短例子补齐；第二周按需看[第二期](https://youtu.be/l0O4Vbc3l5c)和Demo 3的逐步lowering。Demo 4优化留到后续，不要求两周读完全部代码。
2. [Understanding the IR Structure](https://mlir.llvm.org/docs/Tutorials/UnderstandingTheIRStructure/)与[MLIR语言参考](https://mlir.llvm.org/docs/LangRef/)用于核对Operation、Value、Region、Block及SSA作用域。先读相关小节，不要求实现SSA构建算法。
3. [Toy第1章](https://mlir.llvm.org/docs/Tutorials/Toy/Ch-1/)和[第2章](https://mlir.llvm.org/docs/Tutorials/Toy/Ch-2/)先读AST到MLIR的关系与Dialect例子；[第3章](https://mlir.llvm.org/docs/Tutorials/Toy/Ch-3/)用于观察重写。第一轮不实现完整parser。

第一周只需浏览器、编辑器与笔记；手写的`.mlir`标注为“未运行验证”。第二周的小型C++/NumPy实验只检查并补齐必要工具，不预装大型SDK。每次阅读后留下标注、手推或代码结果，不能用观看时长代替理解检查。

Beginner-Friendly仓库的Demo 0要求源码构建，并固定了自己的LLVM子模块；前两周跳过该构建步骤。以后运行其代码时核对所需版本，与U的20.1.8分开处理，不默认兼容，也不在当前磁盘条件下同时复制多套LLVM。在线Toy文档用于理解概念，U实现与命令以20.1.8版本内源码/文档为准。

数学与算法使用[[16.AI编译器/02.AI编译器与MLIR的数学和算法基础|数学与算法基础笔记]]作为参考，阅读位置与小检查统一维护在implement.md的“数学与算法基础的阶段关联”。各项嵌入原Lab，不追加前置数学学期；当前推理初版不要求自动微分或完整多面体理论。

### 固定的三套环境边界

| 标识 | 版本与来源 | 使用范围 | 来源报告状态（待本地复验） |
|---|---|---|---|
| U：自研工程 | LLVM `llvmorg-20.1.8`，commit `87f0227cb60147a26a1eeb4fb06e3b505e9c7261` | 官方 standalone、Toy、Transform；自己的 C++ 工程 | 源码/tag已核查；实际构建待验证 |
| G：教学练习 | `Groverkss/mlir-tutor` 的下列 exercise SHA；`mlir-wheel==20260115`、`mlir-python-bindings==20260115` | 只做选定练习；独立小 venv/build | 分支、README、requirements及TODO已核查；wheel安装/测试待验证 |
| S：工业案例 | TPU-MLIR `v1.30.2`，commit `ecc9dd6c32e79517807610c51a9bbfdd66c41029` | BM1684X单目标案例、量化、LayerGroup与CModel | 源码/发布/版本内文档已核查；整套运行待验证 |

`20.1.8`是自研工程的教学基线，不是最新版，也不用于替换其他项目的依赖。G 的 `lit` 未固定，成功安装后还需锁定；wheel平台/Python兼容需实测。S 的版本内环境文档指定 `sophgo/tpuc_dev:v3.4`，这是候选配对，不是已验证镜像组合；拉取后记录镜像 digest、实际LLVM信息与共享库哈希。

来源报告核查的浮动 TPU-MLIR 在线开发手册显示版本 `1.28-beta.0-13-g46a1d195d`，与选定 v1.30.2 不同。命令、接口优先查 v1.30.2 checkout 下 `docs/developer_manual/source_zh/` 和对应发布资料。即使同一 checkout，说明也可能落后于实现，执行路径以源码、日志和测试共同确认。

### 免费资源的真实性与类别

| 资源 | 实际类型 | 选取位置 |
|---|---|---|
| [LLVM Toy](https://mlir.llvm.org/docs/Tutorials/Toy/) | 完整分章示例，不是留空作业 | U：`mlir/examples/toy/Ch2`、`Ch3`、`Ch5`、`Ch6` |
| [standalone](https://github.com/llvm/llvm-project/tree/llvmorg-20.1.8/mlir/examples/standalone) | 工程模板、示例 Dialect 与测试 | U：`mlir/examples/standalone` |
| [Transform Tutorial](https://mlir.llvm.org/docs/Tutorials/transform/) | 官方讲解、示例与扩展接口 | U：`mlir/examples/transform`、版本内教程/测试 |
| [mlir-tutor](https://github.com/Groverkss/mlir-tutor) ch1 | 真实填空练习 | `ch1-exercise@9cae1b9e585638c7e6f6de22a40fce23bd9d336d`；`tutorial/ch1-cpu-vector-dsl` |
| 同仓库 ch2 | 真实填空练习 | `ch2-exercise@48e89a92127af8636cc325560d131d8edcc16bb6`；`tutorial/ch2-cpu-vector-dsl-loops` |
| 同仓库 ch4 | 真实填空练习和完整示例 | `ch4-exercise@8352d03164ffb4f25d5d28a0de0de113e81d1b0d`；`tutorial/ch4-linalg-transforms` |
| [TPU-MLIR](https://github.com/sophgo/tpu-mlir/tree/v1.30.2) | 工业源码、转换/验证工具，不是官方求职训练作业 | S：`lib/Dialect`、`lib/Conversion`、`lib/Backend`、`python/tools`、`python/test`、`regression` |
| [Bril](https://github.com/sampsyo/bril) / [CS6120](https://www.cs.cornell.edu/courses/cs6120/2025sp/self-guided/) | IR教学基础设施和课程实现任务 | 只在SSA/数据流明显卡住时补4–8小时，不完整重做DCE/LVN课程 |

Grover各练习分支是独立快照。使用独立 worktree/小 build 目录和同一匹配wheel环境；不能假设切换分支会保留各章未提交修改。其 `square.py`、`matmul.py` 的说明是展示 lowering IR，打印成功不代表已执行生成的机器码。`check-tutorial-ch1/ch2/ch4` 检查哪些层面，要阅读测试本身。

G 的 ch3 GPU题不是做 ch4 Linalg题的必经门槛。完成教学题后，把概念应用到 U，不混用 G 的生成头文件和 U 的二进制库。

## 六、通用概念与厂商依赖必须分开

| 层 | 本方案研究内容 | 边界 |
|---|---|---|
| 通用 | SSA/use-def、合法性、Dialect、lowering、tiling/fusion、bufferization、生命周期与调度 | 不依赖某厂商才能学习 |
| 厂商专用 | Top/Tpu、LayerGroup策略、BModel、BM168X CodeGen/API | 不是MLIR标准接口，也不代表所有NPU |
| 可修改源码 | C++Dialect/Pass/Conversion/规划、Python转换/验证、部分backend封装 | 源码里的调用不代表被调的全部底层实现都以源码提供 |
| 二进制与SDK | 后端动态库/CModel、工具链、打补丁的tblgen/PPL/交叉工具等 | 按license与版本使用；来源报告未重建这些组件，本次整理也未运行构建，也不笼统称全部闭源 |
| 真机Runtime | 芯片驱动、运行时、固件、板卡和性能工具 | 编译器学习不要求自行实现，但硬件测量需要匹配组合 |

S的CodeGen版本内说明明确：BM168X相关封装加载`third_party/nntoolchain/lib/libbackend_xxx.so`函数，收集命令并生成BModel。CMake还使用预编译/补丁版tblgen工具。不能把“仓库开源”理解成“每一层均能仅靠stock LLVM从零重建”。

S的`envsetup.sh`不只是设置PATH：还可能下载PPL/交叉工具链、设置远程ccache并安装git hooks。来源报告记录其远程ccache指向厂商内网；外部学习环境应审查并移除/取消该远程配置，不连接无权访问的服务。`build.sh`显式根据`/proc/stat`生成`-j`，单独设置`CMAKE_BUILD_PARALLEL_LEVEL`不会覆盖显式`-j`。需要记录一项资源控制补丁或使用经审查的分步构建，不把U的构建脚本直接套给S。

CPU/CModel容器不默认使用`--privileged`。真正设备接入时再按最小需要配置设备权限。不要把通用文档中“克隆LLVM主分支供导航”的示例当成项目匹配LLVM版本证明。

## 两个项目的工程归属

项目A从LLVM/MLIR的standalone骨架建立独立C++工程。MLIR提供Operation/Value、重写和后续lowering基础设施；学习者实现自己的算子、Verifier、转换、调度和有限设备规划。CPU生成代码与独立参考实现构成正确性基线。教学片上容量、搬运事件和CPU回放不能当作真实BM1684X的周期、带宽或功耗。

项目B在固定TPU-MLIR基线上建立个人分支或fork，跟踪BM1684X实际编译路径，再修改工业C++代码。厂商后端接口、预编译库和CModel保持版本匹配，不能用自研工程的LLVM安装替换该项目依赖。

`mini-npu-compiler-lab`和`tpumlir-layergroup-study`是候选个人仓库名，不是已存在的整套开源Lab答案仓库。

## 项目B从起步题到简历成果

L7保留默认关闭的教学预算上限。实现时要影响实际可行性检查和切片/回退路径，有默认行为回归；只新增开关、参数传递和日志不能充分证明编译优化能力。

在理解活动路径后，争取深入一项具体工作：合法候选的确定性选择、资源不足诊断与回归、真实规划/正确性问题修复、权重驻留或缓冲区复用策略。是否存在可修复问题需实验确认，不能预设上游有bug。对策略结果记录功能、内存、搬运量及代价；没有真机则不报告芯片性能。超出L7基础范围的工作单列工时，不塞进原预算。

## 设备选择修正

第一周使用Mac M5 Air学习IR/SSA、阅读教程和标注MLIR；第二周继续做小型C++/NumPy参考与测试。PC已购买2TB固态硬盘，后续主构建机确定为9700X＋48GB＋WSL2 Ubuntu。第三周起在硬盘安装、WSL2和工具链验证完成后构建U；WSL虚拟磁盘安排在新SSD上，源码与build放在WSL内部的Linux文件系统。主机48GB物理内存不等于WSL可用内存，实际内存、磁盘余量及构建并发需检查。i7/16GB Ubuntu保留为备用，Mac继续用于编辑和SSH。第一周不因硬盘购入而提前安排大型构建；暂不购买显卡，GPU/NPU也不是本阶段必需设备。

## 简历与岗位范围

项目A对应MLIR实现、张量变换和教学内存规划；B对应工业编译器源码修改、目标约束和回归验证。这些成果适合对照AI编译器图/张量优化、NPU编译规划岗位，LLVM指令级后端、GPU驱动或固件岗位还需要各自的专项能力。

以[华为云AI编译器实习岗位描述](https://www.huaweicloud.com/lab/algorithm/intern_recruitment.html)为例，图优化、内存优化、LLVM/MLIR与项目内容有关；岗位也提出硬件和大模型技术栈要求，不能用项目完成替代全部JD核对。该页面只用于能力范围参考，不作为未来开放名额保证。

简历按实际完成度选择项目：AI编译器方向优先A，再加入已形成实质改动和回归的B；MFMS按岗位需要和本人贡献选用。B仍在阅读/转换阶段时，先展示A与已完成的系统项目，不要求凑满三个。

## 验证状态与来源

源码/tag、exercise分支、版本内文档的核查是来源Pro报告中的记录。原旧24周方案的字节哈希与AUDIT内记录一致；当前使用修订方案作为主线。归档本身只证明收到和保存了这些文本，不继承其中沙箱运行、完整构建或性能实验结论。

本轮已确认开始学习并切换当前Task；迁移文档本身不计为通过任何Lab。逐项记录资源、环境、测试和实际工时。Obsidian详情由任务元数据`meta.obsidian_detail_path`指定到`16.AI编译器/90.学习与实践/`，总看板继续引用该投影。

## 实施计划

来源：`.trellis/tasks/09-30-npu-compiler-study/implement.md`

# NPU编译学习实施计划

从2026-10-18开始，当前进入L0。先用Mac读懂IR/SSA和MLIR短例子，再完成矩阵参考与测试；LLVM构建和独立MLIR工程移到第三周起，按环境条件推进。C++、Python、矩阵和编译原理在任务中按需补充，不以C++精通、力扣熟练或其他课程完成为启动条件。xv6/QEMU已移出本路线。

## 四、时间预算与阶段窗口

### 规划假设

编译器候选投入4–5小时/主要学习日，每周6日，另1天机动。原xv6课时不自动叠加给编译器；两周后根据实际投入、课程和面试情况重新校准。

名义容量24–30小时/周，保留15%机动后约20.4–25.5小时/周，中值22.95。Lab工时已包含一般阅读、调试和测试；等待编译或下载的时间单列。

| 阶段 | 主动工时 | 主要验收门槛 | 中值容量下的累计阶段参考 |
|---|---:|---|---|
| L0 基础诊断与环境 | 32–44 | 张量参考、边界测试、可构建模板 | 第三周起完成构建后验收 |
| L1 SSA / use-def / 合法重写 | 36–48 | 自写Pass、正反例及选定练习 | 第4周 |
| L2 Dialect与部分lowering | 50–66 | 小算子、Verifier、MiniNN→Linalg | 第6周 |
| L3 Bufferization与CPU执行 | 44–58 | 生成代码执行、别名与所有权测试 | 第8–9周 |
| L4 Tiling与fusion | 50–70 | C++变换、尾块和归约合法性，发布A初版 | 第11周 |
| L5 生命周期、片上预算与搬运 | 60–84 | 规划器、容量/依赖检查与CPU回放 | 第14周 |
| L6 TPU转换、量化与CModel | 36–50 | 阶段对照、量化误差与功能验证 | 第16周 |
| L7 工业LayerGroup代码改动 | 60–84 | C++策略修改、默认回归与回退测试 | 第19–20周 |
| 项目发布与面试材料 | 18–26 | 干净环境复现、贡献与原始证据 | 第20周左右 |

前两周提前学习L1的IR/SSA与重写概念，工时仍计入L1；矩阵参考和环境工作计入L0，不重复计算。L0的工程验收移到第三周起；表中L1及之后的周数保留为原工时模型参考，第二周末按实际进度重估，不作为新的完成承诺。

合计386–530小时，中值458；按上述容量约15–26周，中值约20周。开始日提前不改变掌握标准，也不将旧完成日期机械当作新截止日。必要补课单列；先缩范围或延后扩展，保留正确性测试。

L2期间提前验证TPU环境、一个小图及源码/依赖路径，从L6/L7原预算中计时。环境受阻时记录具体原因，继续项目A，避免到最后阶段才发现工业依赖不可用。

### 进入实习或课程增多后的调整

发生时间变化后按实际剩余工时重排，不预设12月已经开始实习。优先保留A的CPU执行和分块融合；B按实际补丁成熟度展示。先取消GPU/CUDA、Inductor/IREE等选修，再缩扩展范围；求职材料随已验收成果更新，不等完整路线结束。

## 五、必修Lab任务书

下列L0–L7总体任务书是本方案自行设计，不冒充官方课程。标为G的填空部分才是仓库原有练习。标为“拟创建”的文件是你需要实现的项目文件，不是本ZIP已提供的代码。

### L0：基础诊断与可复现环境（32–44小时）

**问题与前置**：确认能表达张量运算并建立C++工程，不默认已掌握Python/矩阵/DL。只需已有基本C++和命令行，不需训练完整模型。

**来源/目录**：先读设计中的Beginner-Friendly与IR结构资料；后续使用U的standalone；自己的`tests/reference/`、`tools/`（拟创建）。环境命令按选定版本的官方说明记录；来源原稿提及的配套脚本未随本Task提供，不把它们视为已有工具。

**亲自实现**：C++17的边界检查版二维矩阵乘法参考函数；Python/NumPy形状、转置、广播小实验和对照；记录shape、dtype、stride以及误差。完成小型CMake工程、失败退出码和测试入口。把原始三重循环和独立NumPy实现都保留，避免两个路径共享同一个错误。

**验收**：前两周先记录参考与测试结果，L0仍须等后续MLIR模板构建和自己的测试通过后才整体勾选。手算例、非方阵、K不匹配、单元素；显式拒绝初版不支持的动态/零长度情形。说明NaN/Inf与舍入不是普通实数运算。模板构建成功并运行自己的第一项测试；探测输出不代表工具链已验证。

**交付/求职证据**：环境清单、参考实现、边界测试；只算基础复现，不单独包装为编译器项目。

**失败简化**：先在Mac完成阅读与小型参考程序；编译器或NumPy缺失时记录并补齐必要依赖。Ubuntu构建条件未就绪时继续手推IR、参考测试和源码阅读，保留“构建未验证”，不等待新硬件，也不跳过最终工程验收。

### L1：SSA、use-def与第一个合法C++重写（36–48小时）

**前置**：IR/SSA和MLIR结构的概念学习提前到第一周，不依赖L0构建完成；进入C++ Pass实现前须通过L0。先理解SSA值、块参数、use-def、支配与副作用，不要求自行实现完整SSA构建算法。

**来源/版本**：U standalone、Toy Ch2/Ch3；G ch1对应SHA，`TinyDialect.td`、`TinyToArith.cpp`、`TinyToLLVM.cpp`。教学题控制在8–12小时，计入本Lab。

**亲自实现**：自己的`UseDefReport.cpp`遍历producer/consumer；受限、可解释的DCE工作列表；整数`addi(x,0)`改写。删除前使用正确的副作用/可删除性接口，初版不跨未知region和未知call推断。G填补选定算子定义、算术lowering和store转换，不照抄main答案。

**测试**：链式无用值、多使用者、结果未使用但有store/call；块参数和支配关系反例；不同整数位宽。浮点`x*0`、`x+0`不得机械套整数规则；未选择的FP变换必须保持不改写。结构测试看规则是否发生；可执行小函数比较结果。

**交付/证据**：`mini-opt`、自己的Pass代码、lit/FileCheck及执行测试、至少一个失败用例和修复diff。证明会实现受限变换，不证明已完成完整优化器。

**失败简化**：先仅支持单基本块、无region的纯算子。G wheel装不通时将同类任务移到U并明确标成自设计替代，不宣称通过G官方测试。

### L2：MiniNN Dialect与部分Lowering（50–66小时）

**前置**：L1；矩阵乘法、逐元素Add/ReLU、shape约束、region参数/yield。

**来源**：U standalone、Toy Ch2/Ch5；G ch2 `TinyLoopToSCF.cpp`及`test/lower-to-scf.mlir`，选定练习6–10小时包含在本Lab。

**亲自实现**：`mini.matmul`、`mini.add`、`mini.relu`的ODS/Verifier；静态二维f32，Add要求同shape，不默认广播。选择并记录ReLU的NaN与signed-zero语义。写C++转换到Linalg的patterns、转换目标和失败诊断。Matmul输出累加器必须正确初始化，不能把`tensor.empty`当成全零张量。使用MLIR已有parser/通用打印，无需写语言parser。

G部分实现loop-carried值、region映射、yield以及`tiny_loop.accumulate`到`scf.for`，理解类型转换而不是只拼操作名称。

**测试**：非法维度/rank/dtype、动态shape拒绝、无意广播拒绝、round-trip；循环零次/一次/多次及初值；每条路径经过Verifier。此阶段导出IR成功不算机器码执行成功。

**交付/证据**：小Dialect、shape契约、自己的lowering代码和反例；简短TPU源码地图在本阶段并行开展，但工时计入L6。

**失败简化**：先实现Matmul+Add两种Op，ReLU后补；仍保留累加初始化、诊断和测试。不开始完整ONNX importer。

### L3：Bufferization、生命周期所有权与CPU执行（44–58小时）

**前置**：L2；值语义、指针/别名、函数参数与资源所有权。

**来源**：U版本内Bufferization文档、Toy Ch6、ExecutionEngine工具。此pin工具名为`mlir-runner`。利用已有One-Shot Bufferize和lowering，不自行实现完整LLVM后端。

**亲自实现**：在自研C++工程注册并组合受限pipeline；写输入/输出ABI包装、RAII资源管理、执行测试工具；输出中间IR。说明哪些内存由调用者提供，哪些由生成函数创建/释放。必要时使用版本匹配的buffer deallocation流程，One-Shot Bufferize不等于自动完成所有释放。

**测试**：非方阵、多个消费者、读取旧值引发不可原地更新、函数结果所有权、重复调用无增长性泄漏。生成结果与独立参考实现比较；检查错误输入在编译前拒绝。不仅检查LLVM文本里出现了某个函数名。

**交付/证据**：可执行的端到端小编译器、ABI说明、结构+执行回归、失败定位链。

**失败简化**：先使用固定shape和调用者分配输出，减少返回动态memref的ABI复杂度；不能用悄悄回退NumPy替代编译执行。

### L4：Tiling、Fusion与合法性（50–70小时）

**前置**：L3正确性基线；迭代空间、索引映射、parallel/reduction维、张量依赖。

**来源**：U Transform教程/示例；G ch4 `exercises/ex1-l2-l1-no-fusion.mlir`、`ex2-l2-l1-with-fusion.mlir`、`TinyTransformOps.td/.cpp`，其中存在实际TODO。选定G练习10–16小时包含在本Lab。

**亲自实现**：填两级tiling/融合练习和自定义Transform Op；在U实现自己的C++调度选择/变换Pass，使用现有tiling接口但亲自处理匹配、tile约束、失败和返回值。先tile consumer再按合法依赖融合producer；同一项目维护baseline/tiling/fusion三条pipeline。

**测试**：矩阵大小不是tile倍数、tile大于维度、非法tile、fan-out、多层融合；Matmul的Add/ReLU不得在K归约未完成时提前应用。初版尽量保持归约顺序；fast-math与向量归约放到明确标记的扩展路径。特殊数值按契约检查；通用浮点误差用预定容差，不用放宽容差掩盖错误。

**交付/证据**：C++变换、前后IR、执行回归，多shape的CPU对照；至少解释一个无收益/回退案例。CPU加速不写成NPU实机加速。

**失败简化**：先只tile M/N，不拆K；先两算子融合、串行循环，不必立即实现并行或向量化。G环境失败时完成U同类作业，但不冒称G原题通过。

### L5：张量生命周期、片上内存预算与数据搬运（60–84小时）

**前置**：L4；拓扑依赖、生产/消费时点、物理buffer与逻辑tensor的区别。

**来源**：U自研工程；S固定版本`docs/developer_manual/source_zh/10_layergroup.rst`作概念对照。本Lab的规划器和“教学scratchpad设备”是自行设计，不是完整SOPHGO硬件模型。

**亲自实现（拟创建）**：

- `LiveIntervalAnalysis.cpp`：对固定串行schedule算生命周期，考虑fan-out、输出保留与权重驻留。
- `ScratchpadPlanner.cpp`：带对齐的分配/复用、峰值统计与超预算诊断，区分activation、weight和scratch。
- `TransferPlan.cpp`：输出load→compute→store事件及依赖，统计数据字节数，tile选择超预算时缩小/拒绝/退回已有路径。
- 一个独立CPU回放器和计划检查器，用于验证搬运顺序与计算结果，而不是估计真实NPU时延。

初版设备模型使用可配置的平坦片上存储，只承诺这一范围；真实NPU可能有分bank/分计算单元、布局、压缩和特殊对齐限制，不能用平坦总量替代真实可行性判断。预算值是实验配置，不把文档示意容量当作BM1684X参数。

容量检查以物理缓冲区为单位，例如：

`max_t(时点t仍存活的对齐后物理buffer占用 + scratch + 额外流水缓冲) <= 预算`

别名buffer不得重复计数，但不同内存空间不得混为一个地址池。包含权重、累加器、padding与输出，不能只算输入tile。内存区间在真正最后一次使用完成后才可复用；启用异步预取后，生命周期必须覆盖对应完成事件，不能只看提交时点。

**测试**：恰好装下/差1字节、对齐造成超量、扇出旧值仍被使用、多个输出、常量权重、尺寸乘积溢出、错误复用、遗漏load、过早store/复用。基础路径只做串行；双缓冲/重叠作为额外16–24小时扩展，不偷塞进基础工时。

**交付/证据**：规划器、独立检查器、可读事件trace、内存峰值与搬运字节数对照、功能结果。模型估计周期仅标为估计，不写成CModel/芯片实测。

**失败简化**：固定一个tile和串行schedule，先完成准确生命周期与容量校验；暂不做最优分组、多核和异步双缓冲。正确的保守规划器优于错误的“高性能调度器”。

### L6：TPU-MLIR转换、量化与验证（36–50小时）

**前置**：基础IR与shape；量化部分补scale/zero-point、舍入、饱和与更宽累加。6–8小时源码/依赖地图已提前到L2开展。

**来源/目标**：S，单一目标BM1684X；`python/tools/model_transform.py`、`model_deploy.py`、`run_calibration.py`、`model_runner.py`、NPZ比较工具；使用同版本说明和`--help`确认实际参数。调用已有ONNX转换器，不开发完整importer。

**亲自实现**：生成有限算子的小型ONNX测试图与固定输入；写阶段输出比较/失败定位工具；写量化/反量化参考函数并测试，再调用厂商校准流程。保留原模型、Top浮点、Tpu目标精度和CModel的不同阶段，不把一次相似度通过当作所有阶段验证。

**正确性**：先F32/F16数值对照再INT8；zero-range、outlier、饱和、per-channel轴错误、舍入与累加边界；改变校准输入检查漂移。真实任务精度报告需匹配的预训练模型与独立校准/验证集；随机小图只报告数值误差，不报告模型准确率。

**交付/证据**：环境/二进制来源清单、自己的数值工具与回归、逐阶段IR/NPZ、一次可定位的精度问题；只运行模型转换命令仍只是工具使用证据。

**失败简化**：先使用发行包完成Top/Tpu验证并继续阅读源码，源代码修改后的验证必须再确认运行的是自己的构建。若CModel库缺失/下载不可用，保留前端结果并标记CModel未验证；不声称“无板卡所以什么都做不了”，也不以GPU代替NPU仿真。

### L7：一个可审查的工业LayerGroup C++改动（60–84小时）

**前置**：L5规划器、L6能够定位和验证目标流水线；能够构建并确认使用自己的TPU-MLIR二进制。

**来源**：S：`lib/Dialect/Tpu/Transforms/LayerGroup/GroupMethod.cpp`、`BasicTimeStep.cpp`、`CycleCalculator.cpp`、`CoeffReloadOpt.cpp`及调用关系；CodeGen/Backend接口。`CoeffReloadOpt.cpp`展示权重驻留、生命周期和GDMA成本之间的取舍，但不能仅凭文件存在断言所选模式一定执行它，必须用日志/断点确认活动路径。

**亲自实现的基本任务**：在已经确认活动的分组/切分可行性检查路径，增加一个默认关闭的“教学预算上限”选项，以及结构化拒绝原因输出。选项名称由自己的patch定义，不是上游现有CLI。

这个上限只能保守收紧可用预算，不能修改硬件地址/ABI、扩大真实容量或绕过bank约束。必须找到该目标的容量口径（全局/每计算单元/每bank），不能用总量强行替换原可行性检查。上限改变时调用原有tile搜索/回退路径，而不是只改日志。此任务是一项资源约束下的编译规划策略修改，不预设会加速。

**扩展而非承诺**：在合法候选之间研究一个确定性的选择策略，或研究权重驻留/重加载；只有实际验证到收益/代价才报告。不要承诺找到上游真实bug或PR合入。

**测试**：关闭选项默认行为不变；非法/超出真实范围上限有诊断；小上限触发更小tile或保守回退；同一输入输出一致；多消费者/权重驻留/尾块/边界尺寸。清理或隔离相关编译缓存，日志证明命中修改路径；使用`command -v`、版本输出、Python模块路径排除pip包遮蔽源码构建。

**交付/求职证据**：基线commit、自己的C++diff、回归输入、策略启用/关闭对照、IR与搬运/内存统计、CModel结果及局限。没有真NPU只报告功能与规划统计，不报告芯片时延/功耗/带宽。

**失败简化**：源构建/SDK阻塞超过12–16主动小时先暂停，保留问题记录；在项目A的离线trace和规划器上实现同一策略并测试，但明确这不算完成TPU-MLIR后端/CModel改动。可展示项目A，不虚报项目B完成。

## 七、学习配比与先修边界

C++与Linux已有知识直接用于工程；遇到CMake、模板、RAII、容器或调试问题时补对应内容。算法练习可作为独立维护项，不设完成题量或熟练度门槛。

基础诊断用于定位当前Lab所需的缺口。能实现小函数、运行测试并解释结果，就继续做编译器任务；未通过时只补阻塞项，不把所有基础课重新学一遍。

xv6/QEMU从本方案的排期、验收、工时和简历替代项中移除。以后需要学习操作系统实验时另建独立任务。

### 数学与算法基础的阶段关联

知识参考为 [[16.AI编译器/02.AI编译器与MLIR的数学和算法基础|AI编译器与MLIR的数学和算法基础]]。下表把阅读位置和小检查关联到既有实验，不新增长期阶段，不改动前两周顺序、LLVM构建窗口、Lab工时估算和项目范围。基础阅读与练习计入对应Lab已有工时；若诊断发现额外缺口，记录实际补课时间，按原两周复盘机制处理。

| 使用阶段 | 阅读位置 | 随原实验完成的小检查 |
|---|---|---|
| Day 1：Demo 1动机与调用 | [[16.AI编译器/02.AI编译器与MLIR的数学和算法基础#2.1 矩阵乘法要能落到逐元素计算\|§2.1矩阵乘法]]、[[16.AI编译器/02.AI编译器与MLIR的数学和算法基础#3.1 先掌握当前项目实际使用的算子\|§3.1 MatMul与ReLU]] | 指出Demo 1的输入、权重、输出shape，解释共享维度512；用一个小输入手算一个输出元素及其ReLU结果。今天只需读懂调用，不要求实现矩阵库。 |
| 第二周Day 8–10 / L0：参考实现与测试 | [[16.AI编译器/02.AI编译器与MLIR的数学和算法基础#2.1 矩阵乘法要能落到逐元素计算\|§2.1]]、[[16.AI编译器/02.AI编译器与MLIR的数学和算法基础#2.2 张量形状与内存布局需要分开理解\|§2.2布局]]、[[16.AI编译器/02.AI编译器与MLIR的数学和算法基础#2.4 浮点数与误差从第一次执行验证就要使用\|§2.4误差]] | 用非方阵手算对照三重循环；推导一个元素的线性偏移；区分shape、dtype、stride；用独立NumPy参考和既定容差检查结果，说明NaN/Inf的处理。 |
| 第一周IR学习及L1：依赖与受限重写 | [[16.AI编译器/02.AI编译器与MLIR的数学和算法基础#2.5 图与依赖关系\|§2.5依赖图]]、[[16.AI编译器/02.AI编译器与MLIR的数学和算法基础#5. 数据结构与算法的最低要求\|§5遍历与工作列表]] | 在短IR中标注生产者、消费者与定义/使用；解释工作列表如何继续处理新出现的无用值，以及为何不能删除有副作用的操作。SSA、基本块和支配关系仍按原L1练习学习，不用拓扑排序替代控制流语义。 |
| L2–L3：算子语义、lowering与执行 | [[16.AI编译器/02.AI编译器与MLIR的数学和算法基础#3.1 先掌握当前项目实际使用的算子\|§3.1算子契约]]、[[16.AI编译器/02.AI编译器与MLIR的数学和算法基础#3.2 用 MatMul → Add → ReLU 串起数学与 MLIR\|§3.2逐元素公式与访问映射]]，复用§2.2、§2.4 | 写出ReLU(AB+R)的shape与一个元素的完整公式；指出并行维、归约维及累加初值；把访问映射对应到循环。Add继续要求同shape，不引入广播；L3将纸面推导与生成代码的实际执行结果对照。 |
| L4：分块与融合 | [[16.AI编译器/02.AI编译器与MLIR的数学和算法基础#2.3 整除、取模与边界直接用于分块\|§2.3尾块]]、§3.2；[[16.AI编译器/02.AI编译器与MLIR的数学和算法基础#4.8 编译优化：仿射分析、数据流与资源规划\|§4.8索引与变换合法性相关段落]] | 手推N=10、T=4的三个有效区间；用反例说明ReLU不能移进未完成的归约；检查fan-out和浮点顺序约束。先读懂具体索引映射，不要求先掌握完整多面体算法。 |
| L5及L7已有规划任务：生命周期与容量 | §2.5、§5及§4.8资源规划段落 | 对一条含多消费者的短schedule画生命周期；按实际元素大小与对齐计算物理buffer峰值；解释过早复用为何错误。L7再核对真实目标的容量口径，不用教学总容量代替设备约束。 |
| L6：量化与功能验证 | [[16.AI编译器/02.AI编译器与MLIR的数学和算法基础#4.5 低精度与量化方向\|§4.5量化]]、§2.4；均值、分布和异常值按校准实验需要补 | 按明确的scale、zero-point、舍入和饱和规则手算量化/反量化；与自己的参考函数对照；改变校准输入并记录误差变化。沿用原数值边界测试，不额外安排完整概率统计课程。 |

微积分、矩阵求导和自动微分不是当前推理编译初版的前置；Softmax、Attention、卷积、函数近似等在增加对应算子或选择扩展方向时再读。完整格理论、通用数据流框架、图着色和整数规划也不作为当前受限Pass的启动门槛。笔记第6节自查题随对应实验分批使用，不要求Day 1全部答完。

完成状态仍只更新本Task的原有验收项和学习记录。每次小检查保留学习者答案、检查结论、提示或纠正以及验证层次；阅读过、导师给出答案或新增链接都不计为掌握。本次仅建立关联，上表各项不因此记为通过。

## 八、硬件、租用与购买

### 学习机与后续构建平台

第一周用Mac M5 Air阅读、手写和标注IR，不安装完整LLVM/MLIR、Docker或TPU SDK。第二周继续在Mac做C++矩阵参考与NumPy对照，只补齐小实验需要的工具。记录实际版本与结果，不能把设备型号当作环境已验证。

PC已购买2TB固态硬盘；后续使用9700X＋48GB＋WSL2 Ubuntu作为主构建机。硬盘已购买不代表已安装或环境已验证，需在构建前确认。第一周继续使用Mac，不提前安排大型构建，暂不购买显卡。

第三周起在9700X主机上确认新SSD安装、WSL2 Ubuntu、虚拟化支持及C++工具链/CMake/Ninja。将WSL虚拟磁盘安排在新SSD上，源码与build放在其内部Linux文件系统；避免直接在/mnt/c或/mnt/d下构建大量小文件。记录WSL实际可用内存、CPU和磁盘余量，不把48GB主机内存当作WSL可用内存。候选构建使用Release并保留assertions、只启用MLIR及本机CPU后端，优先评估lld；按实际可用资源设置编译并发，链接先限制为1，再根据日志调整。实际参数以LLVM 20.1.8版本内说明和检查结果为准。

i7/16GB原生Ubuntu保留为备用；主构建机环境受阻时，可在确认备用机磁盘和工具链后继续。备用机从编译并发2、链接并发1的保守配置开始观察，不能据此认定构建已经通过。

100–150GB仍是工具链、源码、容器和缓存的整条路线规划余量，不是第一周最低要求或实测占用。后续先只准备U，核对源码、build与必要余量后再下载；不同时拉取全部教程工具链和厂商SDK。新SSD和WSL中的实际可用空间仍需检查；构建条件不满足时保留待办并继续已有小实验。

### 实验能力边界

| 环境 | 可以做 | 不能据此证明 |
|---|---|---|
| CPU/WSL2 | L0–L5、MLIR/C++测试、部分NPU编译、依赖就绪后的CModel | 真实NPU性能 |
| CModel | 支持目标/操作范围内的功能、数值和指令路径验证 | 未经标定的周期、真实带宽/功耗/并发竞争 |
| 1660 Super Ubuntu | 可选CUDA C++访存/归约/分块实验，匹配toolkit和架构 | 当前Triton的通用支持、Tensor Core专项 |
| 受支持现代GPU | 可选Triton/GPU代码生成与性能验证 | SOPHGO目标的SRAM/后端正确性或速度 |
| 对应真实NPU | 匹配Runtime/驱动/固件后的真实时延、吞吐、内存和功耗测量 | 其他芯片/其他精度/其他shape的普遍效果 |

当前Triton官方列NVIDIA Compute Capability 8.0+；并非任意“CUDA显卡”都可以跑当前教程。每个GPU实验还要单独核对使用的指令/类型/后端要求，不能只看最低CC。不要为了主线先购买显卡。

### 第一次值得租GPU的独立推算

当前暂不购买显卡。这条NPU主线没有必须租GPU的节点；只有选择CUDA/Triton或GPU后端运行与性能实验时，才需要兼容的GPU访问。不做GPU选修时，L0–L7都不因缺少新显卡而停止。

| 学习阶段 | 已具备的能力 | GPU安排 |
|---|---|---|
| L0–L3 | 参考测试、IR/SSA、Dialect/Pass、lowering和CPU执行 | 使用现有PC，不因这些任务租GPU |
| L4 | 完成分块融合，处理尾块、多消费者和归约合法性 | 可以开始记录GPU实验问题；本方案先继续完成L5 |
| L5验收后 | 能解释buffer生命周期、片上容量、数据复用与搬运量 | 若选择GPU选修且完成下方准备，再安排首次短租 |
| L6–L7 | TPU-MLIR转换、CModel与工业改动 | 不自动产生GPU需求；真实NPU性能测量另找匹配NPU资源 |

首次短租前须具备以下成果：

- L0–L5已验收；项目A能实际执行并对照参考，能解释分块、融合与内存规划的正确性边界。
- 独立CPU/NumPy参考、明确的dtype与误差容限，以及非方阵、尾块等边界测试。
- 一个可以比较的假设，例如“固定shape和dtype，比较几种tile大小的正确性与GPU耗时”，并选定基线。
- 本地准备好输入、依赖清单与批量运行脚本，明确预热、设备同步、计时范围和重复次数；租前核对GPU架构、显存与目标工具版本是否匹配。GPU代码仍须在首次上机时验证，不能提前记为通过。

入门GPU实验可从矩阵乘法分块或融合Softmax中选一个；参考[Triton官方教程](https://triton-lang.org/main/getting-started/tutorials/index.html)。先检查正确性，再分析性能，不把启动完整大模型训练作为准备条件。

按原预算，L0–L5合计272–370主动小时；每周20.4–25.5小时约对应11–18周，可按3–5个月量级理解首次评估窗口。构建等待、补课和课程变化会影响日历时间，必须按实际验收推进；该估算不代表届时必须租卡或买卡。

首次先安排小额短会话验证环境和最小用例，通过后再集中测量。原候选限额为1–2次、合计4–8个实例小时，不是已承诺消费或任务完成保证。尽量在本地准备代码与数据，记录实际开机时间和存储费用，用完停止计费实例并确认保留资源的费用。

对NPU目标，L7后可能更需要匹配的NPU短期访问。GPU测试结果不能作为BM1684X真实时延、吞吐或功耗证据。

### 购买决策

当前决定是暂不购买显卡，完成某个Lab或到达某个月份都不会自动触发采购。

先用短租完成实际实验，记录4–6周的使用频率、实例小时数、费用和环境恢复/传输耗时。只有持续、频繁地调试GPU代码，且租用或传输已明显打断学习时，才重新评估购买；偶尔集中运行实验则继续短租。

评估购买前应明确目标框架、GPU架构和显存需求，再比较预计使用期内的租用成本与购卡、电源/机箱适配、电费和维护成本。购买条件是稳定的实际需求和合理的成本，不是“已经学了几个月”。历史租用价格仅供估算，下单时重新核对；本次记录不代表授权租用或购买。

## 九、必修、选修、暂缓

**必修**：L0–L7与项目整理；有限Python/矩阵/浮点补课；U中的C++实现；S单目标案例。它们以验收推进，不能因日期到了自动算完成。

**短观察/选修**：TorchInductor CPU 4–6小时，用于对照框架捕获/重编译与底层代码生成；IREE CPU 6–8小时，用于对照编译产物和Runtime边界，二者首轮最多择一且不用从源码重建。它们不是SOPHGO后端或片上规划的前置。CUDA C++ 8–16小时；Bril补课4–8小时；双缓冲模型16–24小时；现代GPU实验另算本地准备和实例时数。

**暂缓**：完整parser、通用ONNX importer、完整LLVM后端、TVM另一整套项目、所有Toy章节逐一重写、完整Qwen/YOLO部署、FlashAttention追速、多卡、NPU驱动/固件。暂缓不是永远不学，而是不抢春季可展示项目与实习机会。

## 十、两个求职展示项目

### 项目A：`mini-npu-compiler-lab`（自己的MLIR工程）

最小版：静态二维f32的Matmul/Add/ReLU，ODS/Verifier、C++rewrite与lowering、CPU执行、tiling/fusion、边界测试。L4完成即可发布真实的v0.1，不等待全部NPU实验。

增强版：L5的固定schedule生命周期、带对齐的片上预算规划、显式搬运事件与独立检查器。声明这是教学目标模型，不声称已实现SOPHGO后端。合理展示“在给定教学容量和schedule下减少多少分配/搬运字节”，只填运行后得到的数字。

展示物：自己的核心C++diff、pipeline图或文本、自动测试、失败样例、原始结果、复现方法、2–3分钟演示。首轮代码贡献明确，不能把完整教程答案算成全部个人实现。

### 项目B：`tpumlir-layergroup-study`（固定版本工业改动）

范围：BM1684X、少量可支持小图、F16/INT8阶段验证，外加L7保守预算策略改动及诊断。它不是项目A换个名字，核心证据是工业代码实际改动及对应回归。

展示物：上游基线SHA、自己的补丁、路径追踪、默认/受限预算对照、CModel结果。若缺真机，简历不写实机性能；若SDK或构建未过，不能把离线分析写成工业后端实现完成。上游PR可以作为后续目标，不保证被接收。

L3先形成可演示的CPU执行闭环，L4发布A初版；L5完善规划，B按环境和实际改动推进。阶段顺序与验收优先于固定月份，投递时逐项核对目标JD。

## 十一、从现在起前两周具体任务

2026-10-18为Day 1；日期保留为起步参照，未完成任务顺延，不要求机械追赶。第一周全部用Mac，第二周仍以Mac的小实验和阅读为主。Day 7与Day 14留机动。候选4–5小时是整日编译器学习容量，不是连续看视频的要求；按阅读、手推、编码和复盘分别记录。每天只看20–30分钟可以用于预习，不能据此认定两周已掌握全部主题。

以下资料的直接链接见设计中的“入门资料与使用顺序”。每次学习都保存可检查成果；纸面IR和手推结果注明“未运行验证”。

| 日期 | 学习与实验（Mac） | 当日可检查成果 |
|---|---|---|
| 10/18 · Day 1 | 看Beginner-Friendly第一期的动机部分、读Demo 1；认识前端/中端/后端、AST与IR | 用一个加法函数写出编译阶段说明；记录Mac已有工具，不安装LLVM |
| 10/19 · Day 2 | 学SSA、定义与使用；将直线赋值例子改写为SSA形式 | 标注每个值的定义与使用，解释“静态单赋值” |
| 10/20 · Day 3 | 学Basic Block、CFG和分支汇合；阅读LangRef相关小节 | 手画一个if/else控制流，说明汇合块参数如何接收不同分支传来的值 |
| 10/21 · Day 4 | 读IR Structure；标注func.func、arith.addi、func.return短例子 | 区分Operation、Value、Region、Block；指出入口块参数和操作结果 |
| 10/22 · Day 5 | 学Dialect、Type与Attribute；选读Toy Ch1–2 | 解释func/arith命名空间；区分值的类型与操作属性；说明AST到MLIR的关系 |
| 10/23 · Day 6 | 读Beginner-Friendly Demo 2的Matmul/ReLU片段，复习use-def | 标注输入输出shape和producer/consumer；能读懂加法例子，并列出张量例子仍不懂的部分 |
| 10/24 · Day 7 | 机动/休息，补未通过的概念检查 | 整理问题，不开启源码构建 |
| 10/25 · Day 8 | 手算小矩阵，写C++17三重循环参考；检查Mac必要编译工具 | 小程序编译运行记录与手算对照；若失败，保留具体阻塞 |
| 10/26 · Day 9 | NumPy shape/dtype/stride/转置与独立矩阵对照 | C++与NumPy对照；确认两条实现独立，明确比较时的dtype |
| 10/27 · Day 10 | 加非方阵、单元素、K不匹配和失败退出码；补f32误差、NaN/Inf与广播边界 | 输入契约与边界测试；小型CMake/测试入口，工具缺失则记录待办 |
| 10/28 · Day 11 | 选读Toy Ch3，认识Pass与Pattern Rewrite；手推整数addi(x,0)和DCE | 一组改写前后IR；解释store/call不能因结果无人使用就随意删除 |
| 10/29 · Day 12 | 看Beginner-Friendly第二期相关片段与Demo 3，观察逐步lowering | 写出tensor→buffer→循环→LLVM表示的变化；区分lowering与性能优化，不声称已执行 |
| 10/30 · Day 13 | 复盘IR/SSA、矩阵测试和未验证项；核对9700X新SSD安装与WSL2 Ubuntu准备情况 | 两周报告；第三周构建前检查表、已知磁盘/工具链阻塞，暂不下载大型源码 |
| 10/31 · Day 14 | 机动/休息 | 按实际成果调整后续工时，未完成项顺延 |

两周结束时应能标注简单MLIR、解释SSA与分支传值，保留矩阵参考/独立对照/边界测试，并能解释一次重写与lowering。没有安装MLIR时，手写MLIR的语法合法性及其执行结果仍是待验证项；不勾选L0完整工程验收或L1 Pass实现。

### 第三周起：满足条件后构建并进入工程

依次完成：确认9700X新SSD、WSL2 Ubuntu及其实际可用资源和工具链 → 获取固定U源码并记录SHA → 受控构建 → standalone独立工程与自己的测试 → 补齐L0验收 → 实现L1的use-def报告和受限Pass。Grover独立练习环境也后移到主工程稳定后，按原L1预算选做规定题目。

开始构建前应能解释简单IR中的定义/使用与结构，且已完成小型C++编译运行和矩阵对照；WSL2 Ubuntu资源尚未确认时，不承诺某一天构建完成。环境受阻期间继续参考测试、手推IR与重写反例，实际实现/运行验收仍须补齐。

## 十二、统一验收和证据模板

每个Lab至少保存：问题/范围、前置、上游commit、自己的commit或patch、运行环境、构建命令、结构测试、执行测试、边界/反例、原始数据、已知限制。

验证等级分别写：

1. 语法检查：例如AST解析、`bash -n`；不证明依赖可用。
2. 编译成功：目标二进制和库真实产出；不证明结果正确。
3. 运行正确：明确输入集合与参考结果通过；不等于形式化等价性。
4. 性能验证：同设备、相同shape/dtype/layout/线程/同步/计时边界、预热与缓存状态，重复采样并保留原始数据。

FileCheck不替代执行测试；数值容差通过不证明任意输入语义保持；CModel不替代真机性能。区分编译/加载/首次调用/稳态，区分模型kernel与包含拷贝的端到端时间。仅跨机器比较，不能把全部差异归因于某个Pass。

## 资料索引

### 入门阅读

- [Beginner-Friendly仓库](https://github.com/AlexandreSinger/mlir-beginner-friendly-tutorial)、[第一期视频](https://youtu.be/Uno_XhtkT5E)、[第二期视频](https://youtu.be/l0O4Vbc3l5c)
- [IR结构](https://mlir.llvm.org/docs/Tutorials/UnderstandingTheIRStructure/)、[Toy Ch1](https://mlir.llvm.org/docs/Tutorials/Toy/Ch-1/)、[Ch2](https://mlir.llvm.org/docs/Tutorials/Toy/Ch-2/)、[Ch3](https://mlir.llvm.org/docs/Tutorials/Toy/Ch-3/)

### 核心源码/文档
- LLVM pinned源码：[llvmorg-20.1.8](https://github.com/llvm/llvm-project/tree/llvmorg-20.1.8)
- [MLIR语言参考](https://mlir.llvm.org/docs/LangRef/)、[Bufferization](https://mlir.llvm.org/docs/Bufferization/)、[Transform教程](https://mlir.llvm.org/docs/Tutorials/transform/)
- [Grover ch1](https://github.com/Groverkss/mlir-tutor/tree/9cae1b9e585638c7e6f6de22a40fce23bd9d336d/tutorial/ch1-cpu-vector-dsl)
- [Grover ch2](https://github.com/Groverkss/mlir-tutor/tree/48e89a92127af8636cc325560d131d8edcc16bb6/tutorial/ch2-cpu-vector-dsl-loops)
- [Grover ch4](https://github.com/Groverkss/mlir-tutor/tree/8352d03164ffb4f25d5d28a0de0de113e81d1b0d/tutorial/ch4-linalg-transforms)
- [TPU-MLIR源码](https://github.com/sophgo/tpu-mlir/tree/ecc9dd6c32e79517807610c51a9bbfdd66c41029)
- [版本内环境说明](https://github.com/sophgo/tpu-mlir/blob/ecc9dd6c32e79517807610c51a9bbfdd66c41029/docs/developer_manual/source_zh/02_environment.rst)
- [版本内LayerGroup说明](https://github.com/sophgo/tpu-mlir/blob/ecc9dd6c32e79517807610c51a9bbfdd66c41029/docs/developer_manual/source_zh/10_layergroup.rst)
- [版本内CodeGen说明](https://github.com/sophgo/tpu-mlir/blob/ecc9dd6c32e79517807610c51a9bbfdd66c41029/docs/developer_manual/source_zh/12_codegen.rst)
- [实际LayerGroup源码](https://github.com/sophgo/tpu-mlir/tree/ecc9dd6c32e79517807610c51a9bbfdd66c41029/lib/Dialect/Tpu/Transforms/LayerGroup)

### 环境

- [WSL资源配置](https://learn.microsoft.com/en-us/windows/wsl/wsl-config)
- [WSL文件系统建议](https://learn.microsoft.com/en-us/windows/wsl/filesystems)
- [Triton兼容性](https://github.com/triton-lang/triton#compatibility)

来源原稿保留用于追溯；当前路线以本Task的最新文档为准。核心Pass、Dialect、规划器与工业补丁需要逐步实现。

## 工时计算与重排

L0至发布共386–530主动小时，中值458。可用容量20.4–25.5小时/周，中值22.95，对应约15–26周、中值20周。xv6工时已完全移除。基础不足时按具体阻塞单列补课时间，不降低数值正确性要求。

两周后用实际投入和通过的验收项重新估算。L3形成可演示闭环，L4即可发布A初版；B与选修不阻塞A发布和投递。

## 学习与交付验收清单

以下勾选只在实际通过验收时更新。

### L0：参考与工程

- [ ] 完成Python/NumPy、矩阵和浮点基础诊断，记录需要补课的缺口。
- [ ] 完成C++矩阵参考、独立NumPy对照和非法输入测试。
- [ ] 在实际主机记录CPU、可见RAM、磁盘与工具链；模板和自己的首项测试通过。

### L1：受限重写

- [ ] 完成编译流程、IR/SSA、CFG及MLIR结构标注练习，解释块参数与use-def；注明纸面推导和运行验证的区别。
- [ ] 对照实例解释Pass、Pattern Rewrite与lowering的作用和语义约束。
- [ ] 完成use-def分析、受限DCE和整数addi(x,0)改写。
- [ ] 副作用、块参数、位宽和浮点不改写反例通过。
- [ ] 完成选定Grover ch1题或注明同类自设计替代，保留自己的代码与测试。

### L2：Dialect与lowering

- [ ] 定义静态二维f32的有限算子、Verifier及明确的ReLU语义。
- [ ] 完成到Linalg的C++转换，正确初始化Matmul累加器。
- [ ] 非法shape、dtype、动态维度、广播拒绝和round-trip通过。
- [ ] 完成TPU源码/依赖地图6–8小时；计入L6，不重复叠加。

### L3：执行与所有权

- [ ] 接通bufferization、循环/LLVM lowering及生成代码的实际CPU执行。
- [ ] 明确输入输出ABI、别名和所有权，重复调用与释放路径通过。
- [ ] 生成结果与独立参考一致，保存失败定位记录。

### L4：分块与融合

- [ ] 完成baseline/tiling/fusion流水线及自己的C++调度/变换。
- [ ] 尾块、fan-out、归约完成顺序和非法tile测试通过。
- [ ] 发布项目A v0.1：可复现执行、优化对照及至少一个回退/无收益案例。

### L5：教学片上规划

- [ ] 完成固定schedule生命周期、带对齐的分配与复用、容量检查。
- [ ] 完成load/compute/store事件、搬运字节统计和独立CPU回放/检查器。
- [ ] fan-out、输出保留、对齐超量、溢出、过早复用和遗漏搬运测试通过。
- [ ] 发布项目A增强版，明确教学模型范围与硬件测量边界。

### L6：TPU转换与功能验证

- [ ] 匹配环境中完成少量ONNX图的Top/Tpu阶段比较。
- [ ] 完成量化参考函数、校准漂移及数值边界测试。
- [ ] CModel功能结果通过；若不可用则明确记录未验证，不勾选通过。

### L7：工业改动

- [ ] 构建并确认使用自己的TPU-MLIR二进制，日志/断点证明活动路径。
- [ ] 完成默认关闭的预算约束起步题，容量口径、切片/回退与默认回归通过。
- [ ] 记录工业补丁及失效/回退反例；深入候选策略、诊断或真实问题修复的工时另列。
- [ ] 汇总项目B的实际贡献、回归、功能/规划对照和限制，决定是否达到简历项目门槛。



### 发布、简历与投递

- [ ] 复查干净环境构建、执行和测试命令，保存自己的核心diff与版本信息。
- [ ] 每个项目准备2–3分钟演示和一个失败定位/修复案例。
- [ ] 依据实际完成度选择A、B和MFMS，不要求凑满三个项目；逐项核对目标JD。
- [ ] 更新面试/投递记录与剩余工时，不等待全部选修完成。

## 费用与采购记录

已有设备承担主线，学习资料和开源工具可以免费获取。已购买PC用2TB固态硬盘，金额待记录；暂不购买显卡，当前不计划其他新增硬件采购。电费、已有订阅、可选书籍和未来扩容单独记录。CModel等依赖尚待实际取得和验证，不能凭计划文本认定已运行。

GPU不属于必修。2026-09-30查询[AutoDL官网](https://autodl.com/home)得到3090约1.32元/实例小时、4090约1.88元/实例小时；下单时以实际主机价格为准。8/50/100实例小时分别约10.56/66/132元和15.04/94/188元。先预留300元作为第一轮可选云实验限额，不等同于课程总费用或必需消费。

按[AutoDL计费说明](https://www.autodl.com/docs/price/)，开机的安装、编译和调试也计费，付费存储另算。本地准备好代码、边界测试和批量脚本，再短租验证环境和集中测量。

真NPU测量单独安排报价或设备访问，不能使用GPU租金代替NPU价格。[算能官方课程页](https://sophgo.com/curriculum/online.html)列有可申请的免费SE5空间；当前可用性、芯片型号、SDK与权限仍待确认，不认定为已经取得的BM1684X资源。

## 每周工时与验证记录模板

每周只更新本Task的权威记录，再同步Obsidian投影。主动实现、阅读、调试和测试分别记录；等待时间单列，不与同时进行的阅读重复计时。

| 周起止 | 编译器主动小时 | 面试/算法/MFMS小时 | 已通过验收 | 未通过/阻塞 | 下周剩余工时 |
|---|---:|---:|---|---|---|
| 2026-10-18起 | 待记录 | 待记录 | 尚无Lab通过记录 | IR/SSA入门待验收；新SSD安装与WSL2构建环境待验证 | 两周后按实际记录校准 |

每份测试或结果注明：环境与版本、自己的commit、输入及参考、是否结构/执行/性能验证、原始输出和已知限制。保存计划时不填写学习完成次数和性能数字。

## Day 1 学习记录（2026-10-19续学）

仍按Day 1推进，不因日期变化自动进入Day 2。以下证据来自本次交互问答，均为纸面理解检查，未编译或运行；主动学习时长未记录。任务仍在L0，L0/L1整体验收清单不变。

- [x] 表达式树基础检查：学习者正确指出`a + b`的根为加法、两个子节点为a和b。导师纠正了把return误解为赋给临时变量的说法；完整函数AST尚待学习者独立补画。
- [x] 直线伪IR练习：学习者为`return (a + 0) + b`独立写出`t0 = integer_add a, 0; t1 = integer_add t0, b; return t1`。第一次优化答案仍保留两次加法，随后借助题目提示自行纠正为`integer_add a, b`；不记为首次独立完成优化。
- [x] 阶段职责检查：学习者正确把消除整数加零归到中端，把ARM指令选择归到后端。已讲解IR是程序表示，中端执行分析和变换。
- [x] 简单复用与正确性检查：在整数语义一致的前提下，学习者判断新语言前端输出相同IR后可以复用加零优化；关闭该优化仍能正确执行，并指出优化需保持结果。

已讲解但未单独复测：汇编器将汇编代码转换成目标文件；作用域与生命周期的区别；静态链接按需抽取目标文件；动态库替换的ABI条件。优化必须保持可观察行为、且不保证每次都加速的补充说明待继续讲解。

Day 1剩余项：

- [ ] 学习者独立补画原始add函数的简化AST，并对应AST、教学伪IR和目标代码阶段。
- [ ] 观看第一期动机部分并完成理解检查；尚未收到观看完成反馈。
- [ ] 阅读Demo 1并解释保留高层运算信息的作用；导师已核对README，不能替代学习者完成阅读。
- [x] 检查并记录Mac已有工具与实际版本，不安装完整LLVM、Docker或TPU SDK。探测与Demo 1编译结果见下方；不代表项目A工具链已验证。
- [ ] 完成Day 1综合复述与验收。

### Mac工具检查与Demo 1运行探测（2026-10-19）

Demo存放约定（2026-10-19用户指定）：后续Demo统一放在`~/project/milr/`下，每个Demo使用独立子目录，保留`milr`拼写。Demo 1已从临时目录迁移到`~/project/milr/demo1-mlir-motivation/`，包括三个源码文件和已编译的`demo1`；迁移前后逐文件SHA-256一致。目录变更不计为新增学习验收。

由助手在当前Mac执行，未安装任何依赖。系统架构为arm64，已配置Apple Command Line Tools。PATH中找到Apple clang 21.0.0（clang-2100.1.1.101，target为arm64-apple-darwin25.5.0）、Python 3.14.4、Git 2.53.0；也找到g++命令，但未单独验证其身份与版本。PATH中未找到CMake、Ninja，不据此断言整台机器没有其他安装。

仅将教程commit `02240bfc475cc87eca802c6e0e2871bc9aff5f23` 下的 `demo1-mlir-motivation/demo1.cpp`、`tensor.h`、`linalg_lib.h` 下载到系统临时目录，未修改源码，未获取LLVM子模块或执行Demo 0。

在三份文件所在目录执行：

```sh
clang++ -std=c++17 -O2 -Wall -Wextra demo1.cpp -o demo1
./demo1
```

编译退出码0，无诊断；运行退出码0，标准输出和标准错误均为空。该示例仅依赖C++标准库，当前Mac可以直接编译运行。源码的float向量默认初始化为零，未打印结果或包含数值断言；本次只证明原始Demo 1可编译且程序正常退出，不构成独立数值验证、性能测量、MLIR工程验收或学习者亲自运行的证据。未消费的计算还可能被优化器删除，不能依据运行耗时判断矩阵计算性能。项目A仍固定LLVM/MLIR 20.1.8，Apple clang版本不替代该基线。

## Task JSON

来源：`.trellis/tasks/09-30-npu-compiler-study/task.json`

```json
{
  "id": "npu-compiler-study",
  "name": "npu-compiler-study",
  "title": "AI编译器与NPU编译优化学习（现在开始）",
  "description": "2026-10-18启动：第一周用Mac学习编译流程、IR/SSA与MLIR入门教程；第二周做矩阵参考、测试和重写/lowering阅读。LLVM构建与独立MLIR工程后移到第三周起，主构建机为9700X＋48GB＋WSL2 Ubuntu，PC已购2TB固态，待环境验证后推进；项目A/B按验收继续。",
  "status": "in_progress",
  "dev_type": "study",
  "scope": "ai-compiler-npu",
  "package": null,
  "priority": "P1",
  "creator": "melene",
  "assignee": "melene",
  "createdAt": "2026-09-30",
  "completedAt": null,
  "branch": null,
  "base_branch": "main",
  "worktree_path": null,
  "commit": null,
  "pr_url": null,
  "subtasks": [],
  "children": [],
  "parent": null,
  "relatedFiles": [
    ".trellis/tasks/09-30-npu-compiler-study/prd.md",
    ".trellis/tasks/09-30-npu-compiler-study/design.md",
    ".trellis/tasks/09-30-npu-compiler-study/implement.md",
    ".trellis/tasks/09-30-npu-compiler-study/research/source-manifest.json",
    "16.AI编译器/01.AI编译器与NPU培养方案.md",
    "16.AI编译器/02.AI编译器与MLIR的数学和算法基础.md",
    "16.AI编译器/00.学习导航.md",
    "16.AI编译器/90.学习与实践/09-30-npu-compiler-study.md"
  ],
  "notes": "2026-09-30保存原方案；2026-10-18用户明确提前启动并移除xv6，将方案和任务投影迁入16.AI编译器。保留原始研究资料，取消11月11日门槛。当前进入L0，尚无Lab通过记录。 本轮按用户要求提前IR/SSA与入门教程，第一周用Mac，第三周起评估LLVM构建；9700X原有Windows磁盘空间不足；现已购买2TB固态，后续使用9700X＋48GB＋WSL2 Ubuntu作为主构建机，i7 Ubuntu备用，暂不购买显卡。仅调整计划，未完成任何新Lab。",
  "meta": {
    "domain": "study",
    "start_date": "2026-10-18",
    "target_date": null,
    "graduation_year": 2028,
    "user_total_daily_study_hours": "超过8小时",
    "compiler_daily_hours_assumed": [
      4,
      5
    ],
    "main_study_days_per_week_assumed": 6,
    "weekly_compiler_capacity_after_reserve": [
      20.4,
      25.5
    ],
    "core_active_hours": [
      386,
      530
    ],
    "core_active_hours_midpoint": 458,
    "date_semantics": "按阶段验收；原工时估算约15–26周、中值20周。L0工程验收移至第三周起，后续窗口在两周复盘后重估，不设固定截止日",
    "hardware_os_9700x": "Windows；后续主构建环境为WSL2 Ubuntu，安装与配置待验证",
    "gpu_required": false,
    "first_gpu_optional_window": "L0–L5验收后，备齐GPU假设/基线、CPU参考、边界测试与测量脚本再考虑短租；约3–5个月仅为原工时估算，未选修则无需租用",
    "optional_cloud_budget_cny": 300,
    "npu_hardware_budget": "待匹配真机访问/报价，不包含在GPU预算内",
    "study_started": true,
    "current_lab": "L0",
    "obsidian_detail_path": "16.AI编译器/90.学习与实践/09-30-npu-compiler-study.md",
    "xv6_in_scope": false,
    "estimated_core_weeks": [
      15,
      26
    ],
    "initial_study_device": "Mac M5 Air",
    "build_host_candidate": "9700X＋48GB＋WSL2 Ubuntu（已选定主构建机；环境待验证）",
    "llvm_build_window": "第三周起；先通过入门理解检查、完成矩阵参考，并验证9700X新SSD与WSL2 Ubuntu构建环境",
    "backup_build_host": "i7/16GB原生Ubuntu；空间与工具链待确认",
    "pc_ssd": "2TB固态硬盘已购买；安装与实际可用空间待验证，金额待记录",
    "gpu_purchase_plan": "暂不购买显卡；先短租实践并记录4–6周，形成持续频繁需求、明确架构/显存和成本后再评估，不按日期或Lab自动采购",
    "demo_workspace": "~/project/milr",
    "demo_directory_policy": "后续AI编译器学习Demo统一放在demo_workspace下，每个Demo使用独立子目录；保留用户指定的milr拼写。"
  }
}
```
