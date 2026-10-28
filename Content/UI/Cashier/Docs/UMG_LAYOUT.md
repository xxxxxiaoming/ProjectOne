# 收银找零｜UMG 布局契约（交互侧）

竖屏安全区按微信小游戏刘海预留。同一 Widget 树接多平台；禁止「仅 Touch」才能点到的逻辑。

Widget 放在 `Content/UI/Cashier/`。贴图源在 `Content/UI/Cashier/Textures/`（见 `ASSET_MAP.md`）。类名由 Linsey 指定；`.uasset` 在 UE Editor 内创建，本仓库只提交源图与文档。

| 类名 | 职责 |
|------|------|
| `WBP_CashierMain` | 主界面，竖屏三区 |
| `WBP_CashierSettle` | 班末结算 ShiftSettle |
| `WBP_CashierProductCard` | 可选入口（商品卡） |
| `WBP_CashierDenomChip` | 可选入口（面额 chip） |

## 屏级结构（自上而下）

1. **顶栏**（ART-A07）：时钟、本班分；**禁止**放实付。
2. **顾客区**（ART-A09）：剪影入队 / 到台 / 离场；对错同一套离场；无扫光束。
3. **商品区**（ART-A01 + A02 + A03）：可点未扫卡；已扫灰/勾且不可点；短扫光效。
4. **结算板**（ART-A06）：应收；实付 Scanning=「？？」、PayShown=数字；MakingChange 显 Σ；**无待找零、无正好标签**。
5. **钱箱区**（ART-A04 + A05）：¥1/5/10 五态；已选条 空态/有选中态/禁用；清空钮（Σ=0 时禁用）；确认收款（文案「确认」，非「确认收款」长文）。

## 关键交互摘要

| 态 | UI 要点 |
|----|---------|
| WaitCustomer | 自动来客；钱箱可禁用 |
| Scanning | 实付「？？」；钱箱整区禁用；扫完自动进 PayShown |
| PayShown | 揭晓实付；可开始出面额 |
| MakingChange | 显 Σ；可移除末张/清空；Σ=0 也可确认（开钱箱，无正好捷径） |
| ShiftEnd | ART-A08 数据位留空 |

## 绑定约定（给 Linsey）

- `Pay` **仅**在进入 PayShown 时下发 UI；Scanning 不绑真值。
- Enhanced Input：无 `UndoScan`。
- 上表四个 `WBP_*` 为编辑器内要建的壳；可选入口是 `WBP_CashierProductCard` 与 `WBP_CashierDenomChip`。
