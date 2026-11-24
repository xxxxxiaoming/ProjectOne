# P4 资源与常用样式验证

P4 已完成，**用户本地验证通过，以第 5 节“检查记录与本地验收”为准**（用户验收提交 `b3dbcb8`）。基于远程 `1bc8ceb` 开发，P3 的五项自动化和其他验收已由用户确认通过。P4 只增加纹理、Image、字体和按钮四态；P5 其他容器、GenerateBatch 和业务事件绑定仍未实现。

## 1 编译与调用入口

关闭编辑器，完整编译 `ProjectOneEditor / Development Editor / Win64`，再重新打开项目。新增了反射结构和函数，不能只依赖 Live Coding。普通编辑状态下执行，无需 Play：

```text
Automation RunTests AutoBlueprint.P4
Automation RunTests AutoBlueprint.P3
Automation RunTests AutoBlueprint.P2
Automation RunTests AutoBlueprint.P1
Automation RunTests AutoBlueprint.P0
```

人工样例需要两张实际图片。项目根目录运行以下脚本（Python 3 标准库即可），它会生成带唯一编号的 PNG、JPG 和布局 JSON，**不会创建 UE 资产**：

```text
python Plugins/AutoBlueprint/Scripts/Tests/prepare_p4_example.py
```

脚本输出可直接粘贴到 Output Log 的命令，例如：

```text
AutoBlueprint.GenerateFromFile UIBlueprintLayouts/p4_manual_<编号>.json
```

用脚本实际输出替换示意命令，不要把 `<编号>` 原样输入。生成后在 `/Game/UI/AutoBlueprintTests/P4_Manual_<编号>/` 中找到 `WBP_Styles`、`T_button`、`T_photo`。再次执行同一命令应报冲突；重新运行准备脚本会得到另一组新路径。没有准备图片时，直接运行仓库的 `p4_styles.layout.json` 会因源图不存在而失败，该文件是布局模板。

中文字体可选：准备一个**已保存、含中文字形的 UFont 资产**，将其对象路径传给脚本，例如 `--font /Game/UI/Fonts/F_Chinese.F_Chinese`（请换成自己的真实资产）。P4 不导入 TTF/OTF，也不接受单独的 UFontFace。未指定字体时保留原生默认字体，中文可能显示缺字。

## 2 能力与结果边界

| 输入 | 当前行为 |
| --- | --- |
| `resources.<别名>.object_path` | 加载并复用已有 UTexture2D，不改压缩、分组、sRGB 或脏标记，不重新保存；已有资源应由用户事先保存 |
| `source_file` + `package_path` | 从项目 `Content/UI` 内的 PNG/JPG/JPEG 创建 `/Game/UI` 下的新纹理；目标存在则拒绝 |
| 新纹理设置 | UI 分组、EditorIcon 压缩、sRGB 开启、NoMipmaps；只应用于本任务新导入纹理 |
| Image | `brush.resource`、线性 RGBA `tint`、`draw_as=Image/Box`、归一化四边 `margin`；默认图像尺寸取纹理尺寸，布局由 Slot 控制 |
| 通用透明度 | `render_opacity`；画刷 tint 的 alpha 与控件透明度分别生效 |
| TextBlock | 复用已有 UFont，保留字号、颜色、对齐、自动换行；未指定字体继续使用原生默认字体 |
| Button | normal / hovered / pressed / disabled 四态画刷；未指定状态保留原生样式；focused 明确拒绝 |

新增 C++/UnLua 门面：

```cpp
FAutoBlueprintGenerationResult GenerateFromLayout(
    const FString& LayoutJson, const FAutoBlueprintBuildOptions& Options);
FAutoBlueprintGenerationResult GenerateFromLayoutFile(
    const FString& ProjectRelativePath, const FAutoBlueprintBuildOptions& Options);
```

```lua
local api = require("auto_blueprint")
local options = UE.FAutoBlueprintBuildOptions()
options.bSave = false -- 创建内存对象但不保存；只读检查应调用 validateLayoutFile
local task = api.generateTaskFromFile("UIBlueprintLayouts/my_ui.json", options)
print(task.Widget.bSaved)
for _, resource in ipairs(task.Resources:ToTable()) do
    print(resource.Alias, resource.bReused, resource.bImported, resource.bSaved)
end
```

一次任务仍只有一个 UI。`task.Widget` 是原有单 Widget 结果，`Widget.bSaved` **只代表 Widget 包**；`task.Resources` 独立记录每张声明纹理。任务没有总 `bSaved`。字体只复用、不创建包，因此不是纹理导入结果项；引用和失败诊断仍会检查。

| 资源字段 | 含义 |
| --- | --- |
| Alias / SourceFile / TargetAssetPath | 配置别名、原始源文件路径、纹理对象路径；复用时 SourceFile 为空 |
| CreatedPackageName | 本任务实际创建的资源包；仅复用时为空；后续失败也保留 |
| bAttempted | 是否已经执行到该资源；中途停止后尚未执行的项为 false |
| bReused | 已成功加载已有纹理；这不表示本任务保存了它 |
| bImported | 已从源图创建纹理对象；不代表已写入磁盘 |
| bSaved | 本任务是否实际保存了该资源包；复用时始终为 false |
| bSuccess | 复用成功，或导入及其请求的保存成功；与整个任务是否成功独立 |

资源按别名顺序记录。完整预检或能力检查失败时资源结果为空，因为尚未进入资源阶段。诊断分别在任务、Widget、各资源的 `Diagnostics` 中；布局预检诊断保留在 Widget 结果中。Lua 会输出所有资源与 Widget 状态，再传播失败。

兼容性：原有 `BuildWidgetBlueprint`、`BuildFromLayoutFile`、`api.generateFromFile` 保留单 Widget 返回类型，声明非空资源时返回 `RESOURCE_TASK_REQUIRED`，在创建前停止。需要资源的脚本使用新增任务接口。Console 命令 `AutoBlueprint.GenerateFromFile` 已切换到任务接口；无资源的 P0/P3 布局继续可用。Schema 仍为版本 1。

## 3 保存顺序、报告与失败

流程：重新预检 → 解析字体/复用纹理或导入新纹理 → 构建控件树 → 编译 Widget → 按别名顺序保存新纹理 → 最后保存 Widget。源图在导入前再次读取和检查，交给工厂的就是这一份字节。资源缓存由任务内 FGCObject 跟踪，直到编译和保存结束。

- `bSave=false`：新纹理和 Widget 均留在内存，成功结果的 bSaved 仍为 false，名称仍被占用。
- 编译失败：已经导入的纹理和 Widget 均不保存。
- 第二张纹理保存失败：第一张可能已保存，第二张和 Widget 未保存；报告保留这些不同结果。
- Widget 保存失败：纹理可能全部已保存；不会删除已保存纹理或伪称回滚。
- 报告写入失败：增加 `REPORT_WRITE_FAILED` 警告，不改变真实资产结果。

任务报告位于 `Saved/AutoBlueprint/Reports/<GUID>.json`，`report_version=2`，包含 `widget`、`resources`、任务诊断及 `save_requested`。原有单 Widget API 仍输出版本 1 报告。Console 日志现在显示 `Generation report`。

保存器会先检查目标目录是否被普通文件占据，此种情况直接返回 `PACKAGE_SAVE_FAILED`，不会进入 SavePackage 的移动文件重试。因此 P3 的 `FileLuaAndSaveFailure` 已移除对这些引擎错误日志的要求，保留全部失败状态和磁盘断言；不是忽略错误。P3 另一个旧能力负例改用尚未实现的 SizeBox，因为空 Button style 已是 P4 的合法输入。

## 4 测试文件具体做什么

| 文件/测试 | 实际操作与检查 |
| --- | --- |
| `AutoBlueprintP4Tests.cpp` / ImportStylesAndReuse | 现场编码两张 8×8 PNG/JPG，导入但不保存，检查 Image 画刷、透明度、字号/字体和按钮四态；随后复用同一批纹理，确认共享分组、sRGB 和脏标记不变；还检查只读预检不创建目标和旧 API 在创建前拒绝资源 |
| SaveReloadAndLua | 准备一个真实字体副本并保存，通过真实 UnLua 脚本生成并保存两个纹理和一个 Widget；卸载这些包后从磁盘重载，检查样式/引用，再 CreateWidget 实例化；另生成一个保存的复用界面，确认已有纹理文件未重写 |
| PreflightNoImports | 分别提供缺图、LFS 指针、坏图片、错误 UE 类型、同名目标、focused 状态和未实现控件；检查错误码，以及第一张纹理和 Widget 都未创建 |
| CompileFailureSavesNothing | 将变量控件命名为 Slot，故意与原生属性冲突；确认编译失败，已经导入的两个纹理均没有保存 |
| PartialSaveReports | 用测试专属普通文件挡住第二张纹理或 Widget 的保存目录；检查前面已保存包仍存在、后面失败包未保存，并核对 JSON 报告；再次导入同名纹理应被拒绝，原文件字节不变 |
| `Scripts/Lua/tests/p4_generate.lua` | 在 UE 的独立 UnLua 工具环境里读取嵌套 Widget/资源结果，检查实际反射字段、两个纹理的保存状态和报告路径；不需要 Play |
| `Scripts/Tests/p4_sdk_test.lua` | 普通 Lua 下模拟 UE 返回值，检查先预检再生成、选项转发、复用与未保存状态、部分失败日志、错误传播、Console 脚本路由；不验证真实 UE 资产 |
| `Scripts/Tests/prepare_p4_example.py` | 为人工验收准备真实 PNG/JPG 和新路径布局，打印生成命令；不调用 UE，也不写 .uasset |

自动化测试只清理自己的 GUID 源图/布局/阻挡文件，释放测试创建的未保存对象的保留标志。已保存 `.uasset` 与报告保留供查看；不会删除用户资产。自动化保存样例的原始小图片在测试结束后清理，因此要看贴图效果可打开保存的纹理，要测试重新导入请用人工样例。真实字体测试使用 Engine Roboto 副本，验证引用持久化，不代表它覆盖中文。

典型错误码：

| 错误码 | 简要说明 |
| --- | --- |
| RESOURCE_TASK_REQUIRED | 带纹理资源的布局误用了只返回单 Widget 结果的旧接口 |
| INVALID_SOURCE_PATH / MISSING_RESOURCE | 源图路径不在允许范围，或文件缺失/不可读/为空/超过 32 MiB |
| LFS_SOURCE_NOT_MATERIALIZED | 源文件仍是 Git LFS 指针，先下载真实图片 |
| INVALID_IMAGE | 无法解码为 PNG/JPEG，或图片超过 16 百万像素 |
| INVALID_RESOURCE_TYPE | 纹理路径不是 UTexture2D，或字体路径不是 UFont |
| ASSET_ALREADY_EXISTS / DUPLICATE_PACKAGE_TARGET | 目标内存/磁盘包已存在，或本次多个输出使用同一包名 |
| RESOURCE_RESOLVE_FAILED | 预检后资源状态变化，构建时无法重新加载已有纹理/字体 |
| RESOURCE_IMPORT_FAILED | UE 纹理工厂未能创建纹理；检查结果中是否已创建了未保存包 |
| RESOURCE_SAVE_FAILED | 某张纹理保存失败，Widget 因此未保存；具体原因在该资源 Diagnostics 中 |
| PACKAGE_SAVE_FAILED | 对应包无法建立保存目录或 SavePackage 失败 |
| UNSUPPORTED_FOCUS_BRUSH | 原生按钮没有独立 focused 画刷，当前明确不支持 |
| BUILD_WIDGET_NOT_IMPLEMENTED | 协议认识该控件，但当前阶段还没有生成处理器 |
| BLUEPRINT_COMPILE_FAILED | Widget 编译失败；本任务新纹理和 Widget 均不保存 |

## 5 检查记录与本地验收

| 检查                             | 状态                              |
| ------------------------------ | ------------------------------- |
| Schema 样例与注册表一致性               | 云端通过（28 项；不执行 UE）               |
| Lua 5.4：P0/P1/P2/P3 回归及 P4 SDK | 云端通过；模拟 UE 门面                   |
| 人工样例准备脚本、PNG/JPEG 解码与生成 JSON   | 云端通过（隔离目录生成两次、校验图片解码/JSON/字体选项） |
| C++ 差异、CRLF、风格及阶段范围            | 云端源码检查；不等于编译                    |
| UE 5.3.2 Win64 完整编译/UHT        | ✅                               |
| AutoBlueprint.P4 五项自动化         | ✅                               |
| 当前版本 P3/P2/P1/P0 回归            | ✅                               |
| Designer 和运行预览：图片、四态、九宫格、中文    | ✅                               |
| 关闭并重启编辑器后重开与实例化                | ✅                               |

离线检查（项目根目录，Python 需 jsonschema，Lua 需 5.4）：

```text
python Plugins/AutoBlueprint/Scripts/Tests/p2_schema_test.py
lua Plugins/AutoBlueprint/Scripts/Tests/p0_scripts_test.lua
lua Plugins/AutoBlueprint/Scripts/Tests/p1_host_test.lua
lua Plugins/AutoBlueprint/Scripts/Tests/p2_sdk_test.lua
lua Plugins/AutoBlueprint/Scripts/Tests/p3_sdk_test.lua
lua Plugins/AutoBlueprint/Scripts/Tests/p4_sdk_test.lua
```

本地人工验收：

1. 用第 1 节准备脚本生成样例并执行输出命令。应看到两张纹理均 saved、Widget saved、Generation report 路径；核对版本 2 报告。
2. 打开 WBP_Styles，设计尺寸 960×640。根 Canvas 有标题、IMG_Photo 和 BTN_Confirm，按钮下有标签；图片的 Tint 为 `[1,0.5,0.25,0.8]`，Render Opacity 为 0.75。九宫格 Margin 为四边 0.25。
3. 在 Designer 检查按钮四种画刷。运行测试界面中检查默认、鼠标悬停、按下；将按钮设为 Disabled 后观察禁用态。四态分别白、绿、蓝、半透明灰色调。这里只验证生成样式，不要求事件绑定。
4. 使用自己的已保存中文 UFont 重新准备新布局，核对标题字形、字号 32、对齐和换行。默认 Roboto 下的缺字不能当作中文验收通过。预览条件记录为设计尺寸 960×640、实际视口尺寸和实际 DPI；不能仅凭自动化断言声明视觉通过。
5. 复制布局并更换 Widget 目标名，将 resources 改为现有纹理 `object_path`。生成后应显示 reused，资源 bSaved=false；检查复用纹理的既有设置及磁盘修改时间未变。
6. 重复原始导入命令应拒绝；换新 Widget 名但保留旧纹理导入目标也应拒绝。不要删旧资产来绕过 CreateOnly 检查。
7. 关闭编辑器并重开项目，再打开 Widget，检查图片和字体仍在、四态和变量仍在；用 CreateWidget/AddToViewport 显示。P4 自动化的同会话卸载重载不能替代这一步。

完整 cook/打包与完整页面视觉还原仍按后续阶段验收；P4 不宣称 P5–P7 已完成。


## 6 2026-11-24 纹理异步编译断言修复

用户运行 P4 自动化时，在 `FTextureCompilingManager::AddTextures` 命中 `bIsRoutingPostCompilation == false` 断言。截图调用链是编译完成处理 → UpdateResource → CreateResource → 再次加入纹理编译队列，说明完成回调中发生了不允许的重新注册；截图没有显示最初修改纹理的插件调用位置。

源码检查发现两处未遵守纹理编辑生命周期的写入：

| 位置 | 原行为 | 修复 |
| --- | --- | --- |
| `ResolvedResources.cpp` 新纹理导入 | 工厂返回后直接改 LODGroup、CompressionSettings、MipGenSettings、SRGB，再 PostEditChange；工厂先前发起的编译可能尚未完成 | 修改前调用 PreEditChange(nullptr)，修改后 PostEditChange；再用 FTextureCompilingManager::FinishCompilation 等待这一张新纹理编译及收尾完成，之后才继续画刷构建和保存 |
| `AutoBlueprintP4Tests.cpp` / ImportStylesAndReuse | 为测试“复用不改设置”，直接修改刚导入的纹理设置，没有 PreEditChange/PostEditChange | 使用完整的 PreEditChange → 设置修改 → PostEditChange → 等待该纹理完成编译流程，再清理测试脏标记并执行复用；增加任务返回后和测试夹具准备完成后的 IsCompiling=false 断言 |

原因是纹理分组、压缩、Mip 等设置参与编译产物的生成。旧编译还没结束就改变这些输入，可能导致引擎在完成旧编译、创建渲染资源时发现需要重新编译，进而触发截图中的断言。Epic 的 [UE-201308](https://issues.unrealengine.com/issue/UE-201308) 也记录了 UE 5.3.2 在导入后修改 UI 分组时的同类调用栈；这支持故障机制判断，但不能替代本项目的本地复测。

修复仅等待当前纹理，不调用全局 FinishAllCompilation，不关闭异步纹理编译，不屏蔽引擎断言。生产代码的已有资源复用分支保持只读。新纹理仍只在原有保存阶段写入磁盘，bSave 与各包结果语义不变。

修复交付时，云端仅完成差异、CRLF、生命周期调用顺序检查，未运行 UE。此后用户已完成本地验证，第 5 节的 UE 编译、P4 自动化、历史阶段回归、视觉及新会话验收均已通过；以下保留当时的复测步骤供后续回归参考。

修复时建议的针对性复测命令（当前验收已通过）：

```text
Automation RunTests AutoBlueprint.P4.ImportStylesAndReuse
Automation RunTests AutoBlueprint.P4
```

后续回归时，可整组连续运行两次，并在测试结束后观察编辑器后续 Tick，排查延迟发生的异步编译异常；再回归 P3/P2/P1/P0 和人工样例。这里保留的是复测方法，不是新的待验收要求；当前状态以第 5 节为准。
