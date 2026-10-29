# 收银找零 demo｜UI 源图与玩法绑定

风格：**A 厚描边手账贴纸**。商品不拟人；钱币为手绘马克厚描边。主色天蓝色 / 软白底。

本目录保留 `feature_cashier` 上已经进仓的 ART-A01～A09 源图、Texture2D，以及编辑器导入的 Widget 空壳。逻辑分支只补玩法脚本和下面的绑定说明，**不改写**这些 `.uasset` / `.umap`。WidgetTree 仍是空的，要在 UE 5.3 Designer 里按 `Docs/WBP_DESIGNER_CHECKLIST.md` 补控件。

**优先用切图 PNG**（商品 icon、钱币、chip、扫码、顾客）。jpg 整板继续当版式参考：A06 / A07 / A08 只有 jpg；各套整板 jpg 与 `*_sheet_rgba.png` 用于对照。A05 chip 的焦点、按下还没齐，先用该面额的 default，等小钰补。清单见 `Docs/CUTOUTS.md`。

## 目录

| 路径 | 内容 |
|------|------|
| `Content/UI/Cashier/` | 四个 Widget 空壳（见下表） |
| `Textures/Icons/` | ART-A01 商品 icon |
| `Textures/ProductCard/` | ART-A02 商品卡态 |
| `Textures/FX/` | ART-A03 扫码光效 |
| `Textures/Currency/` | ART-A04 钱币 icon 与五态 |
| `Textures/Chips/` | ART-A05 已选面额 chip / 条 |
| `Textures/SettleBoard/` | ART-A06 结算板三画面 |
| `Textures/TopBar/` | ART-A07 顶栏 |
| `Textures/ShiftEnd/` | ART-A08 班末结算 |
| `Textures/Customer/` | ART-A09 顾客剪影 |
| `Docs/` | `ASSET_MAP.md`、`UMG_LAYOUT.md`、`CUTOUTS.md`、`WBP_DESIGNER_CHECKLIST.md`、交互稿终稿 |

源文件名保持锁定稿原名。映射见 `Docs/ASSET_MAP.md`。

## 状态命名

- **钱币按钮 / chip 单张五态**：默认 / 悬停 / 焦点 / 按下 / 禁用。词表里没有「锁定」。
- **已选面额条**：空态 / 有选中态 / 禁用。
- **ART-A06 三态**：Scanning（实付「？？」弱对比）→ PayShown（数字揭晓）→ MakingChange（显 Σ，中性灰，不暗示对错）。

## 已落地的 Widget 壳

类名就是分支上的资产名，路径 `/Game/UI/Cashier/`。商品卡和面额 chip 用这两个名字，不要再写成别的：

| 资产 | 职责 |
|------|------|
| `WBP_CashierMain` | 主界面，竖屏三区 |
| `WBP_CashierSettle` | 班末结算 ShiftSettle |
| `WBP_CashierProductCard` | 商品卡 |
| `WBP_CashierDenomChip` | 已选面额 chip |

Designer 里控件名以 `Docs/WBP_DESIGNER_CHECKLIST.md` 为准（必须 Exact）。玩法只读 `ACashierDirector:GetView()` 或 `BoardView`。不要读 `Session.order.tender`。`PushBoardView` 在未揭晓时会把 `PayValue` 清成 0；Scanning 的 `PayDisplay` 固定为 `？？`。结算板没有待找零字段。

Director 查找：`UCashierLibrary::GetDirector(self)`。默认 GameMode 是 `ACashierGameMode`，会生成这个 Actor。索引是 **Lua 1-based**。

### `WBP_CashierMain`

| 控件名 | 绑到 | 说明 |
|--------|------|------|
| `TxtClock` | `BoardView.RemainingSeconds` | 顶栏剩余秒。不要放实付 |
| `TxtScore` | `BoardView.HudScore`，且 `bHudScoreVisible` | 局中不给数字，避免分数跳动暴露对错。班末为真 |
| `ProductList` | 每项一个 `WBP_CashierProductCard` | 数据来自 `BoardView.Items[]` |
| `TxtReceivable` | `BoardView.AmountDue`，当 `bHasAmountDue` | 应收 S，只累加已扫 |
| `TxtPay` | `BoardView.PayDisplay` | Scanning 为 `？？`。只有 `bPayRevealed` 才读 `PayValue` |
| `TxtSigma` | `BoardView.ChangeSum`，当 `bShowChangeSum` | 已出面额合计 Σ。MakingChange / Confirm 才显示，含 0 |
| `BtnYen1` | `Input_AddDenom_1` 或 `AddDenom(1)` | `bDrawerEnabled` |
| `BtnYen5` | `Input_AddDenom_5` 或 `AddDenom(5)` | 同上 |
| `BtnYen10` | `Input_AddDenom_10` 或 `AddDenom(10)` | 同上 |
| `ChipList` | 每张一个 `WBP_CashierDenomChip` | 数据来自 `BoardView.BillStrip` |
| `BtnClear` | `Input_ClearDenoms` | 条空时玩法是空操作，按钮可禁用。不是锁定的 IA |
| `BtnConfirm` | `Input_Confirm` | `bConfirmEnabled`。Σ = 0 也可点。局中不要弹对错 |

### `WBP_CashierProductCard`

| 控件名 | 绑到 |
|--------|------|
| `ImgProductIcon` | `Items[i].Name` 对应的 icon |
| `ImgScannedMark` | `Items[i].bScanned` |
| `BtnProduct` | `ScanItem(Items[i].Index)`。已扫不可再点 |

### `WBP_CashierDenomChip`

| 控件名 | 绑到 |
|--------|------|
| `ImgChip` | `BillStrip` 里这一张的面额（1 / 5 / 10） |
| `BtnRemoveChip` | 最后一张走 `Input_RemoveLastDenom` |

### `WBP_CashierSettle`

`bHidePlayfield` 时盖住三区。只在 `bSettleValid` 时读数：

| 控件名 | 字段 |
|--------|------|
| `TxtShiftScore` | `ShiftScore`（同 `HudScore`） |
| `TxtShiftCorrect` | `CorrectCount`。最优单数在 `OptimalCount`，清单里还没有单独文本位 |
| `TxtShiftOrders` | 本班单数可先用 ledger 长度；玩法快照目前给的是正确 / 最优 / 错找合计 |
| `TxtShiftTime` | 班次时钟。快照在结算时 `RemainingSeconds` 为 0 |
| `BtnShiftContinue` | `Input_OpenSettle` |

错找差额合计是 `WrongAbsSum`，封顶后的扣分是 `WrongPenalty`，新纪录是 `bNewBest` / `BestScore`。清单里这几项目前没有独立控件名，Designer 补文本后再绑。星级没有档位，`GetView().stars` 是空。

## Enhanced Input

`DefaultInput.ini` 已经是 Enhanced Player Input。逻辑 Action 与设备无关。IMC / IA 仍要在编辑器创建，**名字必须和下表一致**。`UCashierLibrary::GetLogicalActionNames` 返回同一组 `FName`。建议放在 `Content/UI/Cashier/Input/`：`IA_ScanItem`、`IA_AddDenom_1`、`IA_AddDenom_5`、`IA_AddDenom_10`、`IA_RemoveLastDenom`、`IA_Confirm`、`IA_FocusNext`、`IA_FocusPrev`、`IA_OpenSettle`，以及 `IMC_Cashier`。

| Action | 触控 / 鼠标 | 键盘（建议） | 手柄（建议） | 生效状态 |
|--------|-------------|--------------|--------------|----------|
| `ScanItem` | 点未扫商品卡 | 焦点在商品上时 Space / Enter | A / × | Scanning |
| `AddDenom_1` | `BtnYen1` | `1` | 十字左 | PayShown、MakingChange |
| `AddDenom_5` | `BtnYen5` | `2` 或 `5` | 十字上 | 同上 |
| `AddDenom_10` | `BtnYen10` | `3` 或 `0` | 十字右 | 同上 |
| `RemoveLastDenom` | `BtnRemoveChip`（最后一张） | Backspace | B | 同上 |
| `Confirm` | `BtnConfirm` | Enter | A / × | 同上，含 Σ = 0 |
| `FocusNext` | — | Tab / 方向 | 左摇杆 | 有焦点时 |
| `FocusPrev` | — | Shift+Tab | 左摇杆反 | 同上 |
| `OpenSettle` | `BtnShiftContinue` | Enter | A | ShiftSettle |

路由会丢掉当前状态不接受的 Action。Enter 可以同时绑 `ScanItem` 和 `Confirm`。

**没有 `UndoScan`。** 已扫商品只靠灰态挡住连点。面额只有 `{1,5,10}`。没有「正好」快捷键。玩家认为不用找零时，一张不点，直接确认。

## 玩法脚本

`Content/Script/Cashier/`（UnLua 模块 `Cashier.CashierDirector`）和 `Source/ProjectOne/Cashier/`。状态机：`WaitCustomer → Scanning → PayShown → MakingChange → Confirm → NextCustomer`，到时进 `ShiftSettle`。校验是 `Σ == Pay - S`。分数旋钮在 `CashierKnobs.lua`，不是定稿表。

宿主自检：`lua Content/Script/Cashier/RunCashierTests.lua`。

## 本批仍缺

- **ART-A10** 通用焦点框尚未纳入。
- A02 商品卡、A06 结算板、A07 顶栏、A08 班末没有切图，沿用 jpg 整板。
- A05 chip 缺焦点 / 按下。
- IMC / IA 还没建。
- 四个 WBP 的 WidgetTree 还是空壳。

玩法 UI 放在 `Content/UI/Cashier/`，不要放进 `Plugins/UnLuaTestSuite`。
