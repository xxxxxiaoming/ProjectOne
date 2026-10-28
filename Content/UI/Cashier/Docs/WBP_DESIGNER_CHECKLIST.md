# WBP Designer 补齐清单（UE5.3）

> 命令行 Python **无法**写入 WidgetTree（引擎属性 protected）。下面请在 Editor Designer 里按命名补齐。家家已 Import 全部 Texture2D，并创建空壳：
> `/Game/UI/Cashier/WBP_CashierMain|Settle|ProductCard|DenomChip`

竖屏 9:16。根用 `CanvasPanel` 名 `RootCanvas`，内层 `SafeArea` 铺满。

## WBP_CashierMain（自上而下）

| 区 | 锚点 Y | 控件名（必须 Exact） | 类型 | 贴图/文案 |
|----|--------|----------------------|------|-----------|
| TopBar | 0–0.08 | `TopBar` | Overlay | |
| | | `ImgTopBarBg` | Image | `Textures/TopBar/v2f_ART-A07_topbar` |
| | | `TxtClock` | Text | 00:00 |
| | | `TxtScore` | Text | 0 |
| CustomerZone | 0.08–0.22 | `CustomerZone` | Overlay | |
| | | `ImgCustomer` | Image | `Textures/Customer/A09_customer_frame1` |
| ProductZone | 0.22–0.52 | `ProductZone` | Overlay | |
| | | `ProductList` | HorizontalBox | 放 `WBP_CashierProductCard` |
| | | `ImgScanFx` | Image | `Textures/FX/A03_scan_frame1` |
| SettleBoard | 0.52–0.74 | `SettleBoard` | Overlay | |
| | | `ImgSettleBg` | Image | Scanning/PayShown/MakingChange 三态切换 |
| | | `TxtReceivable` | Text | 应收 |
| | | `TxtPay` | Text | ？？ → 揭晓数字 |
| | | `TxtSigma` | Text | Σ（仅 MakingChange 显） |
| CashDrawer | 0.74–1.0 | `CashDrawer` | Overlay | Scanning 整区禁用 |
| | | `DenomRow` | HorizontalBox | |
| | | `BtnYen1` `BtnYen5` `BtnYen10` | Button | 五态刷自 Currency/A04_yen*_… |
| | | `ChipBar` | Overlay | |
| | | `ImgChipBar` | Image | empty / selected |
| | | `ChipList` | HorizontalBox | `WBP_CashierDenomChip` |
| | | `BtnClear` | Button | 文案「清空」；Σ=0 禁用 |
| | | `BtnConfirm` | Button | 文案「确认」 |

## WBP_CashierSettle
- `ImgShiftBg` ← ShiftEnd jpg
- `TxtShiftTitle` / `TxtShiftOrders` / `TxtShiftCorrect` / `TxtShiftScore` / `TxtShiftTime`
- `BtnShiftContinue`

## WBP_CashierProductCard
- `ImgProductIcon` / `ImgScannedMark` / `BtnProduct`

## WBP_CashierDenomChip
- `ImgChip` / `BtnRemoveChip`

补齐后 Compile + Save，commit 推 `feature_cashier`，再喊 Linsey 绑 EI/状态机。