# 收银找零 demo｜资源进仓映射（老板 OK ART-A01～A09）

风格：**A 厚描边手账贴纸**；商品不拟人；钱币手绘马克厚描边。主色天蓝色 / 软白底。

仓库落点（Linsey 2026-10-28 确认）：`Content/UI/Cashier/`

- Widget Blueprint 建在 `Content/UI/Cashier/`（与 `Textures/`、`Docs/` 同级）
- 贴图源文件在 `Content/UI/Cashier/Textures/`；商品 icon 用 `Textures/Icons/`

下表是锁定 jpg 整板，路径相对于 `Content/UI/Cashier/`。源文件名保持锁定稿原名。透明切图与 jpg 同目录，见下一节。

| ART | 锁定源文件 | Content 子路径 | 用途 |
|-----|------------|----------------|------|
| A01 | `v2f_ART-A01_icons_styleA.jpg` | `Textures/Icons/` | 商品 icon 套 |
| A02 | `v2f_ART-A02_styleA_sticker.jpg` | `Textures/ProductCard/` | 未扫/已扫/悬停/焦点卡态 |
| A03 | `v2f_ART-A03_scan_fx.jpg` | `Textures/FX/` | 扫码光效 |
| A04 | `v2f_ART-A04_currency_icons_handdrawn.jpg` + `v2f_ART-A04_five_states_handdrawn.jpg` | `Textures/Currency/` | ¥1/5/10 五态 |
| A05 | `v2f_ART-A05_from_user_ref.jpg` | `Textures/Chips/` | chip 五态 + 整条三态 |
| A06 | `v2e_ART-A06_Scanning.jpg`、`v2e_ART-A06_PayShown.jpg`、`v2e_ART-A06_MakingChange_Sigma.jpg` | `Textures/SettleBoard/` | 结算板三画面 |
| A07 | `v2f_ART-A07_topbar.jpg` | `Textures/TopBar/` | 顶栏时钟/分数（无实付） |
| A08 | `v2f_ART-A08_shift_settle.jpg` | `Textures/ShiftEnd/` | 班末结算版式 |
| A09 | `v2f_ART-A09_customer_enter_exit.jpg` | `Textures/Customer/` | 剪影入队·到台·离场 |

## 切图 PNG（优先导入）

单件 `A0x_*.png` 是 icon / 钱币 / chip / 扫码 / 顾客的优先导入资产，与上表 jpg 放在同一子目录。文件名与缺口见 `Docs/CUTOUTS.md`。

| ART | 切图 | 说明 |
|-----|------|------|
| A01 | `Textures/Icons/A01_*.png` | 商品 icon，优先于整板 jpg |
| A03 | `Textures/FX/A03_*.png` | 扫码帧 |
| A04 | `Textures/Currency/A04_*.png` | 钱币五态齐 |
| A05 | `Textures/Chips/A05_*.png` | chip 的焦点、按下未齐；先用该面额 default，等小钰补 |
| A09 | `Textures/Customer/A09_*.png` | 顾客入队 / 离场帧 |

A06 / A07 / A08 仍只有 jpg 整板，作版式参考。整板 jpg 与 `*_sheet_rgba.png` 用于对照，不代替单件切图。

## Widget Blueprint 类名（Linsey 指定）

在 UE Editor 中创建，资产放在 `Content/UI/Cashier/`。本仓库不提交这些类的二进制。

| 类名 | 职责 |
|------|------|
| `WBP_CashierMain` | 主界面，竖屏三区 |
| `WBP_CashierSettle` | 班末结算 ShiftSettle |
| `WBP_CashierProductCard` | 可选入口（商品卡） |
| `WBP_CashierDenomChip` | 可选入口（面额 chip） |

## 状态名（钉死，勿另造词）

**钱币按钮 / chip 单张五态**：默认 / 悬停 / 焦点 / 按下 / 禁用（**无「锁定」**）

**已选面额条**：空态 / 有选中态 / 禁用

**chip 色**：¥1 金 / ¥5 绿 / ¥10 蓝

**A06**：Scanning（实付「？？」弱对比）→ PayShown（数字揭晓）→ MakingChange（显 Σ，中性灰，不暗示对错）

## 未进本包

- **ART-A10** 通用焦点框：老板尚未在本批 OK，暂不进仓。
- A02 商品卡没有切图，仍用 jpg 整板。
- A05 chip 的焦点、按下未进仓；¥10 chip 也没有禁用。先用该面额 default。

## 二进制 .uasset

云端 VM **无 UE5.3 Editor**，本 PR **不伪造** Widget/Texture `.uasset`。本包提交：锁定 jpg、透明 PNG 切图、交互终稿与布局契约。Texture2D / UMG Widget Blueprint 由本机 Editor 导入或 Linsey 在编辑器内建壳后挂贴图。

玩法资产放在 `Content/UI/Cashier/`，不要放进 `Plugins/UnLuaTestSuite`。
