#!/usr/bin/env python3
"""
Які виклики Android у зібраному коді новіші за minSdk — і де саме.

Застосунок збирається проти android-36, а працювати мусить з Android 5
(API 21). Компілятор про виклик, якого на старому телефоні немає, не
попереджає — телефон просто впаде з NoSuchMethodError, коли дійде до цього
рядка. Android Studio ловить таке через lint; тут lint немає, тож цей
скрипт робить головне з того самого: проходить скомпільовані класи (javap)
і звіряє кожне звернення з таблицею версій SDK (api-versions.xml).

Кожен рядок звіту має стояти за перевіркою Build.VERSION.SDK_INT — це
перевіряється очима: скрипт показує, у якому методі шукати.

    python3 scripts/api-check.py <тека-класів> <api-versions.xml> [minSdk]
"""
import os
import re
import subprocess
import sys
import xml.etree.ElementTree as ET

classes_dir, api_xml = sys.argv[1], sys.argv[2]
min_sdk = int(sys.argv[3]) if len(sys.argv) > 3 else 21
javap = os.path.join(os.environ.get("JAVA_HOME", "/usr"), "bin", "javap")

# ---- таблиця версій --------------------------------------------------------
api = {}  # клас -> {since, extends:[...], members:{назва+опис: since}}
for c in ET.parse(api_xml).getroot().iter("class"):
    since = int(c.get("since", "1"))
    members = {}
    for m in list(c.iter("method")) + list(c.iter("field")):
        members[m.get("name")] = int(m.get("since", since))
    parents = [e.get("name") for e in c.iter("extends")] + [e.get("name") for e in c.iter("implements")]
    api[c.get("name")] = {"since": since, "parents": parents, "members": members}


def member_since(owner, key, seen=None):
    """Версія, з якої член є в класі owner (з урахуванням предків), або None."""
    seen = seen or set()
    if owner in seen or owner not in api:
        return None
    seen.add(owner)
    info = api[owner]
    if key in info["members"]:
        return info["members"][key]
    for p in info["parents"]:
        s = member_since(p, key, seen)
        if s is not None:
            return s
    return None


# ---- власні класи й їхні предки ------------------------------------------------
files = []
for d, _, names in os.walk(classes_dir):
    files += [os.path.join(d, n) for n in names if n.endswith(".class")]
out = subprocess.run([javap, "-c", "-p"] + sorted(files), capture_output=True, text=True).stdout

own_parents = {}
for m in re.finditer(r"^(?:[\w ]*?)(?:class|interface) ([\w.$]+)(?: extends ([\w.$]+))?(?: implements ([\w.$, ]+))?", out, re.M):
    name = m.group(1).replace(".", "/")
    parents = []
    if m.group(2):
        parents.append(m.group(2).replace(".", "/"))
    if m.group(3):
        parents += [p.strip().replace(".", "/") for p in m.group(3).split(",")]
    own_parents[name] = parents


def resolve(owner, key):
    """Для власного класу — шукати член у його предках із SDK."""
    if owner in api:
        return member_since(owner, key)
    for p in own_parents.get(owner, []):
        s = resolve(p, key)
        if s is not None:
            return s
    return None


# ---- звернення ---------------------------------------------------------------
ref = re.compile(r"// (Method|InterfaceMethod|Field|class|InvokeDynamic) ([^\s]+)")
report = {}
cls = meth = None
for line in out.splitlines():
    m = re.match(r"^(?:[\w ]*?)(?:class|interface) ([\w.$]+)", line)
    if m and not line.startswith(" "):
        cls, meth = m.group(1), None
        continue
    if line.startswith("  ") and not line.startswith("   ") and line.rstrip().endswith(";") and "(" in line:
        meth = line.strip()
        continue
    r = ref.search(line)
    if not r:
        continue
    kind, what = r.groups()
    since, label = None, None
    if kind == "class":
        owner = what.strip('"').lstrip("[").lstrip("L").rstrip(";")
        if owner in api:
            since, label = api[owner]["since"], owner
    elif kind == "InvokeDynamic":
        # лямбда: тип функціонального інтерфейсу — у типі результату
        t = re.search(r"\)L([\w/$]+);", what)
        if t and t.group(1) in api:
            since, label = api[t.group(1)]["since"], t.group(1) + " (лямбда)"
    else:
        if "." not in what or ":" not in what:
            continue
        owner, rest = what.split(".", 1)
        name, desc = rest.split(":", 1)
        owner = owner.strip('"')
        name = name.strip('"')
        key = name + desc if kind != "Field" else name
        if owner.startswith("ua/"):
            since = resolve(owner, key)
        elif owner in api:
            since = member_since(owner, key)
            if since is None:
                since = api[owner]["since"]
        label = owner + "." + name
    if since and since > min_sdk:
        report.setdefault((since, label), set()).add(f"{cls} → {meth}")

if not report:
    print(f"  нових за API {min_sdk} звернень немає")
for (since, label), where in sorted(report.items()):
    print(f"  API {since:>2}  {label}")
    for w in sorted(where):
        print(f"          у {w}")
