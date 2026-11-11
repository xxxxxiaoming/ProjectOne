# P1 编辑器 Lua 工具入口与本地验证

实现状态：P1 代码已完成，已由用户本地 review 和 validate 通过。基线 `fff6a1b` 的 P0/refactor 验证记录保留。UE 编译、UE 自动化、独立 Lua 检查及手动验证已通过；具体验证范围、已接受的控制台参数限制和证据以第 7 节为准。

## 1 编译与入口

关闭 UE Editor，fetch `feature_auto_blueprint_plugin`，重新生成工程并完整编译 UE 5.3.2 的 `ProjectOneEditor / Development Editor / Win64`。本次新增反射类型和函数，需要完整编译与编辑器重启。无需修改 Engine、UnLua 或安装 Python 插件。

项目已仅为 Editor 目标启用 AutoBlueprint；插件为 Editor 模块，依赖项目 UnLua。P1 提供明确的控制台命令入口，暂不增加菜单或面板。普通编辑状态下，在 **Output Log** 输入：

```text
AutoBlueprint.Help
AutoBlueprint.Run examples/hello.lua
AutoBlueprint.Run examples/hello.lua hello_world 中文参数
```

预期打印就绪信息或原样参数，以及 `AutoBlueprint Lua task completed`。这些命令不创建资产，无需点击 Play。

控制台参数约定：每个参数不含空白，多词内容统一用 `_` 连接（例如 `hello_world`），不使用引号包裹参数。下划线按原样传入，不会自动还原为空格。当前控制台入口会拆分带空格的引号参数并保留引号；用户已接受该限制，不修改解析代码。此约定仅针对控制台文本入口，下面的 C++ `Arguments` 数组仍直接传递字符串。

P0 命令继续保留：

```text
AutoBlueprint.P0.Generate /Game/UI/AutoBlueprintTests/WBP_P1_Review01
AutoBlueprint.P0.ErrorProbe
```

第一个仍创建并保存固定树蓝图，使用新的资产名。第二个故意返回 `LUA_SCRIPT_FAILED` 和 `p0_error.lua` 的 traceback。生成后关闭整个编辑器，在新会话中重开资产，检查持久化；重复生成应保持 CreateOnly 冲突。

## 2 脚本和反射契约

正式 C++/UnLua 入口：

```cpp
FAutoBlueprintLuaRunRequest Request;
Request.ScriptPath = TEXT("examples/hello.lua");
Request.Arguments.Add(TEXT("hello world"));
FAutoBlueprintLuaRunResult Result = UAutoBlueprintLibrary::RunEditorScript(Request);
```

静态门面转调子系统。脚本执行结果与资产构建结果独立：

| 字段 | 含义 |
| --- | --- |
| 请求的 `ScriptPath` | 插件 `Scripts/Lua/` 下的相对文件 |
| `Arguments` | 有序字符串数组，以数据传入 Lua，不拼接可执行源码 |
| 结果的 `bSuccess` | 入口执行结束，且 `run` 返回 true |
| `Code`、`Message` | 宿主错误码、原始错误、脚本位置及调用栈 |
| 结果的 `ScriptPath` | 已解析的绝对入口路径；路径未解析完成时为空 |

工具脚本返回一个带 `run(arguments)` 的表：

```lua
local greeter = require("examples.greeter")
local module = {}
function module.run(arguments)
    print(greeter.message(arguments))
    return true
end
return module
```

P0 脚本入口由 `run(packagePath)` 迁移为 `run(arguments)`，从 `arguments[1]` 读取包路径，控制台命令用法不变。内部旧 C++ Runner 重载作为测试适配，仍使用同一个 P1 宿主。

共享 [auto_blueprint.lua](../Scripts/Lua/auto_blueprint.lua) 提供：

- `buildSmokeTest(request)`：调用真实 P0 构建 API，检查并报告单 UI 结果。
- `reportBuildResult(result, saveRequested)`：处理创建、编译、保存和诊断，允许明确不保存的成功请求。
- `generateFromFile(layoutPath)`：P1 能力占位，明确抛出 `NOT_IMPLEMENTED`，由 P2/P3 接入实际生成。

```text
AutoBlueprint.GenerateFromFile UIBlueprintLayouts/Test.layout.json
```

P1 预期返回 `LUA_SCRIPT_FAILED`，消息包含 `NOT_IMPLEMENTED` 和调用栈。此阶段不读取布局、不生成替代 UI；JSON Schema、文件校验、通用控件构建和真正的批量执行仍未实现。

## 3 加载边界和生命周期

- 入口只允许工具目录下的相对 `.lua` 路径，使用正斜杠；文件名和目录组件为 ASCII 标识符（字母/下划线开头，后接字母、数字、下划线）。绝对路径、点路径、空组件、反斜杠和目录穿越均拒绝。
- `require("examples.greeter")` 映射到 `Scripts/Lua/examples/greeter.lua`。模块名为点分标识符，不带 `.lua` 后缀；不回退到游戏目录或原生库。
- 每次任务创建独立 VM 与工具模块缓存，不复用初始化时的 `package.loaded`。循环依赖报告 `LUA_MODULE_CYCLE`，模块失败保留原始位置和调用栈。nil 导出缓存为 true；false 导出可重新加载。
- 每源文件不超过 1 MiB；入口路径不超过 240 字符，模块名不超过 128 字符；至多 32 个参数、每个不超过 4096 字符；每任务至多 64 次模块执行尝试，缓存命中不计。
- 插件执行可信的项目工具脚本。路径检查用于模块发现，不提供不可信代码沙箱；工具目录使用正常项目文件，不借外部链接扩展根目录。脚本仍能访问 UE/Lua 能力。
- 公共任务在编辑器 Game Thread 同步执行。PIE/SIE、GC、保存中、嵌套脚本及构建期间的脚本请求拒绝；任务内部可调用单 UI 构建 API。
- 成功或失败返回前释放 VM，不保留跨任务 Lua 引用。模块关闭注销全部命令，子系统停止接受任务；模块不支持动态重载，更改后完整编译、重启编辑器。
- 不调用游戏环境的 `Start`、`SetActive` 或默认启动模块。UnLua 自身初始化及已注册回调仍按原有行为执行，隔离结果以本地测试为准。
- 脚本失败不回滚此前已保存的资产；各次构建结果及保存日志仍有效。

## 4 UE 测试与手动验证

在普通编辑状态执行：

```text
Automation RunTests AutoBlueprint.P1
Automation RunTests AutoBlueprint.P0
```

也可使用 **Tools → Session Frontend → Automation**。新增源码：[AutoBlueprintP1Tests.cpp](../Source/AutoBlueprintEditor/Private/Tests/AutoBlueprintP1Tests.cpp)。

| P1 测试 | 验证内容 |
| --- | --- |
| `InputValidation` | 路径边界、缺失脚本、参数数量/长度、模块标识符和 VM 无残留 |
| `HostLifecycle` | 真实加载/反射、参数原样传递、模块缓存、跨任务隔离、异常/循环恢复、重入拒绝、VM 释放与原有游戏 Lua 状态 |
| `FileEntryBoundary` | 文件入口明确报告未实现；参数不足及非工具模块被拒绝 |

P1 三项测试不创建或保存资产。P0 五项测试一起回归，其中 `SingleBuildResult` 创建并编译内存资产，不保存 `.uasset`。

手动检查：连续执行 hello → 错误探针 → hello；在 Play/Simulate 中运行工具应返回 `EDITOR_BUSY`，停止后可用；生成新名字的 P0 资产并完成保存/新会话重开；检查原有游戏 Lua 行为；正常关闭重开后命令可用且无 Lua/委托残留错误。

## 5 独立 Lua 检查

安装 Lua 5.4（项目基线 5.4.3），在仓库根目录执行：

```text
lua Plugins/AutoBlueprint/Scripts/Tests/p0_scripts_test.lua
lua Plugins/AutoBlueprint/Scripts/Tests/p1_host_test.lua
```

P0 保留 15 项结果检查并接入共享 SDK；P1 新增 14 项宿主/SDK 检查，使用真实 bootstrap/脚本和模拟文件读取、UE 门面。覆盖参数、缓存、模块异常/循环、语法错误、入口契约、未实现能力、失败重试、游戏模块隔离、模块导出和数量限制。

这些检查不验证 C++ 路径处理、真实 UnLua 生命周期或资产持久化。P0 15 项和 P1 14 项独立 Lua 检查已由用户本地执行通过，结果见第 7 节。

## 6 代码入口

| 文件 | 职责 |
| --- | --- |
| [AutoBlueprintEditorModule.cpp](../Source/AutoBlueprintEditor/Private/AutoBlueprintEditorModule.cpp) | 五个命令注册/注销，调用门面 |
| [AutoBlueprintLuaTypes.h](../Source/AutoBlueprintEditor/Public/AutoBlueprintLuaTypes.h) | 反射请求与运行结果 |
| [AutoBlueprintSubsystem.cpp](../Source/AutoBlueprintEditor/Private/AutoBlueprintSubsystem.cpp) | 生命周期和重入保护 |
| [EditorLuaRunner.cpp](../Source/AutoBlueprintEditor/Private/Scripting/EditorLuaRunner.cpp) | 独立 VM、参数桥接、异常和模块缓存 |
| [LuaScriptSource.cpp](../Source/AutoBlueprintEditor/Private/Scripting/LuaScriptSource.cpp) | 路径规则及有界源码读取 |
| [generate_from_file.lua](../Scripts/Lua/generate_from_file.lua) | 布局入口和 P2/P3 能力边界 |

## 7 本次验证记录

| 检查                          | 状态                                                 |
| --------------------------- | -------------------------------------------------- |
| 描述文件、源码引用、换行和文档链接静态检查       | 已通过；不等价于 UHT/UE 编译                                 |
| Lua 文件与 bootstrap 静态语法检查    | 已通过（luaparse 0.3.1，Lua 5.3 兼容语法）；不等价于 Lua 5.4 运行测试 |
| UE 5.3.2 Win64 Editor 完整编译  | ✅                                                  |
| 三项 `AutoBlueprint.P1` 测试    | ✅                                                  |
| 五项 `AutoBlueprint.P0` 兼容回归  | ✅                                                  |
| 独立 Lua：P0 15 项、P1 14 项      | ✅                                                  |
| 工具命令、PIE/SIE 拒绝、生成/保存/新会话重开 | ✅                                                  |
| 原有游戏 Lua 行为和关闭/重开           | ✅                                                  |

2026-11-11 本地手动验证补充：

- Help、hello → 错误探针 → hello、文件入口的 `NOT_IMPLEMENTED` 诊断符合预期；PIE 和 SIE 中均返回 `EDITOR_BUSY`，停止后工具恢复可用。
- 新建并保存 `WBP_P1_CUA_20261111_1325`；完整关闭编辑器并重启后，资产可重开，`RootCanvas → TXT_Title / BTN_Confirm → TXT_Confirm` 控件树及文本完整。同名生成返回 `ASSET_ALREADY_EXISTS`。
- 将原有 `BP_Test` 放入另存的验证关卡 `L_P1_CUA_20261111_1325`；重启前后均输出 `BP_Test BeginPlay from lua`。正常退出和新会话日志未发现 Lua/委托残留错误；游戏 Lua 行为的验收范围为该 BeginPlay 示例。
- 带引号参数检查发现：`"hello world" "中文参数"` 实际输出为 `"hello | world" | "中文参数"`，未实现带空格参数的原样传递。用户决定不修复，以本页的 `_` 连接约定作为使用限制；上述通过状态不表示引号解析已修复，也不表示 `_` 示例已在本轮重新执行。
- 证据：`Saved/Logs/ProjectOne-backup-2026.11.11-05.27.09.log`（重启前）及 `Saved/Logs/ProjectOne.log`（重启后；后续编辑器会话可能轮换此文件）。验证 Widget、关卡及关联文件保留在本地。

按 [constitution 第 10 节](constitution.md#10-可维护性和变更交付)，完成后推送功能分支，由用户 review/validate，默认不创建 PR。
