# ProjectOne

基于 **Unreal Engine 5.3** 的 C++ 工程，使用 [UnLua](https://github.com/Tencent/UnLua) 在 Lua 侧编写游戏逻辑，美术资产通过 **Git LFS** 管理。

AutoBlueprint 文档：[架构设计](Plugins/AutoBlueprint/Docs/Architecture.md)、[编码规范 Constitution](Plugins/AutoBlueprint/Docs/constitution.md)、[Lua/UnLua 开发路线图](Plugins/AutoBlueprint/Docs/plan.md)、[P0 编译与验证步骤](Plugins/AutoBlueprint/Docs/P0-Validation.md)。

## 技术栈

| 组件 | 说明 |
|---|---|
| Unreal Engine | 5.3.2 |
| 工程模块 | `ProjectOne`（Runtime C++ 模块） |
| UnLua | 2.3.6，Lua 脚本位于 `Content/Script/` |
| UnLuaExtensions | LuaProtobuf / LuaRapidjson / LuaSocket |
| UnLuaTestSuite | UnLua 官方测试套件，默认未启用 |
| 版本控制 | Git + Git LFS |

## 环境要求

- **Unreal Engine 5.3.2**（通过 Epic Games Launcher 安装）
- **Visual Studio 2022**，组件清单见仓库根目录的 `.vsconfig`，首次打开解决方案时 VS 会提示按该文件安装
- **Git LFS 3.x 及以上**（`git lfs version` 检查）

## 克隆仓库

```powershell
# 1. 每台机器装一次 LFS
git lfs install

# 2. 克隆
git clone git@github.com:xxxxxiaoming/ProjectOne.git
cd ProjectOne
```

克隆后如果发现资产文件只有几百字节（内容是 `version https://git-lfs.github.com/spec/v1`），说明 LFS 没有生效，执行：

```powershell
git lfs install
git lfs pull
```

> CI 或只需要查阅代码的机器可以跳过资产下载：
> `GIT_LFS_SKIP_SMUDGE=1 git clone git@github.com:xxxxxiaoming/ProjectOne.git`

## 首次打开工程

1. 右键 `ProjectOne.uproject` → **Generate Visual Studio project files**
   （`.sln` 不入库，每台机器自行生成）
2. 打开生成的 `ProjectOne.sln`，选择 `Development Editor | Win64`，编译
3. 双击 `ProjectOne.uproject` 启动编辑器

首次启动会编译 UnLua 及其扩展插件，需要几分钟。UnLua 依赖的预编译 Lua 库已随仓库提供，无需额外安装。

## 目录结构

```
ProjectOne/
├─ Config/            # 项目配置（DefaultEngine/Game/Editor/Input.ini）
├─ Content/
│  ├─ Script/         # Lua 脚本（UnLua）
│  ├─ Collections/    # 资产收藏夹（本地，不入库）
│  └─ Developers/     # 个人沙盒目录（不入库）
├─ Plugins/
│  ├─ AutoBlueprint/            # UI 蓝图生成 Editor 插件，P0 源码已准备、待 UE 验证
│  ├─ UnLua/                    # Lua 运行时 + 预编译 Lua 库
│  ├─ UnLuaExtensions/          # Protobuf / Rapidjson / Socket
│  └─ UnLuaTestSuite/           # 测试套件（默认未启用）
├─ Source/            # C++ 代码
├─ .vsconfig          # Visual Studio 组件清单
├─ .gitattributes     # Git LFS 规则
└─ ProjectOne.uproject
```

## 版本控制约定

- **不入库**：`Binaries/`、`Intermediate/`、`Saved/`、`DerivedDataCache/`、`.vs/`、`.idea/`、`*.sln`、`Plugins/Developer/RiderLink/`
- **走 Git LFS**：`*.uasset`、`*.umap`、图片/音频/视频、DCC 源文件等，规则见 `.gitattributes`
- **普通 Git**：`Plugins/UnLua/Source/ThirdParty/` 下的预编译库（`*.lib`、`*.a`、`*.dll`、`*.pdb`）——内容不会变化，不占 LFS 流量
- 主分支为 `main`，个人开发请开分支后提 PR
- `Content/Developers/` 与 `Content/Collections/` 属于个人目录，不要提交

### 新增二进制文件类型时

`.gitattributes` 只覆盖了当前已知的扩展名。如果引入了新的二进制类型（例如 `.usd`、`.vdb`、`.hdr`、`.fbx` 之外的自定义格式），**先补规则再 `git add`**：

```powershell
git lfs track "*.usd"
git add .gitattributes
git add path/to/file.usd
```

已经提交后才想起来要转 LFS 的，需要用 `git lfs migrate import` 重写历史，成本高且要求所有人重新同步，所以务必在提交前确认。

检查某个文件是否会被 LFS 接管：

```powershell
git check-attr filter -- path/to/file.uasset   # 输出 filter: lfs 即正确
```

## 常见问题

| 现象 | 原因 / 处理 |
|---|---|
| 资产文件是几百字节的文本 | LFS 未安装或未初始化，执行 `git lfs install` 后 `git lfs pull` |
| 打开工程提示模块缺失 | 没编译 C++，先生成 `.sln` 并编译 `Development Editor` |
| 打开工程提示插件版本不匹配 | 清掉 `Intermediate/`、`Binaries/` 后重新生成工程文件并编译 |
| 提交时提示文件超过 100 MB | 该类型未纳入 LFS，参考上文补 `.gitattributes` |
