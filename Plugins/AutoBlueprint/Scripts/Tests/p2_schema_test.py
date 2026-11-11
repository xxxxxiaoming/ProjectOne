"""Offline structural contract checks. Requires jsonschema; UE semantics are tested in AutoBlueprint.P2."""
import copy
import json
from pathlib import Path
from jsonschema import Draft202012Validator

root = Path(__file__).resolve().parents[2]
schema = json.loads((root / 'Resources/Schemas/layout.schema.json').read_text())
Draft202012Validator.check_schema(schema)
validator = Draft202012Validator(schema)
base = json.loads((root / 'Examples/minimal.layout.json').read_text())
count = 0

def check(document, valid):
    global count
    errors = list(validator.iter_errors(document))
    assert (not errors) == valid, [e.message for e in errors]
    count += 1

for path in (root / 'Examples').glob('*.json'):
    # invalid_slot and duplicate_name are structurally legal; UE semantics must reject them.
    check(json.loads(path.read_text()), True)
for path, value in [
    (['schema_version'], 2), (['schema_version'], True),
    (['unknown'], 1), (['design', 'width'], 0),
    (['root', 'type'], 'ListView'), (['root', 'is_variable'], 'false'),
    (['root', 'name'], 'bad name'), (['root', 'properties'], {'render_opacity': 2}),
    (['root', 'properties'], {'text': 'wrong widget'}),
    (['resources'], {'bad.alias': {'object_path': '/Game/T.T'}}),
    (['resources'], {'pic': {'object_path': '/Game/T.T', 'source_file': 'Content/UI/p.png', 'package_path': '/Game/UI/T'}}),
    (['resources'], {'pic': {'source_file': 'Content/UI/p.png'}}),
    (['root', 'children', 0, 'slot', 'anchors'], {'min': [0, 0], 'max': [2, 1]}),
    (['root', 'children', 0, 'properties', 'font_size'], 12.5),
    (['root', 'children', 1, 'properties'], {'style': {'focused': {'resource': 'pic'}}}),
    (['root', 'children', 0, 'slot', 'typo'], 0),
]:
    doc = copy.deepcopy(base)
    target = doc
    for key in path[:-1]:
        target = target[key]
    target[path[-1]] = value
    check(doc, False)
for field in ['name', 'type', 'is_variable']:
    doc = copy.deepcopy(base)
    del doc['root'][field]
    check(doc, False)
for kind, max_children in [('Image', 0), ('Button', 1)]:
    doc = copy.deepcopy(base)
    doc['root']['type'] = kind
    check(doc, False)
# Every schema widget has a native class registration and vice versa.
import re
registry = (root / 'Source/AutoBlueprintEditor/Private/Widgets/WidgetTypeRegistry.cpp').read_text()
native = set(re.findall(r'&U(\w+)::StaticClass', registry))
assert native == {key[4:] for key in schema['$defs'] if key.startswith('node') and key != 'node'}
print(f'PASS: {count} schema cases and registry parity (no UE execution)')
