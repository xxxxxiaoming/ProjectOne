# Create named WBP assets only (UE5.3 Python cannot edit WidgetTree).
import unreal
unreal.load_module("UMGEditor")
EAL = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
DEST = "/Game/UI/Cashier"
NAMES = [
    "WBP_CashierMain",
    "WBP_CashierSettle",
    "WBP_CashierProductCard",
    "WBP_CashierDenomChip",
]
# cleanup probes
for p in ("_ProbeWBP", "_ProbeWBP2"):
    full = DEST + "/" + p
    if EAL.does_asset_exist(full):
        EAL.delete_asset(full)
        unreal.log("[wbp] deleted probe " + full)

if not EAL.does_directory_exist(DEST):
    EAL.make_directory(DEST)

created = []
for name in NAMES:
    full = DEST + "/" + name
    if EAL.does_asset_exist(full):
        EAL.delete_asset(full)
    factory = unreal.WidgetBlueprintFactory()
    wbp = tools.create_asset(name, DEST, unreal.WidgetBlueprint, factory)
    if not wbp:
        raise RuntimeError("create failed " + name)
    try:
        unreal.BlueprintEditorLibrary.compile_blueprint(wbp)
    except Exception as e:
        unreal.log("[wbp] compile warn %s" % e)
    EAL.save_asset(full, only_if_is_dirty=False)
    created.append(full)
    unreal.log("[wbp] created " + full)
unreal.log("[wbp] done count=%d" % len(created))
