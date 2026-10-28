import unreal
unreal.load_module("UMGEditor")
EAL = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
full = "/Game/UI/Cashier/_ProbeWBP2"
wbp = EAL.load_asset(full)
gc = wbp.generated_class()
unreal.log("[p3] gc=%s" % gc)
unreal.log("[p3] gc dir widgetish=%s" % [a for a in dir(gc) if "idget" in a.lower() or "tree" in a.lower() or "root" in a.lower()])
for prop in ["WidgetTree", "widget_tree", "RootWidget", "root_widget"]:
    try:
        unreal.log("[p3] gc.%s = %s" % (prop, gc.get_editor_property(prop)))
    except Exception as e:
        unreal.log("[p3] gc.%s fail %s" % (prop, e))
# CDO
cdo = unreal.get_default_object(gc) if hasattr(unreal, "get_default_object") else gc.get_default_object()
unreal.log("[p3] cdo=%s type=%s" % (cdo, type(cdo)))
unreal.log("[p3] cdo dir widgetish=%s" % [a for a in dir(cdo) if "idget" in a.lower() or "tree" in a.lower() or "root" in a.lower()])
for prop in ["WidgetTree", "widget_tree", "RootWidget", "root_widget"]:
    try:
        unreal.log("[p3] cdo.%s = %s" % (prop, getattr(cdo, prop)))
    except Exception as e:
        unreal.log("[p3] cdo getattr %s fail" % prop)
    try:
        unreal.log("[p3] cdo.get %s = %s" % (prop, cdo.get_editor_property(prop)))
    except Exception as e:
        unreal.log("[p3] cdo.get %s fail" % prop)

# try create WidgetTree object and assign somehow via call_method
try:
    unreal.log("[p3] call_method names via help")
    # construct widget tree as outer=wbp
    wt = unreal.new_object(unreal.WidgetTree, wbp)
    unreal.log("[p3] new WidgetTree %s" % wt)
except Exception as e:
    unreal.log("[p3] new WidgetTree fail %s" % e)

# list if WidgetTree class exists
unreal.log("[p3] has WidgetTree class %s" % hasattr(unreal, "WidgetTree"))
if hasattr(unreal, "WidgetTree"):
    unreal.log("[p3] WidgetTree methods %s" % [a for a in dir(unreal.WidgetTree) if not a.startswith("_")][:40])
