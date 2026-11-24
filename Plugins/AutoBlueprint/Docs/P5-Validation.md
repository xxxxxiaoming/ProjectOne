# P5 基础容器与 Slot 验证

P5 实现完成，**待 UE 5.3.2 本地编译和验收**。基于远程 `73d13c0`，保留 P4 已通过的验收记录及纹理异步编译修复。以后本阶段的验证状态以第 5 节“检查记录与本地验收”为准，其他描述由助手同步。

## 1 编译与调用入口

关闭编辑器，完整编译 `ProjectOneEditor / Development Editor / Win64` 后重启。普通编辑状态下执行，无需 Play：

```text
AutoBlueprint.GenerateFromFile Plugins/AutoBlueprint/Examples/p5_nested.layout.json
Automation RunTests AutoBlueprint.P5
```

示例目标为 `/Game/UI/AutoBlueprintTests/WBP_P5_Nested`，含 17 个节点、全部 11 类控件及全部 8 类父 Slot。它不依赖外部图片，Image 使用空画刷，Border 使用原生背景；用于检查容器结构和布局，不以空 Image 为导入失败。自动化的 NestedSaveReload 另行生成 PNG，并让 Border/Image 共享真实纹理；测试日志会给出可打开检查的已保存 Widget 路径。

重复生成同名目标应被 CreateOnly 拒绝。需要新的人工样例时，将 JSON 复制到 `UIBlueprintLayouts/` 并修改 `asset.package_path`，再使用同一命令生成。P4 的任务接口与单 Widget 接口保持原有契约；P5 没有新增反射结果类型，也没有实现 GenerateBatch。

本阶段回归命令：

```text
Automation RunTests AutoBlueprint.P4
Automation RunTests AutoBlueprint.P3
Automation RunTests AutoBlueprint.P2
Automation RunTests AutoBlueprint.P1
Automation RunTests AutoBlueprint.P0
```

## 2 控件与 Slot 能力表

所有控件共用显式 name、is_variable、visibility、is_enabled、render_opacity。可见性默认 Visible，启用默认 true，透明度默认 1；根节点不能有 Slot，非根节点必须填写与父容器匹配的 Slot。

| 控件 | 直接子节点 | 控件专属属性 | 子节点的 Slot |
| --- | --- | --- | --- |
| CanvasPanel | 多个 | 无；布局写在各子节点 slot 中 | Canvas |
| Overlay | 多个 | 无；保持 JSON 子节点顺序，后面的子节点叠在前面之上 | Overlay |
| HorizontalBox | 多个 | 无；按子节点顺序水平排列 | HorizontalBox |
| VerticalBox | 多个 | 无；按子节点顺序垂直排列 | VerticalBox |
| Border | 最多一个 | brush（资源/Image/Box/九宫格/tint）、content_color（默认线性白色） | Border |
| SizeBox | 最多一个 | width_override、height_override；省略则不启用覆盖，显式 0 则启用零尺寸覆盖 | SizeBox |
| ScaleBox | 最多一个 | stretch、stretch_direction、user_specified_scale | ScaleBox |
| Button | 最多一个 | P4 四态 style、is_focusable；focused 画刷不支持 | Button |
| Image | 不允许 | P4 brush | 无 |
| TextBlock | 不允许 | P3/P4 文字、字号、已有 UFont、颜色、对齐、换行 | 无 |
| Spacer | 不允许 | size，默认 `[0,0]`，表示期望大小 | 无 |

| Slot 类型 | 字段与默认值 |
| --- | --- |
| Canvas | anchors.min/max 默认 `[0,0]`；offsets 默认 `[0,0,0,0]`；alignment 默认 `[0,0]`；auto_size=false；z_order=0 |
| Overlay | padding=`[0,0,0,0]`，horizontal_alignment=Fill，vertical_alignment=Fill |
| HorizontalBox | padding 和对齐同上；size_rule=Auto，fill=1 |
| VerticalBox | padding 和对齐同上；size_rule=Auto，fill=1 |
| Border | padding 和对齐同 Overlay |
| SizeBox | padding 和对齐同 Overlay |
| ScaleBox | 仅对齐生效；原生 UScaleBoxSlot 不支持 Padding，非零 padding 在预检时拒绝 |
| Button | padding 和对齐同 Overlay，复用原有语义 |

水平对齐枚举为 Fill/Left/Center/Right，垂直为 Fill/Top/Center/Bottom。Padding 按 `[Left,Top,Right,Bottom]` 解释。Box Slot 的 Auto 使用内容期望尺寸；Fill 按 fill 权重分配父容器主轴上的剩余空间，不是像素数。Auto 时 fill 仍记录在 FSlateChildSize 中，但不参与空间分配。

Canvas 固定锚点下 offsets 的后两个值表示宽高；拉伸锚点下对应边距。auto_size 使用内容期望尺寸；z_order 控制 Canvas 层级。P3 已实现这些字段，P5 复用并增加与嵌套容器组合的持久化检查。

ScaleBox 支持 None、Fill、ScaleToFit、ScaleToFitX、ScaleToFitY、ScaleToFill、UserSpecified；默认 ScaleToFit。stretch_direction 为 Both/DownOnly/UpOnly，默认 Both。user_specified_scale 默认 1，仅在 UserSpecified 模式控制缩放，其他模式仍保留配置值。

**ScaleBox Padding 限制：**原生 [UScaleBoxSlot::SetPadding](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/UMG/Components/UScaleBoxSlot/SetPadding?application_version=5.5) 是已弃用的无效接口。Schema v1 保留 padding 字段及零默认值，C++ 语义预检新增非零拒绝，返回 `UNSUPPORTED_SLOT_PROPERTY` 并定位到 `.slot.padding`；不静默忽略，不插入隐藏控件。需要内边距时明确增加 Border/SizeBox 包装层。P4 尚不生成 ScaleBox，因此没有此前可用的生成行为需要迁移。

## 3 实现与兼容性

- `WidgetTypeRegistry.cpp` 为全部 11 类控件登记生成能力及属性/Slot 回调。
- `WidgetBuildHandlers.h/.cpp` 新增 Border、SizeBox、ScaleBox、Spacer 属性处理，以及 Overlay/HorizontalBox/VerticalBox/Border/SizeBox/ScaleBox 的 Slot 处理。可复用的 Padding/Alignment 映射集中实现；Box Slot 单独追加 FSlateChildSize。Border 复用 P4 画刷和资源缓存。
- `LayoutValidation.cpp` 增加 ScaleBox 非零 Padding 的明确诊断；名称、父子数量、Slot 匹配、资源和目标预检继续走原有流程。
- `WidgetTreeBuilder.cpp` 仍按注册表遍历原生模板。显式保留 DataDriven 拒绝分支：未来 ListView 需独立的数据项/Entry 策略，不能注册成可构建后直接走普通 AddChild。当前 ListView、ScrollBox 均未注册，直接拒绝。
- P3 的两项旧“Overlay/SizeBox 尚未实现”负例改为错误父 Slot 和根节点带 Slot；P4 的未实现控件负例改为 ListView。仍检查在创建资源和 Widget 前拒绝，未降低资产保护断言。

单 UI/资源任务结果、逐包保存次序、CreateOnly、原始图片导入、Lua 宿主保持原有契约。P4 的纹理 PreEditChange/PostEditChange/FinishCompilation 修复保留。P5 不实现 P6/P7、批量生成、运行时事件或自定义控件。

## 4 测试具体做什么

| 测试/文件 | 具体操作与检查 |
| --- | --- |
| AutoBlueprint.P5.NestedSaveReload | 准备一张真实 PNG；生成含全部 11 类控件的 17 节点嵌套树，Border/Image 共用一个新纹理。检查每个节点的原生类、父子关系、兄弟顺序、8 类 Slot、Padding/Alignment、Auto/Fill 权重、Canvas 布局、SizeBox 覆盖标志、ScaleBox 和 Spacer 属性、Border 颜色与画刷。保存后卸载 Widget/纹理包，再从磁盘重载并重复检查，最后 CreateWidget 并检查实例树；还检查变量生成和同名拒绝 |
| AutoBlueprint.P5.ScaleModesAndDefaults | 不导入图片，分别构建 7 种 Stretch，并轮换覆盖 3 种方向；另一次省略 ScaleBox 专属字段检查默认值；检查 bSave=false 没有写出 Widget 文件 |
| AutoBlueprint.P5.InvalidLayoutsNoWrites | 10 个负例：父 Slot 错配、叶子有子节点、单子容器有多个子节点、ScaleBox 非零 Padding、Fill 权重为 0、ListView、根节点带 Slot、负尺寸覆盖、缺 Slot、重名。输入附带一张有效待导入图片；逐项检查诊断和 JSON 位置，以及内存/磁盘都没有生成 Widget 或纹理 |
| AutoBlueprint.P5.FileLuaNoSave | 写入临时布局，通过真实独立 UnLua 环境执行 `tests/p5_generate_no_save.lua`，生成同一嵌套树与纹理但不保存；检查反射结果、实际控件属性和磁盘无输出 |
| Scripts/Tests/p5_contract_test.py | 离线检查嵌套样例的结构、11 类注册、父 Slot 声明、缩放枚举与数值边界。只验证 Schema 结构，不能代替 UE 属性、Slot 或保存重载测试 |
| Scripts/Lua/tests/p5_generate_no_save.lua | UE 自动化调用的工具脚本，使用原有 generateTaskFromFile，检查 Widget/资源均成功且未保存；不需要 Play |

自动化只清理本次 GUID 专属的原始 PNG、布局文件及未保存对象的保留/脏标记。已保存的 Widget、纹理和报告保留供人工查看，不删除用户资产。自动化原图在测试结束后清理，保存纹理仍可显示；不要用这些已清理原图的资产测试重新导入。

| 错误码 | 简要说明 |
| --- | --- |
| INVALID_SLOT | 根节点有 Slot、非根缺 Slot、Slot 类型与父容器不匹配，或 Canvas 参数关系非法 |
| INVALID_CHILD_COUNT | 叶子节点带子节点，或单子容器超过一个直接子节点 |
| UNSUPPORTED_SLOT_PROPERTY | 请求原生 ScaleBox 不支持的非零 Padding |
| INVALID_VALUE | 尺寸、权重、颜色等超范围，或枚举非法 |
| UNSUPPORTED_WIDGET | 未注册的控件，例如 ListView/ScrollBox |
| BUILD_CHILD_POLICY_NOT_IMPLEMENTED | 内部注册描述使用 DataDriven，但尚未提供专用构建策略；当前公共 Schema 不接受此类控件 |
| DUPLICATE_WIDGET_NAME | 控件名称重复，比较不区分大小写 |
| ASSET_ALREADY_EXISTS | 目标 Widget/新纹理包在内存或磁盘已存在，CreateOnly 拒绝 |

## 5 检查记录与本地验收

本节是本阶段验证状态的依据；用户只需更新这里，其他状态描述由助手同步。

| 检查 | 状态 |
| --- | --- |
| Schema 原有样例/边界与注册表一致性 | 云端通过（29 项，包含 P5 样例；无 UE） |
| P5 嵌套结构、缩放与属性边界 | 云端通过（32 项；无 UE） |
| Lua 5.4 P0–P4 回归及 P5 工具脚本 | 云端通过；模拟 UE，不代表真实反射通过 |
| C++ 源码、CRLF、差异和能力范围 | 云端检查；不等于 UE 编译 |
| UE 5.3.2 Win64 完整编译/UHT | 待本地验证 |
| AutoBlueprint.P5 四项自动化 | 待本地验证 |
| 当前版本 P4/P3/P2/P1/P0 回归 | 待本地验证 |
| Designer：嵌套层级、8 类 Slot、尺寸、缩放、边距及资源 | 待本地验证 |
| 新会话重开与运行时实例化 | 待本地验证 |
| 两种视口下的 Canvas 拉伸、Box Fill 与 ScaleBox 效果 | 待本地验证 |

离线命令（项目根目录，Python 需 jsonschema，Lua 需 5.4）：

```text
python Plugins/AutoBlueprint/Scripts/Tests/p2_schema_test.py
python Plugins/AutoBlueprint/Scripts/Tests/p5_contract_test.py
lua Plugins/AutoBlueprint/Scripts/Tests/p0_scripts_test.lua
lua Plugins/AutoBlueprint/Scripts/Tests/p1_host_test.lua
lua Plugins/AutoBlueprint/Scripts/Tests/p2_sdk_test.lua
lua Plugins/AutoBlueprint/Scripts/Tests/p3_sdk_test.lua
lua Plugins/AutoBlueprint/Scripts/Tests/p4_sdk_test.lua
```

本地人工验收：

1. 执行第 1 节命令，打开 WBP_P5_Nested。设计尺寸 960×640。层级主干应为 RootCanvas → Overlay_Main → Border_Card → Column_Card → Row_Content → Size_Art → Scale_Art → IMG_Art；Row_Content 的另两个子节点依次为 Gap 和 BTN_Confirm。
2. 按第 2 节能力表检查实际 Slot 类型；边距存在于父容器生成的 Slot 中。确认 Column_Card 下标题为 Auto、Row_Content 为 Fill=2；Row_Content 下 Size_Art 为 Auto、按钮为 Fill=3。Gap 的期望尺寸是 24×12。
3. Size_Art 为 160×100，Size_Default 不勾选宽高覆盖，Size_Zero 勾选宽高覆盖且值为 0。Size_Default 的 Canvas Slot 开启 Auto Size，锚点 `[0,0]`、Alignment `[0.5,0.5]`、ZOrder 7。
4. Scale_Art 为 ScaleToFit/DownOnly，子 Image 的 ScaleBox Slot 为 Right/Top。用 NestedSaveReload 日志中保留的真实纹理样例检查缩放和 Border 九宫格；无外部图片的仓库样例中 Image 为空是预期。
5. 在 960×640、1280×720 两种 Designer/运行视口下观察：主 Overlay 四周保持 24 的 Canvas 边距，行列按 Fill 权重分配空间，ScaleBox 不错误拉伸宽高比。记录项目实际 DPI 设置和视口；这些视觉结论需要人工观察，自动化的属性断言不能代替。
6. 关闭并重新启动编辑器，重开资产，确认上述结构、属性、变量和图片仍在；CreateWidget/AddToViewport 检查实例正常显示。再次生成同名目标应拒绝且原资产保持不变。
7. 复制 JSON、改用新目标名，将 IMG_Art 的 slot.padding 改为 `[1,0,0,0]`，应返回 UNSUPPORTED_SLOT_PROPERTY 且不创建新资产；需要边距时改为显式包装容器，不用隐藏节点规避。
