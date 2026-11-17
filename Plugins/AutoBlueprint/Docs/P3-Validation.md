# P3 JSON 生成、编译与持久化

P3 实现完成，当前环境的 Schema、Lua 和源码检查已通过；**UE 5.3.2 编译、自动化及新会话验收尚未执行，等待用户本地验证**。开发基于远程 `7e6a71a`，保留 P2 已通过的本地记录。P3 首先生成 CanvasPanel、TextBlock、Button；P4 资源/样式、P5 其他容器与 GenerateBatch 没有提前实现。

## 1 编译与调用入口

关闭编辑器，完整编译 `ProjectOneEditor / Development Editor / Win64` 后重启；新增了反射结构和函数，不能只依赖 Live Coding。普通编辑状态下，无需 Play，在 Output Log 执行：

```text
AutoBlueprint.GenerateFromFile Plugins/AutoBlueprint/Examples/p3_minimal.layout.json
```

这条命令执行插件的 `generate_from_file.lua`：先只读校验文件，再调用 C++ 构建门面。C++ 会重新读取、校验布局及目标冲突；一次调用只生成一个 Widget Blueprint 和一个 Package。

默认目标为 `/Game/UI/AutoBlueprintTests/WBP_P3_Example`。成功后应看到 Created asset、Saved package、Build report 和 Generated 日志。再次执行同一命令应报告 `ASSET_ALREADY_EXISTS`，不改变原资产。重试新布局时复制 JSON 到 `UIBlueprintLayouts/` 并修改 `asset.package_path`，使用新名称；不要通过删旧资产来掩盖冲突测试。

等价工具命令：

```text
AutoBlueprint.Run generate_from_file.lua Plugins/AutoBlueprint/Examples/p3_minimal.layout.json
```

自动化测试可分组执行：

```text
Automation RunTests AutoBlueprint.P3
Automation RunTests AutoBlueprint.P2
Automation RunTests AutoBlueprint.P1
Automation RunTests AutoBlueprint.P0
```

P1 的 FileEntryBoundary 现在检查非法文件路径被正确拒绝，已不再断言旧的生成 TODO。P2 的协议校验范围保持不变。

C++/UnLua 新门面：

```cpp
FAutoBlueprintBuildResult BuildWidgetBlueprint(
    const FString& LayoutJson, const FAutoBlueprintBuildOptions& Options);
FAutoBlueprintBuildResult BuildFromLayoutFile(
    const FString& ProjectRelativePath, const FAutoBlueprintBuildOptions& Options);
```

Lua 工具中的用法：

```lua
local api = require("auto_blueprint")
local options = UE.FAutoBlueprintBuildOptions()
options.bSave = false
options.bWriteReport = true
local result = api.generateFromFile("UIBlueprintLayouts/main.layout.json", options)
```

`bSave`、`bWriteReport` 默认均为 true。`bSave=false` 仍创建并编译内存资产，占用该名称，但不保存 `.uasset`；只读检查应使用 ValidateLayout/ValidateLayoutFile。CreateOnly 是唯一写入策略，没有覆盖选项，也不自动打开资产编辑器。

## 2 当前生成能力与数据流程

| 配置内容 | P3 实际行为 |
| --- | --- |
| root、children、name、is_variable | 构造唯一根节点；保留子节点顺序；显式设置名称和变量开关 |
| CanvasPanel | 可作为根或合法子控件；子节点使用真正的 CanvasPanelSlot |
| Canvas Slot | 设置 anchors、offsets、alignment、auto_size、z_order；沿用 P2 的尺寸/边距语义 |
| Button | 最多一个子节点；设置 is_focusable；保留原生默认按钮样式 |
| Button Slot | 设置 padding、horizontal_alignment、vertical_alignment |
| TextBlock | 设置 text、font_size、线性 color、justification、auto_wrap_text；保留原生默认字体资源 |
| 三种控件的通用属性 | 设置 visibility、is_enabled、render_opacity |
| design.width/height | 编译后写入生成类默认对象的 DesignTimeSize，DesignSizeMode=Custom，供 Designer 预览；不改变项目 DPI 或运行时视口 |
| resources 非空、TextBlock.font、Button.style | P2 仍可校验；P3 构建在创建 Package 前明确拒绝，等待 P4 |
| 其余八种协议控件 | P2 仍可校验；P3 返回未实现构建处理器诊断，不生成替代控件 |

本次同时修正 P2 Schema 中误声明的 `properties.default` 输入字段；该未记录的伪属性现在按 UNKNOWN_FIELD 拒绝，正常 Schema 默认值不变，schema_version 仍为 1。离线及 UE 负例均已补充。

未知字段、不合法父子关系、重复名称、非法 Slot 等仍由 P2 拒绝。Schema 校验通过不等于所有协议能力都已具备生成实现。P3 的完整 Canvas 字段映射是当前基础链路的一部分；P5 继续完成其他容器和组合布局验证。

代码职责：

| 文件或目录（相对 Source/AutoBlueprintEditor） | 具体在做什么 |
| --- | --- |
| Public/AutoBlueprintTypes.h、AutoBlueprintLibrary.h、AutoBlueprintSubsystem.h | 定义构建选项、结果字段及 C++/Lua 可调用入口 |
| Private/AutoBlueprintLibrary.cpp、AutoBlueprintSubsystem.cpp | 检查编辑器状态，阻止重入，读取文件并把任务交给构建器 |
| Private/Assets/LayoutBlueprintBuilder.* | 重新执行 P2 校验和 P3 能力检查，核验 CreateOnly，创建唯一目标包/蓝图，组织构树、编译和保存 |
| Private/Widgets/WidgetTypeRegistry.* | 同一个注册表登记校验类型、原生类、是否能构建，以及属性和子 Slot 回调 |
| Private/Widgets/WidgetTreeBuilder.* | 递归创建控件模板，挂到 Blueprint 自己的 WidgetTree；父容器创建子 Slot |
| Private/Widgets/WidgetBuildHandlers.* | 将已校验的数据写到 Canvas/Button Slot、文本和通用控件属性 |
| Private/Assets/WidgetBlueprintWriter.* | 复用 P0 编译与保存流程，收集编译诊断，设置设计尺寸，编译失败时不保存 |
| Private/Diagnostics/BuildReport.* | 把本次构建结果写成 Saved 下的 JSON，不把报告文件当作资产包 |
| Private/Layout/SmokeTestLayout.* | 只提供 P0 固定样例的 JSON 数据，替代旧 SmokeTestWidgetTree；P0 也使用统一构建器 |

P0 的函数签名、测试路径限制及独立结果契约保留，固定样例不默认输出磁盘报告。它现在也依赖插件随附的 Schema；不再维护另一套固定树构造代码。

## 3 结果、报告与失败行为

| 字段 | 含义 |
| --- | --- |
| bSuccess | 构树、编译和本次请求的保存全部成功；未请求保存时编译成功即可为 true |
| bAssetCreated | 本次已实际创建目标 Widget Blueprint |
| bCompiled | 本次蓝图编译成功，生成类有效 |
| bSaved | 本次已保存目标 Widget 包；未请求、跳过或失败时均为 false |
| TargetAssetPath | 经路径检查得到的目标对象路径，不证明已经创建 |
| CreatedPackageName | 本次实际创建的唯一包名；后续失败仍保留；同名冲突时为空 |
| ReportFilePath | 成功写出的 JSON 报告绝对路径；关闭报告或写入失败时为空 |
| Diagnostics | 稳定错误码、严重程度、JSON 路径、控件/资产名称和说明 |

报告默认写入 `Saved/AutoBlueprint/Reports/<GUID>.json`，采用 `report_version: 1`、UTC 时间及 `lower_snake_case` 字段，记录 save_requested、success、asset_created、compiled、saved、目标路径、实际包名和诊断。每次构建独立一份，不汇总多个 UI，也不承诺事务回滚。

进入 C++ 构建后的普通校验、能力、编译和保存失败都会生成报告（启用报告且目录可写时）。编辑器上下文/忙碌拒绝不写报告；Lua 在只读预检阶段就拒绝的请求尚未进入构建，因此仅返回诊断。报告写入失败增加 `REPORT_WRITE_FAILED` 警告，不改变已发生的创建/保存状态，也不把已保存资产说成未保存。

构树或编译失败可能留下未保存的新包；不会自动删除它，更不会删除用户已有资产。Lua SDK 会打印创建/未保存包及报告位置，然后传播失败到脚本宿主。这里的“宿主”就是在 UE 编辑器中创建独立 Lua 环境、运行脚本并接收异常的 C++ 代码。

| 典型错误码 | 简要说明 |
| --- | --- |
| ASSET_ALREADY_EXISTS | 目标已在内存或磁盘存在；包括校验加载依赖或编译回调期间出现的新冲突 |
| BUILD_WIDGET_NOT_IMPLEMENTED | 协议支持该控件，但当前阶段尚无构建处理器 |
| BUILD_PROPERTY_NOT_IMPLEMENTED | 请求了尚未实现的字体资产或按钮样式写入 |
| BUILD_RESOURCES_NOT_IMPLEMENTED | 配置包含资源清单；复用/导入需等待 P4 |
| ASSET_CREATE_FAILED | Package 或 Widget Blueprint 创建失败，结果保留已创建包的信息 |
| WIDGET_TREE_BUILD_FAILED | 无法构造控件模板，或父容器未产生预期 Slot |
| BLUEPRINT_COMPILE_ERROR / BLUEPRINT_COMPILE_FAILED | 编译器错误或编译整体失败；目标 Widget 不保存 |
| PACKAGE_SAVE_FAILED | 无法创建输出目录或 UE 保存失败；可能已经创建并编译，但 bSaved=false |
| REPORT_WRITE_FAILED | 构建报告无法写入；资产结果保持真实 |

P2 原有错误码继续有效，见 [P2-Validation.md 第 4 节](P2-Validation.md#4-路径资源与只读保证)。

## 4 测试具体在验证什么

以下五项 UE 测试在 `Private/Tests/AutoBlueprintP3Tests.cpp` 中；每次使用 GUID 生成独立包名，不覆盖用户资产。

| UE 测试名（AutoBlueprint.P3. 后缀） | 执行步骤及检查目标 | 会留下什么 |
| --- | --- | --- |
| TreeAndVariables | 从 JSON 创建不保存的蓝图；检查四个控件、父子顺序、文字、字号颜色、变量成员、按钮焦点、两种 Slot 和设计尺寸；再次构建验证未保存同名包也会冲突；另生成 TextBlock 单根布局，检查转义/中文文本、默认值和通用属性，证明没有套用固定树 | 不写 `.uasset`；结束时清理测试创建对象的 Standalone/Dirty 标记 |
| PreflightNoWrites | 分别请求尚未构建的 Overlay、按钮 style、重复名称、误写的 properties.default 及坏 JSON；检查明确诊断，内存和磁盘都没有新目标包；前两种配置仍可通过 P2 协议校验 | 无目标资产；关闭报告输出 |
| SaveReloadAndConflict | 创建并保存蓝图，读取 JSON 报告；卸载整个测试包，再从磁盘加载；重新检查树、属性、变量及设计尺寸，实例化 UserWidget；再次生成检查原对象、Dirty 状态和磁盘内容未变 | 保留一个 `/Game/UI/AutoBlueprintTests/WBP_P3_<GUID>` 资产和报告，便于人工打开；日志输出路径 |
| FileLuaAndSaveFailure | 写临时 JSON，通过真实独立 UnLua 环境运行 p3_build_no_save.lua，核验反射选项默认值和结果；另用测试专属普通文件挡住输出目录，验证创建/编译成功但保存失败的部分状态 | 删除临时 JSON 和挡路文件；新包保持未保存并清理测试标记；保留失败报告 |
| CompileFailureDoesNotSave | 用名为 Slot 的变量 TextBlock 与原生 UWidget::Slot 成员形成类型冲突，触发真实蓝图编译器错误；检查 bCompiled/bSaved/bSuccess 为 false，诊断可见且没有目标文件 | 未保存测试包清理标记；保留编译失败报告 |

`SaveReloadAndConflict` 是同进程卸载重载；不能替代第 5 节的新编辑器会话验收。测试只实例化控件，不宣称已经通过 AddToViewport、视觉一致或 cook/打包验证。编译负例预期返回编译错误，最终该项自动化本身应为 Success。

| 其他测试文件 | 具体用途与限制 |
| --- | --- |
| Scripts/Tests/p3_sdk_test.lua | 独立 Lua 模拟 UE 返回值，检查先校验再构建、选项传递、成功/未保存状态及各失败阶段是否向上抛错；不能证明 C++ 反射或资产行为 |
| Scripts/Lua/tests/p3_build_no_save.lua | 由上述 UE 自动化实际运行，使用真实 FAutoBlueprintBuildOptions 和 C++ 文件门面；验证无需 Play 即可创建、编译但不保存 |
| Scripts/Tests/p2_schema_test.py | 检查 Schema、P3 新样例的字段结构及注册类型一致性；不创建 UE 资产 |
| P0/P1/P2 原有测试 | 检查统一生成器接入后，固定样例、Lua 环境和只读校验是否回归 |

## 5 检查记录与本地验收

| 检查 | 当前状态 |
| --- | --- |
| Draft 2020-12 Schema、27 项样例/结构用例、注册表一致性 | 云端通过（jsonschema；包含新增 P3 样例） |
| Lua 5.4.3：P0 15 项、P1 14 项、P2 校验转发 | 云端通过；模拟 UE 门面 |
| P3 Lua 预检/构建顺序、选项及四类失败传播 | 云端通过；模拟 UE 门面 |
| C++ 风格、CRLF、差异及阶段边界检查 | 云端通过；不等于 UE 编译 |
| UE 5.3.2 Win64 完整编译/UHT | 待本地验证 |
| AutoBlueprint.P3 五项自动化 | 待本地验证 |
| 当前版本 P2 四项、P1 四项、P0 五项回归 | 待本地验证；之前版本的通过记录保留 |
| Output Log 生成、Designer 对照、同名保护 | 待本地验证 |
| 关闭并重启编辑器后重开、变量及运行时显示 | 待本地验证 |

离线命令（仓库根目录，Lua 使用 5.4；Python 需 jsonschema）：

```text
python Plugins/AutoBlueprint/Scripts/Tests/p2_schema_test.py
lua Plugins/AutoBlueprint/Scripts/Tests/p0_scripts_test.lua
lua Plugins/AutoBlueprint/Scripts/Tests/p1_host_test.lua
lua Plugins/AutoBlueprint/Scripts/Tests/p2_sdk_test.lua
lua Plugins/AutoBlueprint/Scripts/Tests/p3_sdk_test.lua
```

本地人工验收：

1. 执行第 1 节生成命令，打开 `WBP_P3_Example`。根 Canvas 下依次是 TXT_Title 和 BTN_Confirm，按钮下面是 TXT_Confirm。
2. Designer 设计尺寸为 960×640；标题是 `P3 JSON layout`，字号 32、浅蓝色。按钮使用底部锚点，横向拉伸；标签居中。核对 JSON 中的 offsets、padding 和 z_order。
3. TXT_Title、BTN_Confirm 的 Is Variable 开启，RootCanvas、TXT_Confirm 关闭；按钮 Is Focusable 关闭。检查报告与资产的实际创建、编译、保存状态一致。
4. 不删除目标，重复生成，应报冲突；原有树、属性和保存文件保持不变。修改 JSON 的目标包名可生成第二个独立 UI，但这不是 GenerateBatch。
5. 关闭编辑器，重新启动并打开资产，检查层级、文字、变量、设计尺寸和 Slot 仍存在；放入测试界面或通过 CreateWidget/AddToViewport 显示，确认可正常实例化。此时才记录新会话持久化通过。
6. 在 PIE/SIE 中尝试生成另一个新名称，应该拒绝且不产生目标；停止 Play 后同一布局应可执行。

已知边界：纯文本采用 culture-invariant 文本，不创建本地化资源；未指定字体时使用原生默认字体，不保证中文覆盖。报告不是批量任务记录。生成类仅继承原生 UUserWidget，控件均为原生 UMG；完整 cook/打包与视觉验收仍按后续阶段执行。

## 6 2026-11-17 首次编译反馈修复

用户本地 UE 5.3.2 编译在 P3 测试代码中发现两处兼容问题：

| 报错位置 | 原因与修复 |
| --- | --- |
| TreeAndVariables：GetJustification / TestTrue | UE 5.3.2 的 UTextBlock 没有 GetJustification；原表达式无效，继而产生 TestTrue 参数错误。改为通过 FByteProperty 读取已序列化的 Justification 枚举，先断言字段存在，再检查是否为 Right；保留对齐结果验证。 |
| SaveReloadAndConflict：CreateWidget | 原调用把 GeneratedClass 的 TObjectPtr<UClass> 直接传给要求 TSubclassOf<UUserWidget> 的参数，无法匹配重载。改为先 Get() 取出类指针，显式构造 TSubclassOf<UUserWidget>，再调用 CreateWidget；并检查编辑器 World 有效。 |

本次只修改自动化测试及本节记录，生成器实现不变。云端完成源码、CRLF 和差异检查；没有 UE 环境，修复后的实际编译及自动化仍待用户本地验证，第 5 节状态不改为通过。
