import json

INPUT_FILE = "configClient.json"
OUTPUT_FILE = "configClient.json"  # sobrescribe el mismo archivo


def collapse_weights(obj):
    if isinstance(obj, dict):
        for key, value in obj.items():
            if key == "weights":
                obj[key] = [0]
            else:
                collapse_weights(value)
    elif isinstance(obj, list):
        for item in obj:
            collapse_weights(item)


with open(INPUT_FILE, "r") as f:
    data = json.load(f)

collapse_weights(data)

with open(OUTPUT_FILE, "w") as f:
    json.dump(data, f, indent=2)

print("✨ JSON limpio y weights → [0]")