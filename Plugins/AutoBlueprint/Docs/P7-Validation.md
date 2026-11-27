# P7 收银界面测试素材与本地验收

P7 使用写实收银柜台作为测试界面。素材和布局已准备，UE 5.3.2 本地验收状态以第 5 节为准。

## 1 生成蓝图

更新 `feature_auto_blueprint_plugin` 后，确认 `Content/UI/Textures` 中的 11 张 `T_Cashier_*.png` 已由 Git LFS 下载。可在项目根目录执行 `git lfs pull --include="Content/UI/Textures/T_Cashier_*.png"`。

在 UE 5.3.2 的普通编辑状态，通过 Output Log 依次执行：

```text
AutoBlueprint.Run validate_from_file.lua UIBlueprintLayouts/p7_cashier.layout.json
AutoBlueprint.GenerateFromFile UIBlueprintLayouts/p7_cashier.layout.json
```

只读预检成功时会输出目标 Widget 路径。生成任务应导入、保存 11 个纹理包，再保存 Widget。生成后在 Content Browser 中打开 `/Game/UI/AutoBlueprintTests/P7Cashier/WBP_P7_CashierMain`。Texture2D 统一保存在其同级的 `Textures` 子目录。控制台会输出各包保存结果和 JSON 报告路径。

## 2 样例内容

设计尺寸为 1024 × 1536。外层 ScaleBox 使用 ScaleToFit，等比缩放固定尺寸的画布。建议先以设计尺寸对照，再用 1080 × 1920 检查居中、留边和文字可读性。记录实际视口和项目 DPI。

| 区域 | 验收时看到的内容 |
| --- | --- |
| 顶栏 | 剩余时间 01:24 和本班分 120 |
| 商品与扫码 | 购物篮里的面包、已扫状态和独立的扫码窗 |
| 小票 | 「已扫描」和面包 ¥8 |
| 结算板 | 应收 ¥8、实付 ¥10、已出合计 ¥1 |
| 钱箱 | 可编辑的 ¥1 标签、¥1/¥5/¥10 按钮和橙色「确认」 |

这是 MakingChange 状态的静态审阅样例。文字、金额和已扫标记由 UMG 叠加。三个面额按钮共享一个银色键帽，通过画刷色彩实现正常、悬停、按下和禁用外观。按钮事件、计时与订单数据由后续业务接入。

这一版沿用 UMG 默认字体。请在 Designer 和打包版本检查「已出合计」、¥ 和 Σ。若中文缺字，可在本地准备含所需字形的 UFont，并在审阅布局中为 TextBlock 指定该资产后重新生成。

## 3 生成后检查

1. 对照 Designer 的层级和画面，确认贴图层与文字分开，扫码窗和小票没有挡住按钮，已扫面包处于禁用态。
2. 检查倒计时、分数、三个金额、已选面额容器和按钮的 Is Variable。退出并重启编辑器，重新打开蓝图，确认层级和纹理引用都能恢复。
3. 在本地测试关卡用生成类 Create Widget 并 Add to Viewport。用鼠标验收面额和确认键的悬停、按下和热区。
4. 将实际引用该蓝图的测试关卡纳入本地 cook/打包。通过 Reference Viewer 检查运行时资产依赖，并在打包版本打开测试关卡，确认 Widget、纹理和文字可用。

## 4 常见结果

| 结果 | 处理 |
| --- | --- |
| LFS_SOURCE_NOT_MATERIALIZED | 先下载 LFS 对象，再预检 |
| ASSET_ALREADY_EXISTS | 已有包受 CreateOnly 保护。复测可复制布局并更换 Widget 及纹理包名，也可对已导入纹理使用明确的 object_path 复用 |
| PACKAGE_SAVE_FAILED 或 RESOURCE_SAVE_FAILED | 查看输出报告中的独立 Widget 和纹理状态。先确认哪些包已保存，再决定复用或选择新目标 |

## 5 检查记录与本地验收

本节是 P7 验收状态的依据。

| 检查 | 状态 |
| --- | --- |
| P7 布局 Schema、71 个控件命名与父子 Slot、资源映射 | 云端检查通过 |
| P2 Schema 和 P5 结构检查回归 | 云端通过（29 项、32 项） |
| 11 张 PNG 可解码、透明通道及 LFS 记录 | 云端检查通过 |
| UE 5.3.2 只读预检、导入 11 张纹理、蓝图编译与保存 | 待本地验证 |
| Designer 对照、中文与符号、变量和按钮热区 | 待本地验证 |
| 重启后重开蓝图、运行时实例化与显示 | 待本地验证 |
| 运行时资产依赖及 cook/打包 | 待本地验证 |
