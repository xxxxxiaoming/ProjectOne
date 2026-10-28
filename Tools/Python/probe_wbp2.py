import unreal
unreal.log("[p2] loading modules")
for m in ("UMGEditor", "Blutility", "EditorScriptingUtilities", "UMG"):
    try:
        unreal.load_module(m)
        unreal.log("[p2] loaded " + m)
    except Exception as e:
        unreal.log("[p2] load %s fail %s" % (m, e))

# list widget-related after load
hits = [n for n in dir(unreal) if "WidgetTree" in n or "WidgetBlueprint" in n or "UMGEditor" in n]
unreal.log("[p2] hits=%s" % hits)

factory = unreal.WidgetBlueprintFactory()
tools = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
path = "/Game/UI/Cashier"
name = "_ProbeWBP2"
full = path + "/" + name
if EAL.does_asset_exist(full):
    EAL.delete_asset(full)
wbp = tools.create_asset(name, path, unreal.WidgetBlueprint, factory)
# try all editor properties
try:
    # C++ WidgetTree might be under different exposure
    for prop in ["WidgetTree", "widget_tree", "RootWidget", "root_widget", "SimpleConstructionScript"]:
        try:
            v = wbp.get_editor_property(prop)
            unreal.log("[p2] prop %s = %s" % (prop, v))
        except Exception as e:
            unreal.log("[p2] prop %s fail" % prop)
except Exception as e:
    unreal.log("[p2] props loop %s" % e)

# BaseWidgetBlueprint methods
unreal.log("[p2] BaseWidgetBlueprint dir sample: %s" % [a for a in dir(unreal.BaseWidgetBlueprint) if not a.startswith("_")][:40])
unreal.log("[p2] WidgetBlueprint dir sample: %s" % [a for a in dir(unreal.WidgetBlueprint) if not a.startswith("_")][:60])

# try created class
try:
    gc = wbp.generated_class()
    unreal.log("[p2] generated_class %s" % gc)
except Exception as e:
    unreal.log("[p2] generated_class err %s" % e)

# save empty wbp as proof
EAL.save_asset(full)
unreal.log("[p2] saved empty " + full)
