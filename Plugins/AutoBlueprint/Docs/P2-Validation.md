# P2 布局协议与只读校验

实现完成，UE 5.3.2 Win64 编译、反射及 UE 自动化待用户本地验证。P1（含控制台引号修复）的通过记录保留。本阶段不创建、导入、编译或保存资产；通用生成属于 P3，资源实际导入属于 P4，GenerateBatch 仍返回 NOT_IMPLEMENTED。

## 1 调用入口

完整编译 ProjectOneEditor / Development Editor / Win64 后重启编辑器，无需 Play。在 Output Log 执行：

```text
AutoBlueprint.Run validate_from_file.lua Plugins/AutoBlueprint/Examples/minimal.layout.json
AutoBlueprint.Run validate_from_file.lua Plugins/AutoBlueprint/Examples/all_types.layout.json
AutoBlueprint.Run validate_from_file.lua Plugins/AutoBlueprint/Examples/invalid_slot.layout.json
AutoBlueprint.Run validate_from_file.lua Plugins/AutoBlueprint/Examples/duplicate_name.layout.json
Automation RunTests AutoBlueprint.P2
Automation RunTests AutoBlueprint.P1
Automation RunTests AutoBlueprint.P0
```

前两个样例应通过（目标包不存在时），后两个分别报告 INVALID_SLOT、DUPLICATE_WIDGET_NAME。验证脚本遇到无效布局会输出结构化诊断，再向宿主报告脚本失败。验证后不应出现新的 Widget、纹理、Package 或磁盘文件；已存在的目标按 CreateOnly 报冲突。

C++/UnLua 门面：

```cpp
FAutoBlueprintValidationResult ValidateLayout(const FString& LayoutJson);
FAutoBlueprintValidationResult ValidateLayoutFile(const FString& ProjectRelativePath);
```

共享 `auto_blueprint.lua` 提供 `validateLayout(json)`、`validateLayoutFile(path)`，直接返回结果；无效布局不被 SDK 隐藏。结果字段：`bValid`、`PlannedAssetPath`、`PlannedImports`、`Diagnostics`。后两种计划路径不表示已创建、预留或导入资产；无效结果中也可能有部分计划信息。诊断字段继续使用 `Code/JsonPath/WidgetName/AssetPath/Message/Severity`。生成前必须再次校验，不能依赖之前的预检。

`AutoBlueprint.GenerateFromFile` 和 SDK `generateFromFile` 仍明确失败，不会用 P0 固定树代替布局。P1 旧错误文本仍为 P2/P3 能力提示，兼容原回归；实际生成待 P3。

## 2 协议与默认值

唯一字段契约是 [layout.schema.json](../Resources/Schemas/layout.schema.json)，版本为 `schema_version: 1`。必须提供 `asset.package_path`、`design.width/height` 和唯一 `root`；每节点必须显式提供 `name/type/is_variable`。未知字段、类型、枚举、错误 JSON 类型和重复 JSON 键均拒绝。名称使用 ASCII 标识符，最多 64 字符，Widget 名按 UE FName 忽略大小写去重，`None` 为 UE 保留名称。

Schema 描述字段结构、范围和默认值；C++ 读取同一份文件并执行其所用关键词子集（type、enum、required、properties、additionalProperties、propertyNames、pattern、数值/长度/数量限制、oneOf、$ref、default）。这不是任意第三方 JSON Schema 执行器：新增关键词或正则形状时必须扩展解释器及测试。当前 pattern 仅为 ASCII 标识符。Schema 的默认值在 C++ 校验时深拷贝并填入内部类型化数据，避免节点之间共享可变默认对象。

- 坐标和间距采用 UMG 布局单位；设计尺寸范围 1–16384，不更改项目 DPI。
- `offsets` 为 Left/Top/Right/Bottom；锚点相同的轴后值是尺寸，锚点不同则是边距。固定锚点尺寸不允许负值。
- `anchors.min/max`、alignment、颜色和九宫格 margin 使用 0–1；锚点 min 不超过 max。颜色为线性 RGBA。padding 顺序为 Left/Top/Right/Bottom。
- Canvas 默认 anchors 全 0、alignment 为 [0,0]、offsets 全 0、auto_size=false、z_order=0。
- 其他 Slot 默认 padding 全 0、水平/垂直 Fill；线性容器 size_rule=Auto、fill=1。
- 通用属性默认 visibility=Visible、is_enabled=true、render_opacity=1。
- TextBlock 默认 text=""、font_size=24、color 白色、justification=Left、auto_wrap_text=false。font 可选，必须是已有项目 UFont 对象路径，省略时保留原生默认字体。字号采用 UMG 字号单位。
- Brush 的 resource 为必填资源别名；draw_as=Image、margin 全 0、tint 白色。Button 四态 brush 均可选；省略的 brush/style 保留原生默认。focused 明确拒绝。
- Button is_focusable=true；Border content_color 白色；SizeBox 未指定宽高时不启用 Override；ScaleBox stretch=ScaleToFit、stretch_direction=Both、user_specified_scale=1；Spacer size=[0,0]。

这些是 P2 接受并规范化的协议值；P3/P4/P5 按阶段实现写入控件的处理器。本阶段“校验通过”不意味着已能生成对应控件。

## 3 控件注册与 Slot

`WidgetTypeRegistry` 集中登记类型、原生 UClass、子节点策略及子 Slot 标识；Schema 集中声明属性规则。新增控件须同时扩展两者，离线检查验证类型集合一致。内部 `FLayoutSpec/FLayoutNode/FLayoutResource` 描述布局，属性和 Slot 使用有类型标签的 `FLayoutValue` 树，不向生成器传递原始 FJsonObject；后续属性写入处理器据注册类型解释这些值。DataDriven 策略保留分类，尚未注册 ListView/TileView/TreeView。

| 类型 | 子节点数 | 子 Slot | 专属属性 |
| --- | --- | --- | --- |
| CanvasPanel | 0–1024 | Canvas | 无 |
| Overlay | 0–1024 | Overlay | 无 |
| HorizontalBox / VerticalBox | 0–1024 | 同名 Slot | 无 |
| Border | 0–1 | Border | brush、content_color |
| SizeBox | 0–1 | SizeBox | width_override、height_override |
| ScaleBox | 0–1 | ScaleBox | stretch、stretch_direction、user_specified_scale |
| Button | 0–1 | Button | is_focusable、style.normal/hovered/pressed/disabled |
| Image | 0 | 无 | brush |
| TextBlock | 0 | 无 | text、font、font_size、color、justification、auto_wrap_text |
| Spacer | 0 | 无 | size |

根节点不能有 Slot；其他节点必须提供 Slot，类型由父容器决定。不存在单独的控件树生成路径。ScrollBox、ListView、业务事件、动画及任意 UObject 属性赋值不在本阶段支持范围。

## 4 路径、资源与只读保证

- 布局文件只接受项目相对路径，位于 `UIBlueprintLayouts/` 或 `Plugins/AutoBlueprint/Examples/`，扩展名 `.json`。拒绝绝对路径、反斜杠、冒号、空组件、`.` 和 `..`。目录按正常可信项目文件使用，不支持通过符号链接扩展允许根目录；路径约束为词法检查，不是不可信文件系统沙箱。
- 布局文件最多 1 MiB；直接 JSON 字符串最多 1,048,576 个 TCHAR；JSON 嵌套最多 128，Widget 层级最多 32、总节点最多 1024、资源最多 256、诊断最多 64。
- 目标 Widget 与新纹理包都必须在 `/Game/UI/`。检查已加载包（含未保存对象）、磁盘包及目标文件；目标之间按 FName 规则去重。
- 资源别名必须选择一种形式：已有 `object_path`，或 `source_file + package_path`。已有对象限定 `/Game/`，只读加载并验证 Texture2D；字体必须是 UFont。引用不存在的别名明确报错。
- 图片源限定 `Content/UI/` 的 PNG/JPG/JPEG。检查存在性、32 MiB 文件上限、LFS 指针和实际图片解码；解码最多 16 百万像素。计划导入只记录目标包，不创建纹理、不改已有资源设置。
- 预检可加载已有资源到内存，但没有 CreatePackage、工厂、导入、MarkPackageDirty、编译或保存调用。主线程/PIE/SIE/GC/保存状态使用现有上下文检查。

典型错误码：INVALID_JSON、DUPLICATE_JSON_FIELD、UNSUPPORTED_SCHEMA_VERSION、MISSING_FIELD、UNKNOWN_FIELD、INVALID_FIELD_TYPE、INVALID_VALUE、LAYOUT_LIMIT、INVALID_LAYOUT_PATH、LAYOUT_READ_FAILED、INVALID_PACKAGE_PATH、ASSET_ALREADY_EXISTS、DUPLICATE_PACKAGE_TARGET、INVALID_OBJECT_PATH、MISSING_RESOURCE、INVALID_RESOURCE_TYPE、INVALID_SOURCE_PATH、LFS_SOURCE_NOT_MATERIALIZED、INVALID_IMAGE、UNSUPPORTED_WIDGET、INVALID_CHILD_COUNT、DUPLICATE_WIDGET_NAME、INVALID_SLOT、UNSUPPORTED_FOCUS_BRUSH、SCHEMA_UNAVAILABLE。JSON 语法及重复键错误定位文档根，其他错误提供字段路径。

## 5 检查记录与本地验收

| 检查 | 状态 |
| --- | --- |
| Draft 2020-12 Schema 自检、25 项有效/无效结构用例、注册类型一致性 | 云端通过（jsonschema 4.26.0） |
| Lua 5.4.3 P0 15 项、P1 14 项兼容回归 | 云端通过（使用仓库自带 Lua 源码编译解释器） |
| P2 Lua 门面转发、无效结果传播、生成能力边界 | 云端通过；模拟 UE 门面 |
| UE 5.3.2 Win64 完整编译/UHT | 待用户本地验证 |
| AutoBlueprint.P2.Protocol / ReadOnlyConflicts / Examples / ResourcesAndDisk | 已编写，待 UE 执行 |
| AutoBlueprint.P1 四项及 P0 五项回归 | 本次待 UE 执行 |
| 真资源检查、未写入证明、Lua 真实反射 | 待用户本地验证 |

离线运行：`python Scripts/Tests/p2_schema_test.py`（从插件目录，需 jsonschema）；仓库根目录执行 `lua Plugins/AutoBlueprint/Scripts/Tests/p2_sdk_test.lua`。

本地补充：用已有 Texture2D/UFont 测试复用；错误类、缺资源、LFS 指针、损坏图片、资源目标与 Widget 重名应失败。对已有磁盘 Widget 执行预检应返回 ASSET_ALREADY_EXISTS，前后文件时间/内容及包 Dirty 状态保持一致。未保存包保护由 ReadOnlyConflicts 覆盖。检查普通编辑状态调用和 PIE/SIE 拒绝；连续有效→无效→有效不残留状态。完整运行上述四项 P2、四项 P1、五项 P0 测试后再记录 UE 通过，不以 Schema/Lua 离线通过替代 UE 编译或资产验证。
