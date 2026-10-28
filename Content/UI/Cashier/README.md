# 收银找零 demo｜UI 源图

风格：**A 厚描边手账贴纸**。商品不拟人；钱币为手绘马克厚描边。主色天蓝色 / 软白底。

本目录提交老板已锁定的 ART-A01～A09 源图（jpg）和布局文档。Texture2D 与 Widget Blueprint 的 `.uasset` 必须在 **UE 5.3 Editor** 里导入或创建。云端没有编辑器，这里不放任何 `.uasset` / `.umap`。

## 目录

| 路径 | 内容 |
|------|------|
| `Content/UI/Cashier/` | Widget Blueprint 的落点（编辑器创建后才有 `.uasset`） |
| `Textures/Icons/` | ART-A01 商品 icon |
| `Textures/ProductCard/` | ART-A02 商品卡态 |
| `Textures/FX/` | ART-A03 扫码光效 |
| `Textures/Currency/` | ART-A04 钱币 icon 与五态 |
| `Textures/Chips/` | ART-A05 已选面额 chip / 条 |
| `Textures/SettleBoard/` | ART-A06 结算板三画面 |
| `Textures/TopBar/` | ART-A07 顶栏 |
| `Textures/ShiftEnd/` | ART-A08 班末结算 |
| `Textures/Customer/` | ART-A09 顾客剪影 |
| `Docs/` | `ASSET_MAP.md`、`UMG_LAYOUT.md`、交互稿终稿 |

源文件名保持锁定稿原名。映射见 `Docs/ASSET_MAP.md`。

## 状态命名

- **钱币按钮 / chip 单张五态**：默认 / 悬停 / 焦点 / 按下 / 禁用。词表里没有「锁定」。
- **已选面额条**：空态 / 有选中态 / 禁用。
- **ART-A06 三态**：Scanning（实付「？？」弱对比）→ PayShown（数字揭晓）→ MakingChange（显 Σ，中性灰，不暗示对错）。

## 编辑器里要建的 Widget

类名由 Linsey 指定，资产放在 `Content/UI/Cashier/`：

| 类名 | 职责 |
|------|------|
| `WBP_CashierMain` | 主界面，竖屏三区 |
| `WBP_CashierSettle` | 班末结算 ShiftSettle |
| `WBP_CashierProductCard` | 可选入口（商品卡） |
| `WBP_CashierDenomChip` | 可选入口（面额 chip） |

## 本批没有的东西

- **ART-A10** 通用焦点框尚未纳入。
- 当前多为整板 jpg。导入 Texture2D 前需要抠透明底，PNG 另补。

玩法 UI 放在 `Content/UI/Cashier/`，不要放进 `Plugins/UnLuaTestSuite`。
