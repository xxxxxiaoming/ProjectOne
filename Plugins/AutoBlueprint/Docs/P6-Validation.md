# P6 可靠性、环境隔离与资产保护验证

P6 **已通过 UE 5.3.2 Win64 本地验收**。完整编译与 UHT、P6 五项自动化、P0–P5 回归，以及脚本异常后的控制台结果、PIE/SIE 环境隔离、游戏 Lua 启动与运行、编辑器退出与重开均已通过。验收记录见 `3a14338`（2026-11-26），验证状态以第 5 节“检查记录与本地验收”为准。

## 1 编译与调用入口

关闭编辑器，完整编译 `ProjectOneEditor / Development Editor / Win64`，再启动编辑器。本次新增反射结构和字段，需要完整编译/UHT，不以 Live Coding 代替。

普通编辑状态、无需 Play，执行：

```text
Automation RunTests AutoBlueprint.P6
```

然后依次回归：

```text
Automation RunTests AutoBlueprint.P5
Automation RunTests AutoBlueprint.P4
Automation RunTests AutoBlueprint.P3
Automation RunTests AutoBlueprint.P2
Automation RunTests AutoBlueprint.P1
Automation RunTests AutoBlueprint.P0
```

P6 自动化准备 GUID 专属的布局、PNG、阻挡保存的普通文件。测试结束清理这些临时输入，以及本次未保存对象的 Standalone/脏标记；已保存 Widget、纹理、JSON 报告保留供检查。不会清理用户已有资产。`DirtyAssetsAndLateConflict` 会在自己的 GUID 目标创建一个临时 `.uasset` 文本占位文件，用来证明写入器不会覆盖保存前突然出现的目标；测试退出时仅删除这个占位文件。

`tests/p6_outcomes.lua` 是自动化夹具，不是常规生成入口：它需要测试准备的资源布局路径、新的 smoke Widget 路径和 `throw`/`return` 模式，会真实生成资产。日常仍使用 `AutoBlueprint.GenerateFromFile <布局路径>`。

## 2 本阶段修改了什么

### 脚本失败时保留每次生成结果

以前脚本生成资产后再抛错，宿主的 `FAutoBlueprintLuaRunResult` 只有失败信息，具体资产结果需从各次报告和日志另找。现在增加只读反射字段 `Operations`，由子系统在生成调用返回前记录值副本，运行结束后转交调用方。即使 Lua 抛错，其前面已经完成的调用仍可查询。

例如同一脚本先导入两张图片，第一张保存成功、第二张失败，又生成一个不保存的 Widget，最后抛错：

| 结果层级                                      | 正确含义                             |
| ----------------------------------------- | -------------------------------- |
| LuaRunResult.bSuccess=false               | 脚本没有正常完成；Code/Message 保留错误位置和调用栈 |
| Operations 中的资源任务 bSuccess=false          | 该次生成任务没有完成所有请求步骤                 |
| 第一张纹理 bImported=true、bSaved=true          | 文件已保存，脚本失败不会回滚它                  |
| 第二张纹理 bImported=true、bSaved=false         | 对象已导入到内存，但没有成功保存                 |
| 资源任务的 Widget bCompiled=true、bSaved=false  | Widget 已编译，但因资源保存失败而停止保存         |
| 另一条不保存的 Widget bSuccess=true、bSaved=false | 该次调用按 bSave=false 完成；不能把它当作已落盘   |

`bSuccess` 仍只描述各自层级。脚本主动处理生成失败并返回 true 时，宿主可以成功，但 Operations 中失败的生成仍为 false。没有新增整体 `bSaved`，没有实现 GenerateBatch，也不执行失败回滚。

### 新增反射字段与兼容性

| 字段 | 说明 |
| --- | --- |
| FAutoBlueprintLuaRunResult.Operations | `TArray<FAutoBlueprintScriptOperation>`，按生成调用完成顺序排列；不包含校验或被拒绝的嵌套脚本请求 |
| FAutoBlueprintScriptOperation.Operation | 实际子系统入口名称，例如 GenerateFromLayoutFile、BuildWidgetBlueprint |
| FAutoBlueprintScriptOperation.Result | `FAutoBlueprintGenerationResult`；任务调用保留 Widget/Resources/Diagnostics/ReportFilePath；旧 Build 调用填入 Widget，并复制成功状态和报告路径，Resources 为空 |

成功的 BuildSmokeTestWidget 委托 BuildWidgetBlueprint，只记录一次，入口名为 BuildWidgetBlueprint；smoke 参数预检失败则记录为 BuildSmokeTestWidget。记录覆盖进入当前脚本子系统的 Build/Generate 调用，包括返回失败的调用；门面在进入子系统前拒绝的上下文请求不加入记录。它不追踪可信脚本通过其他 UE API 写入的资产，空 Operations 不证明任意脚本没有副作用。

既有字段、Schema v1 和逐次版本 1/2 JSON 报告不变。Operations 是此次宿主返回的内存记录，随调用结果保留，不新增脚本汇总文件；已有生成报告路径在记录中可查。`bWriteReport=false` 仍不写生成报告，但不会关闭内存记录。直接 C++ 生成不会污染下一次脚本记录；每次脚本退出后子系统不保留历史记录、VM、回调或 UObject 引用。

控制台现在无论脚本成功或失败，都输出每条生成记录的 Widget、纹理创建/编译/保存状态及报告路径。Help 的旧 P3 能力说明也更新为当前 11 类控件与资源任务。

### 实现文件

| 文件 | 用途 |
| --- | --- |
| Public/AutoBlueprintLuaTypes.h | 新增可被 UnLua 读取的记录结构与 Operations 数组 |
| Public/AutoBlueprintSubsystem.h、Private/AutoBlueprintSubsystem.cpp | 收集本次脚本的生成结果，退出时交给宿主结果，防止下一任务读到旧记录 |
| Public/AutoBlueprintLibrary.h | 说明脚本状态、生成记录和无回滚契约 |
| Private/AutoBlueprintEditorModule.cpp | 控制台列出生成记录，并更新当前能力说明 |
| Private/Scripting/EditorLuaRunner.h/.cpp | 仅在 WITH_DEV_AUTOMATION_TESTS 下提供实例级故障开关，测试宿主拿不到 Lua state 时的诊断与清理；正常执行路径保持独立 FLuaEnv |
| Private/Tests/AutoBlueprintP6Tests.cpp | 五项真实 UE 自动化，操作对象、磁盘包、GC 和真实 UnLua |
| Scripts/Lua/tests/p6_outcomes.lua | 混合执行拒绝/保存/不保存调用，再抛错或正常返回，供 UE 测试检查宿主记录 |
| Scripts/Tests/p6_host_test.lua | 在离线 Lua 中运行实际 bootstrap 和上述夹具，用模拟 UE 验证执行顺序和异常传播；不验证 C++ 记录或资产 |

以上 Source 路径相对 `Source/AutoBlueprintEditor`。未修改引擎、UnLua、资产保存顺序、CreateOnly 或 P4 纹理异步编译修复。

## 3 测试具体做什么

| 测试 | 实际操作及通过标准 |
| --- | --- |
| AutoBlueprint.P6.ScriptOutcomesAndRecovery | 三轮独立任务：完整保存后抛错、部分保存后抛错、部分保存后正常返回。每轮混合五次生成调用，检查宿主状态与各次结果独立、顺序正确、报告和磁盘相符；下一空操作任务不继承记录，旧结果不变；既有 VM/游戏 Lua 激活状态不变；换 Widget 名重试仍拒绝已有纹理 |
| AutoBlueprint.P6.DirtyAssetsAndLateConflict | 分别创建已保存和仅内存的 Widget，修改标题并置脏，再请求同名生成；检查对象身份、文字、脏状态和已有文件都保持不变。另在写入器保存前放入测试专属目标占位，证明保存前再次检查冲突 |
| AutoBlueprint.P6.ResourceLifetimeAndSourceRecheck | 图片预检通过后，将自己的源文件改为 LFS 指针，再进入资源解析；必须拒绝导入并标明后续资源尚未尝试。另创建不带 Standalone 的临时纹理/字体，使用编辑器 GARBAGE_COLLECTION_KEEPFLAGS 保留其他资产；缓存持有期间 GC 后仍有效，缓存释放后再 GC 应回收 |
| AutoBlueprint.P6.ContextAndInitializationRecovery | 模拟宿主未获得 Lua state，检查 LUA_ENV_INIT_FAILED、VM 释放及同一 runner 重试；工作线程请求生成返回 WRONG_THREAD；保存上下文返回 EDITOR_BUSY；未初始化子系统拒绝脚本；拒绝后正常入口仍可用 |
| AutoBlueprint.P6.ReflectedOperationResults | 由另一独立 FLuaEnv 通过真实 UE 反射调用宿主，读取非空 Operations、嵌套 Widget/Resources 和报告路径；工具 VM 销毁后结果仍可读，后续调用不改旧结果，外部调用者 VM 保持独立并最终释放 |

P6 继续使用已有测试覆盖其他可靠性场景，避免重复实现同类夹具：

| 场景 | 验证依据 |
| --- | --- |
| 缺图、LFS 指针、错误资源类型、命名与字段/Slot/控件错误 | P2、P4.PreflightNoImports、P5.InvalidLayoutsNoWrites，加上 P6 源文件重查 |
| 蓝图编译失败不保存任何新包 | P3.CompileFailureDoesNotSave、P4.CompileFailureSavesNothing |
| 第二张纹理或最终 Widget 保存失败、逐包报告真实 | P4.PartialSaveReports，加上 P6 脚本异常后的记录与磁盘检查 |
| 模块循环、加载/语法异常、受控路径、失败后重试 | P1 自动化及离线 bootstrap 回归 |
| PIE/SIE、实际游戏 Lua 启动、编辑器关闭/重开 | 第 5 节人工验收；不通过伪造 PlayWorld 或强制卸载反射模块来代替 |

初始化测试模拟的是“已构造环境后，宿主未取得 state”的返回分支，不是真实内存耗尽测试。UnLua 2.3.6 构造函数会直接初始化 Lua 库；引擎或第三方库在构造内部发生的致命分配失败不属于可恢复脚本错误，本阶段没有修改第三方实现或承诺处理进程崩溃。

## 4 错误码与边界

| 错误码 | 简要说明 |
| --- | --- |
| LUA_SCRIPT_FAILED | 工具脚本/模块未正常完成；应同时检查 Operations，不能据此推断没有写资产 |
| LUA_ENV_INIT_FAILED | 宿主未取得 Lua state，脚本未开始执行 |
| WRONG_THREAD | 在编辑器游戏线程之外调用，入口拒绝 |
| EDITOR_BUSY | PIE/SIE、GC、保存、重入或非空闲上下文，拒绝新任务 |
| SUBSYSTEM_UNAVAILABLE | 子系统不存在或不接受工具请求 |
| ASSET_ALREADY_EXISTS | 包在内存或磁盘存在，CreateOnly 拒绝；换 Widget 名不绕过纹理冲突 |
| LFS_SOURCE_NOT_MATERIALIZED | 源文件仍是 Git LFS 指针，不是实际图片 |
| BLUEPRINT_COMPILE_FAILED | Widget 编译失败，不保存该 Widget 或本次新纹理 |
| PACKAGE_SAVE_FAILED | 某个包保存失败，已经保存的其他包保持原状 |
| RESOURCE_SAVE_FAILED | 新纹理未全部保存，停止保存 Widget，具体原因查看 Resources |
| REPORT_WRITE_FAILED | 报告写入失败的警告，不改变实际资产结果 |

P6 不增加覆盖/重导入、跨包原子性、批量调度、运行时业务逻辑或 P7 cook/页面视觉交付。现有 Editor 模块隔离、公共 UE API、任务级强引用、主线程入口和命令注销代码已检查；最终 UE 编译、GC 和模块生命周期行为以本地验收为准。

## 5 检查记录与本地验收

本节是本阶段验证状态的依据，用户只需更新这里。

| 检查                                 | 状态                            |
| ---------------------------------- | ----------------------------- |
| Lua P0–P4 回归                       | 云端通过；模拟 UE，不能代替真实反射           |
| P6 bootstrap/夹具执行顺序和异常传播           | 云端通过（5 项；模拟 UE，无 C++ 记录/资产验证） |
| 源码、模块依赖、值记录无 UObject 引用、CRLF 和差异检查 | 云端检查通过；不等于 UE 编译/GC 实测        |
| UE 5.3.2 Win64 完整编译/UHT            | ✅                             |
| AutoBlueprint.P6 五项自动化             | ✅                             |
| 当前版本 P0–P5 回归                      | ✅                             |
| 控制台：脚本异常后仍能定位已保存/未保存的资产和报告         | ✅                             |
| PIE 与 SIE 期间拒绝生成，停止后可正常执行          | ✅                             |
| 工具任务不影响项目已有游戏 Lua 启动及运行            | ✅                             |
| 编辑器退出/重开：无工具环境残留，命令和生成入口正常         | ✅                             |

离线检查命令（项目根目录，Lua 5.4）：

```text
lua Plugins/AutoBlueprint/Scripts/Tests/p0_scripts_test.lua
lua Plugins/AutoBlueprint/Scripts/Tests/p1_host_test.lua
lua Plugins/AutoBlueprint/Scripts/Tests/p2_sdk_test.lua
lua Plugins/AutoBlueprint/Scripts/Tests/p3_sdk_test.lua
lua Plugins/AutoBlueprint/Scripts/Tests/p4_sdk_test.lua
lua Plugins/AutoBlueprint/Scripts/Tests/p6_host_test.lua
```

本地人工验收：

1. 运行 P6 及历史阶段自动化。P6 测试内的预期 Lua 错误由返回值断言，不应导致测试 Fail。第二个纹理的目录被测试文件阻挡，必须返回 PACKAGE_SAVE_FAILED，不应进入引擎反复 MoveFile 的路径。
2. 复制 P5 示例到 `UIBlueprintLayouts/P6_Review.json`，改用全新目标名；先在普通编辑模式生成。检查控制台 Operation 行与目标资产、报告对应。
3. 若要人工查看“保存后抛错”，在插件工具目录新建本地审阅脚本，用 `auto_blueprint.generateTaskFromFile` 生成另一新目标，再 `error("P6 review after save")`；通过 AutoBlueprint.Run 执行。预期脚本失败日志，但 Operation 的 Widget saved=1，磁盘文件与逐次报告仍在。不要复用已生成目标名，否则只会测到同名冲突。
4. 启动 PIE，用新的未创建目标执行同一生成命令，应拒绝 EDITOR_BUSY，既无新 Widget 也无新纹理；停止 PIE 后同一输入应成功。另用新目标在 SIE 重复。记录实际模式和路径。
5. 工具任务前后分别运行项目原有游戏 Lua 启动流程，检查原有行为正常。P6 自动化检查已有 VM 身份和激活状态，但不能代替实际业务启动验证。
6. 关闭编辑器并重开；验证保留的已保存资产可加载，AutoBlueprint.Help/生成命令仍正常。模块不支持动态热重载，本阶段不要求强制卸载仍有反射对象的模块；验证正常退出及重新启动即可。
