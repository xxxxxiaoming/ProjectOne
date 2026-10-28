# -*- coding: utf-8 -*-
"""Import Cashier textures + create WBP shells with named slots (UE 5.3)."""
import os
import unreal

PROJECT_CONTENT = r"E:\Repo\Unreal Projects\ProjectOne\Content"
TEX_ROOT = os.path.join(PROJECT_CONTENT, "UI", "Cashier", "Textures")
DEST_ROOT = "/Game/UI/Cashier"
TEX_DEST = DEST_ROOT + "/Textures"

FOLDER_MAP = {
    "Icons": TEX_DEST + "/Icons",
    "ProductCard": TEX_DEST + "/ProductCard",
    "FX": TEX_DEST + "/FX",
    "Currency": TEX_DEST + "/Currency",
    "Chips": TEX_DEST + "/Chips",
    "SettleBoard": TEX_DEST + "/SettleBoard",
    "TopBar": TEX_DEST + "/TopBar",
    "ShiftEnd": TEX_DEST + "/ShiftEnd",
    "Customer": TEX_DEST + "/Customer",
}

ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary


def log(msg):
    unreal.log("[CashierUI] " + str(msg))


def ensure_dir(game_path):
    if not EAL.does_directory_exist(game_path):
        EAL.make_directory(game_path)


def import_all_textures():
    tasks = []
    for folder, dest in FOLDER_MAP.items():
        ensure_dir(dest)
        disk = os.path.join(TEX_ROOT, folder)
        if not os.path.isdir(disk):
            log("skip missing folder " + disk)
            continue
        for fn in sorted(os.listdir(disk)):
            low = fn.lower()
            if not (low.endswith(".png") or low.endswith(".jpg") or low.endswith(".jpeg")):
                continue
            full = os.path.join(disk, fn)
            name = os.path.splitext(fn)[0]
            task = unreal.AssetImportTask()
            task.filename = full
            task.destination_path = dest
            task.destination_name = name
            task.replace_existing = True
            task.automated = True
            task.save = True
            tasks.append(task)
    log("import tasks: %d" % len(tasks))
    if tasks:
        ASSET_TOOLS.import_asset_tasks(tasks)
    # tune UI textures
    tuned = 0
    for folder, dest in FOLDER_MAP.items():
        disk = os.path.join(TEX_ROOT, folder)
        if not os.path.isdir(disk):
            continue
        for fn in os.listdir(disk):
            low = fn.lower()
            if not (low.endswith(".png") or low.endswith(".jpg") or low.endswith(".jpeg")):
                continue
            name = os.path.splitext(fn)[0]
            path = dest + "/" + name
            if not EAL.does_asset_exist(path):
                continue
            tex = EAL.load_asset(path)
            if not isinstance(tex, unreal.Texture2D):
                continue
            tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
            try:
                tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
            except Exception:
                pass
            if low.endswith(".png"):
                tex.set_editor_property("srgb", True)
            EAL.save_asset(path, only_if_is_dirty=False)
            tuned += 1
    log("tuned textures: %d" % tuned)
    return len(tasks)


def _canvas_fill(slot):
    anchors = unreal.Anchors()
    anchors.minimum = unreal.Vector2D(0.0, 0.0)
    anchors.maximum = unreal.Vector2D(1.0, 1.0)
    slot.set_anchors(anchors)
    slot.set_offsets(unreal.Margin(0.0, 0.0, 0.0, 0.0))
    try:
        slot.set_alignment(unreal.Vector2D(0.0, 0.0))
    except Exception:
        pass


def _canvas_band(slot, min_y, max_y, pad=8.0):
    anchors = unreal.Anchors()
    anchors.minimum = unreal.Vector2D(0.0, min_y)
    anchors.maximum = unreal.Vector2D(1.0, max_y)
    slot.set_anchors(anchors)
    slot.set_offsets(unreal.Margin(pad, pad, pad, pad))


def _set_image_tex(img, tex_path):
    if not EAL.does_asset_exist(tex_path):
        return False
    tex = EAL.load_asset(tex_path)
    if not isinstance(tex, unreal.Texture2D):
        return False
    brush = unreal.SlateBrush()
    brush.set_resource_object(tex)
    img.set_editor_property("brush", brush)
    return True


def create_or_replace_wbp(name):
    ensure_dir(DEST_ROOT)
    asset_path = DEST_ROOT + "/" + name
    if EAL.does_asset_exist(asset_path):
        EAL.delete_asset(asset_path)
    factory = unreal.WidgetBlueprintFactory()
    wbp = ASSET_TOOLS.create_asset(name, DEST_ROOT, unreal.WidgetBlueprint, factory)
    if not wbp:
        raise RuntimeError("failed to create " + name)
    return wbp, asset_path


def build_main(wbp):
    tree = wbp.widget_tree
    root = tree.construct_widget(unreal.CanvasPanel, "RootCanvas")
    tree.root_widget = root

    # Safe portrait canvas ~9:16
    safe = tree.construct_widget(unreal.CanvasPanel, "SafeArea")
    s = root.add_child_to_canvas(safe)
    _canvas_fill(s)

    # 1 TopBar ~8%
    top = tree.construct_widget(unreal.Overlay, "TopBar")
    ts = safe.add_child_to_canvas(top)
    _canvas_band(ts, 0.0, 0.08, 12.0)
    top_bg = tree.construct_widget(unreal.Image, "ImgTopBarBg")
    top.add_child(top_bg)
    _set_image_tex(top_bg, TEX_DEST + "/TopBar/v2f_ART-A07_topbar")
    clock = tree.construct_widget(unreal.TextBlock, "TxtClock")
    clock.set_editor_property("text", unreal.Text("00:00"))
    top.add_child(clock)
    score = tree.construct_widget(unreal.TextBlock, "TxtScore")
    score.set_editor_property("text", unreal.Text("0"))
    top.add_child(score)

    # 2 CustomerZone ~14%
    cust = tree.construct_widget(unreal.Overlay, "CustomerZone")
    cs = safe.add_child_to_canvas(cust)
    _canvas_band(cs, 0.08, 0.22, 12.0)
    img_c = tree.construct_widget(unreal.Image, "ImgCustomer")
    cust.add_child(img_c)
    _set_image_tex(img_c, TEX_DEST + "/Customer/A09_customer_frame1")

    # 3 ProductZone ~30%
    prod = tree.construct_widget(unreal.Overlay, "ProductZone")
    ps = safe.add_child_to_canvas(prod)
    _canvas_band(ps, 0.22, 0.52, 12.0)
    prod_list = tree.construct_widget(unreal.HorizontalBox, "ProductList")
    prod.add_child(prod_list)
    scan_fx = tree.construct_widget(unreal.Image, "ImgScanFx")
    prod.add_child(scan_fx)
    _set_image_tex(scan_fx, TEX_DEST + "/FX/A03_scan_frame1")

    # 4 SettleBoard ~22%
    settle = tree.construct_widget(unreal.Overlay, "SettleBoard")
    ss = safe.add_child_to_canvas(settle)
    _canvas_band(ss, 0.52, 0.74, 12.0)
    settle_bg = tree.construct_widget(unreal.Image, "ImgSettleBg")
    settle.add_child(settle_bg)
    _set_image_tex(settle_bg, TEX_DEST + "/SettleBoard/v2e_ART-A06_Scanning")
    txt_recv = tree.construct_widget(unreal.TextBlock, "TxtReceivable")
    txt_recv.set_editor_property("text", unreal.Text("应收"))
    settle.add_child(txt_recv)
    txt_pay = tree.construct_widget(unreal.TextBlock, "TxtPay")
    txt_pay.set_editor_property("text", unreal.Text("？？"))
    settle.add_child(txt_pay)
    txt_sigma = tree.construct_widget(unreal.TextBlock, "TxtSigma")
    txt_sigma.set_editor_property("text", unreal.Text("Σ 0"))
    settle.add_child(txt_sigma)

    # 5 CashDrawer ~26%
    drawer = tree.construct_widget(unreal.Overlay, "CashDrawer")
    ds = safe.add_child_to_canvas(drawer)
    _canvas_band(ds, 0.74, 1.0, 12.0)

    denom_row = tree.construct_widget(unreal.HorizontalBox, "DenomRow")
    drawer.add_child(denom_row)
    for label, tex in (
        ("BtnYen1", TEX_DEST + "/Currency/A04_yen1_default"),
        ("BtnYen5", TEX_DEST + "/Currency/A04_yen5_default"),
        ("BtnYen10", TEX_DEST + "/Currency/A04_yen10_default"),
    ):
        btn = tree.construct_widget(unreal.Button, label)
        denom_row.add_child_to_horizontal_box(btn)
        img = tree.construct_widget(unreal.Image, label + "_Img")
        # Button content via set_content if available
        try:
            btn.add_child(img)
        except Exception:
            try:
                btn.set_content(img)
            except Exception:
                pass
        _set_image_tex(img, tex)

    chip_bar = tree.construct_widget(unreal.Overlay, "ChipBar")
    drawer.add_child(chip_bar)
    chip_bg = tree.construct_widget(unreal.Image, "ImgChipBar")
    chip_bar.add_child(chip_bg)
    _set_image_tex(chip_bg, TEX_DEST + "/Chips/A05_bar_empty")
    chip_list = tree.construct_widget(unreal.HorizontalBox, "ChipList")
    chip_bar.add_child(chip_list)

    actions = tree.construct_widget(unreal.HorizontalBox, "ActionRow")
    drawer.add_child(actions)
    btn_clear = tree.construct_widget(unreal.Button, "BtnClear")
    actions.add_child_to_horizontal_box(btn_clear)
    btn_confirm = tree.construct_widget(unreal.Button, "BtnConfirm")
    actions.add_child_to_horizontal_box(btn_confirm)
    clear_label = tree.construct_widget(unreal.TextBlock, "TxtClear")
    clear_label.set_editor_property("text", unreal.Text("清空"))
    confirm_label = tree.construct_widget(unreal.TextBlock, "TxtConfirm")
    confirm_label.set_editor_property("text", unreal.Text("确认"))
    try:
        btn_clear.set_content(clear_label)
        btn_confirm.set_content(confirm_label)
    except Exception:
        pass


def build_settle(wbp):
    tree = wbp.widget_tree
    root = tree.construct_widget(unreal.CanvasPanel, "RootCanvas")
    tree.root_widget = root
    panel = tree.construct_widget(unreal.Overlay, "ShiftSettleRoot")
    s = root.add_child_to_canvas(panel)
    _canvas_fill(s)
    bg = tree.construct_widget(unreal.Image, "ImgShiftBg")
    panel.add_child(bg)
    _set_image_tex(bg, TEX_DEST + "/ShiftEnd/v2f_ART-A08_shift_settle")
    title = tree.construct_widget(unreal.TextBlock, "TxtShiftTitle")
    title.set_editor_property("text", unreal.Text("班末结算"))
    panel.add_child(title)
    for n in ("TxtShiftOrders", "TxtShiftCorrect", "TxtShiftScore", "TxtShiftTime"):
        t = tree.construct_widget(unreal.TextBlock, n)
        t.set_editor_property("text", unreal.Text("--"))
        panel.add_child(t)
    btn = tree.construct_widget(unreal.Button, "BtnShiftContinue")
    panel.add_child(btn)


def build_product_card(wbp):
    tree = wbp.widget_tree
    root = tree.construct_widget(unreal.CanvasPanel, "RootCanvas")
    tree.root_widget = root
    card = tree.construct_widget(unreal.Overlay, "CardRoot")
    s = root.add_child_to_canvas(card)
    _canvas_fill(s)
    icon = tree.construct_widget(unreal.Image, "ImgProductIcon")
    card.add_child(icon)
    _set_image_tex(icon, TEX_DEST + "/Icons/A01_water")
    check = tree.construct_widget(unreal.Image, "ImgScannedMark")
    card.add_child(check)
    btn = tree.construct_widget(unreal.Button, "BtnProduct")
    card.add_child(btn)


def build_denom_chip(wbp):
    tree = wbp.widget_tree
    root = tree.construct_widget(unreal.CanvasPanel, "RootCanvas")
    tree.root_widget = root
    chip = tree.construct_widget(unreal.Overlay, "ChipRoot")
    s = root.add_child_to_canvas(chip)
    _canvas_fill(s)
    img = tree.construct_widget(unreal.Image, "ImgChip")
    chip.add_child(img)
    _set_image_tex(img, TEX_DEST + "/Chips/A05_chip_yen1_default")
    btn = tree.construct_widget(unreal.Button, "BtnRemoveChip")
    chip.add_child(btn)


def compile_save(wbp, asset_path):
    try:
        unreal.BlueprintEditorLibrary.compile_blueprint(wbp)
    except Exception as e:
        log("compile warn: %s" % e)
    EAL.save_asset(asset_path, only_if_is_dirty=False)


def main():
    log("start")
    ensure_dir(DEST_ROOT)
    ensure_dir(TEX_DEST)
    n = import_all_textures()
    log("imported sources: %d" % n)

    builders = (
        ("WBP_CashierMain", build_main),
        ("WBP_CashierSettle", build_settle),
        ("WBP_CashierProductCard", build_product_card),
        ("WBP_CashierDenomChip", build_denom_chip),
    )
    created = []
    for name, builder in builders:
        wbp, path = create_or_replace_wbp(name)
        builder(wbp)
        compile_save(wbp, path)
        created.append(path)
        log("created " + path)

    # write marker for git follow-up
    marker = os.path.join(PROJECT_CONTENT, "UI", "Cashier", "Docs", "WBP_BUILD_NOTE.txt")
    with open(marker, "w", encoding="utf-8") as f:
        f.write("Built by UnrealEditor-Cmd Python on HappyMY7.\n")
        f.write("Widgets:\n")
        for p in created:
            f.write("  " + p + "\n")
        f.write("BindWidget names on Main: TopBar,TxtClock,TxtScore,CustomerZone,ImgCustomer,")
        f.write("ProductZone,ProductList,ImgScanFx,SettleBoard,ImgSettleBg,TxtReceivable,TxtPay,TxtSigma,")
        f.write("CashDrawer,DenomRow,BtnYen1,BtnYen5,BtnYen10,ChipBar,ImgChipBar,ChipList,BtnClear,BtnConfirm\n")
    log("done")


main()
