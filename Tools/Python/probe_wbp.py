import unreal
EAL = unreal.EditorAssetLibrary
# list imported textures sample
for p in [
 "/Game/UI/Cashier/Textures/Icons/A01_water",
 "/Game/UI/Cashier/Textures/Currency/A04_yen1_default",
 "/Game/UI/Cashier/WBP_CashierMain",
]:
 unreal.log("[probe] %s exists=%s" % (p, EAL.does_asset_exist(p)))

factory = unreal.WidgetBlueprintFactory()
tools = unreal.AssetToolsHelpers.get_asset_tools()
path = "/Game/UI/Cashier"
name = "_ProbeWBP"
full = path + "/" + name
if EAL.does_asset_exist(full):
    EAL.delete_asset(full)
wbp = tools.create_asset(name, path, unreal.WidgetBlueprint, factory)
unreal.log("[probe] type=%s" % type(wbp))
attrs = [a for a in dir(wbp) if "widget" in a.lower() or "tree" in a.lower() or "root" in a.lower()]
unreal.log("[probe] attrs=%s" % attrs)
for a in ("widget_tree", "WidgetTree", "widgettree"):
    try:
        v = getattr(wbp, a)
        unreal.log("[probe] getattr %s => %s" % (a, v))
    except Exception as e:
        unreal.log("[probe] getattr %s ERR %s" % (a, e))
try:
    v = wbp.get_editor_property("widget_tree")
    unreal.log("[probe] get_editor_property widget_tree => %s" % v)
except Exception as e:
    unreal.log("[probe] get_editor_property ERR %s" % e)
# editor property names containing widget
try:
    props = wbp.get_editor_property_names() if hasattr(wbp, "get_editor_property_names") else []
    unreal.log("[probe] propnames sample")
except Exception:
    pass
# try BlueprintEditorLibrary
unreal.log("[probe] has WidgetBlueprintLibrary %s" % hasattr(unreal, "WidgetBlueprintLibrary"))
unreal.log("[probe] has UMGEditorSubsystem %s" % hasattr(unreal, "UMGEditorSubsystem"))
for n in dir(unreal):
    if "Widget" in n and ("Editor" in n or "Blueprint" in n or "Tree" in n):
        unreal.log("[probe] unreal.%s" % n)
EAL.delete_asset(full)
