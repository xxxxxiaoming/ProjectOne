"""Create unique P4 manual-test PNG/JPEG and layout inputs. Python standard library only.
Run from the project root. This prepares raw files; only UE generates .uassets.
"""
import argparse
import base64
import json
from pathlib import Path
import uuid

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--font", help="Optional existing /Game UFont object path with Chinese glyphs")
args = parser.parse_args()
project = Path.cwd()
plugin = project / "Plugins/AutoBlueprint"
if not (plugin / "Examples/p4_styles.layout.json").is_file():
    parser.error("Run from the ProjectOne root containing Plugins/AutoBlueprint")
if args.font and not args.font.startswith("/Game/"):
    parser.error("--font must be an existing /Game UFont object path")

identifier = uuid.uuid4().hex
source = Path("Content/UI/AutoBlueprintTests") / ("P4_Manual_" + identifier)
source.mkdir(parents=True, exist_ok=False)
layout = json.loads((plugin / "Examples/p4_styles.layout.json").read_text(encoding="utf-8"))
package = "/Game/UI/AutoBlueprintTests/P4_Manual_" + identifier
layout["asset"]["package_path"] = package + "/WBP_Styles"
payloads = {'button': 'iVBORw0KGgoAAAANSUhEUgAAAEAAAABACAIAAAAlC+aJAAAAkklEQVR4nO3aoRGAMBAAwYRJIVSEw9AEtdAGOCpERL8J4njm1iBicvMT1Nd5O0tmE32Bt1r/3MfK3mPAsl/lBxMwgGYArUUH/Y1/R/SfTD8BA2gG0AygGUAzgGYAzQCaATQDaAbQDKAZQDOAZgDNAJoBNANoBtAMoBlAM4BmAM0AWrgvlGWPMf0EDKAZQKtur8Me2YQKZYxFKVwAAAAASUVORK5CYII=', 'photo': '/9j/4AAQSkZJRgABAQAAAQABAAD/2wBDAAgGBgcGBQgHBwcJCQgKDBQNDAsLDBkSEw8UHRofHh0aHBwgJC4nICIsIxwcKDcpLDAxNDQ0Hyc5PTgyPC4zNDL/2wBDAQkJCQwLDBgNDRgyIRwhMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjL/wAARCABAAEADASIAAhEBAxEB/8QAHwAAAQUBAQEBAQEAAAAAAAAAAAECAwQFBgcICQoL/8QAtRAAAgEDAwIEAwUFBAQAAAF9AQIDAAQRBRIhMUEGE1FhByJxFDKBkaEII0KxwRVS0fAkM2JyggkKFhcYGRolJicoKSo0NTY3ODk6Q0RFRkdISUpTVFVWV1hZWmNkZWZnaGlqc3R1dnd4eXqDhIWGh4iJipKTlJWWl5iZmqKjpKWmp6ipqrKztLW2t7i5usLDxMXGx8jJytLT1NXW19jZ2uHi4+Tl5ufo6erx8vP09fb3+Pn6/8QAHwEAAwEBAQEBAQEBAQAAAAAAAAECAwQFBgcICQoL/8QAtREAAgECBAQDBAcFBAQAAQJ3AAECAxEEBSExBhJBUQdhcRMiMoEIFEKRobHBCSMzUvAVYnLRChYkNOEl8RcYGRomJygpKjU2Nzg5OkNERUZHSElKU1RVVldYWVpjZGVmZ2hpanN0dXZ3eHl6goOEhYaHiImKkpOUlZaXmJmaoqOkpaanqKmqsrO0tba3uLm6wsPExcbHyMnK0tPU1dbX2Nna4uPk5ebn6Onq8vP09fb3+Pn6/9oADAMBAAIRAxEAPwDuqKKqXt6lpHgYaU/dX+pr5BJt2R7cISnJRitS3RXMXF3NcnMjkjso4AqJXZGDIxVh0IODWvsn3PSWWStrLU6yismy1UlxHckYPR8Y5961qzlFx3OCtQnRlyyCiiipMgrmLu4NzcvIc4PCg9hXT1ybqUdkYYZTgj3raluz1MsS5pPqJRRRWx64V0WnXBuLNS2dy/KSe+K52t7R1K2RJHDOSPpwP6VnV+E4MxSdK77l+iiiuc8MKydVsmLG5jGRj5x3+ta1FVGXK7mtCtKjPmiclRXRXGnW9wdxUq3dk4zUa6PbKwJMjD0J4/StvaxPXWY0mru5xl5eXEV26JJhRjAwPSnJ4h1REVFusKowB5a9Pyo8Qoqa7cqihVG3AAwPuisyvVp04ShFuK2PzvH4/ETxFT95K3M7K701NX/hI9W/5+//ACGn+FdP4cvbi/0+SW5k3uJSoO0DjA9PrXB12nhH/kEy/wDXc/8AoK1hjKcI0rxSRrllerPEJSk2rPqb9FFFeSfRhRRRQBwPiP8A5D9z/wAA/wDQRWVXeXvhyzv7yS5lknDvjIVhjgAentVf/hEdP/57XP8A30v/AMTXrU8ZSjBRfRHzlfLMROrKStZt9Ti67Twj/wAgmX/ruf8A0FaP+ER0/wD57XP/AH0v/wATWnp2nQ6ZbtDCzsrOXJcgnOAOwHpWWJxNOpT5Ym+AwFahW557H//Z'}
for alias, extension in (("button", "png"), ("photo", "jpg")):
    path = source / (alias + "." + extension)
    with path.open("xb") as output:
        output.write(base64.b64decode(payloads[alias]))
    layout["resources"][alias] = {"source_file": path.as_posix(), "package_path": package + "/T_" + alias}
if args.font:
    layout["root"]["children"][0]["properties"]["font"] = args.font

layout_path = Path("UIBlueprintLayouts") / ("p4_manual_" + identifier + ".json")
layout_path.parent.mkdir(exist_ok=True)
with layout_path.open("x", encoding="utf-8") as output:
    json.dump(layout, output, ensure_ascii=False, indent=2)
    output.write("\n")
print("Prepared inputs only; no UE assets created.")
print("In the idle UE editor Output Log, run:")
print("AutoBlueprint.GenerateFromFile " + layout_path.as_posix())
print("Expected Widget: " + package + "/WBP_Styles")
