"""Offline P5 layout/Schema cases; native slots and persistence require UE automation."""
import copy
import json
import re
from pathlib import Path
from jsonschema import Draft202012Validator

root = Path(__file__).resolve().parents[2]
schema = json.loads((root / "Resources/Schemas/layout.schema.json").read_text())
validator = Draft202012Validator(schema)
fixture = json.loads((root / "Examples/p5_nested.layout.json").read_text())

def nodes(node):
    yield node
    for child in node.get("children", []):
        yield from nodes(child)

def find(document, name):
    return next(n for n in nodes(document["root"]) if n["name"] == name)

def valid(document):
    assert validator.is_valid(document), [e.message for e in validator.iter_errors(document)]

valid(fixture)
items = list(nodes(fixture["root"]))
assert len(items) == 17
assert len({n["name"].lower() for n in items}) == 17
assert {n["type"] for n in items} == {key[4:] for key in schema["$defs"] if key.startswith("node") and key != "node"}
parents = {"CanvasPanel": "Canvas", "Overlay": "Overlay", "HorizontalBox": "HorizontalBox",
           "VerticalBox": "VerticalBox", "Border": "Border", "SizeBox": "SizeBox", "ScaleBox": "ScaleBox", "Button": "Button"}
for parent in items:
    for child in parent.get("children", []):
        assert child["slot"]["type"] == parents[parent["type"]]

count = 1
for mode in schema["$defs"]["nodeScaleBox"]["properties"]["properties"]["properties"]["stretch"]["enum"]:
    for direction in ("Both", "DownOnly", "UpOnly"):
        document = copy.deepcopy(fixture)
        document["root"] = copy.deepcopy(find(document, "Scale_Art"))
        document["root"].pop("slot")
        document["root"]["children"] = []
        document["root"]["properties"].update(stretch=mode, stretch_direction=direction)
        valid(document)
        count += 1

for name, section, field, value in [
    ("Size_Art", "properties", "width_override", -1),
    ("Size_Art", "properties", "height_override", 16385),
    ("Scale_Art", "properties", "user_specified_scale", 0),
    ("Scale_Art", "properties", "stretch", "Unknown"),
    ("BTN_Confirm", "slot", "size_rule", "Unknown"),
    ("BTN_Confirm", "slot", "fill", 0),
    ("Gap", "properties", "size", [1]),
    ("Border_Card", "properties", "content_color", [1, 1, 1, 2]),
    ("Column_Card", "slot", "padding", [0, -1, 0, 0]),
]:
    document = copy.deepcopy(fixture)
    target = find(document, name)
    target[section][field] = value
    target["children"] = []
    document["root"]["children"] = [target]
    assert not validator.is_valid(document), (name, field)
    count += 1

# Schema v1 preserves the padding field. UE semantic validation rejects nonzero ScaleBox padding.
document = copy.deepcopy(fixture)
target = find(document, "IMG_Art")
target["slot"]["padding"] = [1, 0, 0, 0]
document["root"]["children"] = [target]
valid(document)
count += 1
registry = (root / "Source/AutoBlueprintEditor/Private/Widgets/WidgetTypeRegistry.cpp").read_text()
assert len(re.findall(r"&U\w+::StaticClass, true,", registry)) == 11
print(f"PASS: {count} P5 structural cases, 17-node parent slots, all 11 types enabled (no UE execution)")
