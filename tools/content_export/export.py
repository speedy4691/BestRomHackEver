#!/usr/bin/env python3
"""Roadmap D1: export the game's current content into CSVs for the "Emerald Hack Content Data" sheet.

Read-only: it parses the repo's data files and writes one CSV per sheet tab to tools/content_export/out/.
Each CSV matches the tab's header row exactly; import it at A2 ("Replace data at selected cell").

Run from the repo root:  python3 tools/content_export/export.py [--story slice]
"""

import argparse
import csv
import glob
import json
import os
import re
import sys
from collections import OrderedDict, defaultdict

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT = os.path.join(ROOT, "tools", "content_export", "out")
STATUS = "In game"

# Maps for the vertical slice (Littleroot to Rustboro); story dialogue covers these by default.
SLICE_MAP_PREFIXES = ("LittlerootTown", "Route101", "OldaleTown", "Route102", "Route103",
                      "PetalburgCity", "Route104", "PetalburgWoods", "RustboroCity")

HEADERS = {
    "trainers": ["Trainer ID", "Trainer name", "Class", "Map", "Battle type", "Slot", "Species", "Level",
                 "Move 1", "Move 2", "Move 3", "Move 4", "Held item", "Ability", "Nature", "IVs", "EVs",
                 "AI level", "Status", "Notes"],
    "trainer_dialogue": ["Trainer ID", "Trainer name", "Map", "Intro line", "Defeat line", "Post-battle line",
                         "Rebattle line", "Status", "Notes"],
    "encounters": ["Map", "Method", "Time or condition", "Slot #", "Slot %", "Species", "Min level", "Max level",
                   "Source game", "Status", "Notes"],
    "items_on_maps": ["Map", "X", "Y", "Item", "Visible or hidden", "Quantity", "Location description", "Status",
                      "Notes"],
    "npcs": ["NPC ID", "Map", "X", "Y", "Sprite", "Role or purpose", "Dialogue or script reference", "Status",
             "Notes"],
    "story_dialogue": ["Scene ID", "Map", "Trigger", "Speaker", "Line #", "Text", "Requires flag", "Sets flag",
                       "Status", "Notes"],
    "lists": ["Species", "Moves", "Items", "Maps", "Trainer classes"],
}


def read(path):
    with open(os.path.join(ROOT, path), encoding="utf-8") as f:
        return f.read()


def write_csv(name, rows):
    os.makedirs(OUT, exist_ok=True)
    path = os.path.join(OUT, name + ".csv")
    # utf-8-sig adds a BOM so Google Sheets File > Import reads the file as UTF-8 (without it, … became â€¦).
    with open(path, "w", encoding="utf-8-sig", newline="") as f:
        w = csv.writer(f)
        w.writerow(HEADERS[name])
        for r in rows:
            assert len(r) == len(HEADERS[name]), (name, r)
            w.writerow(r)
    return path


def norm(s):
    """Same idea as tools/trainerproc: readable name -> CONSTANT_SUFFIX."""
    s = s.replace("♀", "_F").replace("♂", "_M").replace("é", "E").replace("É", "E")
    s = re.sub(r"['’.:]", "", s)
    s = re.sub(r"[^A-Za-z0-9]+", "_", s).strip("_")
    return s.upper()


def title_from_const(c, prefix):
    return " ".join(w.capitalize() for w in c[len(prefix):].split("_"))


# ---------------------------------------------------------------- names (species, moves, items, classes)

def load_species():
    """SPECIES_X -> unique readable name (forms get "-Form", e.g. Venusaur-Mega)."""
    order = re.findall(r"^\s*(SPECIES_\w+)\s*=\s*\d+", read("include/constants/species.h"), re.M)
    raw, files = {}, []
    for path in sorted(glob.glob(os.path.join(ROOT, "src/data/pokemon/species_info/gen_*_families.h"))):
        files.append(open(path, encoding="utf-8").read())
    # Entries built by a macro (e.g. [SPECIES_UNOWN_B] = UNOWN_MISC_INFO(B, ...)) take the name from the macro body.
    macro_names = {}
    for text in files:
        for m in re.finditer(r"#define\s+(\w+)[^\n]*\\\n(?:[^\n]*\\\n)*[^\n]*", text):
            n = re.search(r'\.speciesName\s*=\s*_\("([^"]*)"\)', m.group(0))
            if n:
                macro_names[m.group(1)] = n.group(1)
    for text in files:
        marks = list(re.finditer(r"\[(SPECIES_\w+)\]\s*=", text))
        for i, m in enumerate(marks):
            end = marks[i + 1].start() if i + 1 < len(marks) else len(text)
            body = re.split(r"^\s*#\s*define", text[m.end():end], maxsplit=1, flags=re.M)[0]
            n = re.search(r'\.speciesName\s*=\s*_\("([^"]*)"\)', body)
            cands = [x for x in re.findall(r"\b([A-Z][A-Z0-9_]+)\b", body) if x in macro_names]
            mac = next((x for x in cands if m.group(1).startswith("SPECIES_" + norm(macro_names[x]))), cands[0] if cands else None)
            if m.group(1) in raw:
                continue
            if n:
                raw[m.group(1)] = n.group(1)
            elif mac:
                raw[m.group(1)] = macro_names[mac]
    names, base_of_name, unresolved = {}, {}, []
    for c in order:
        if c in ("SPECIES_NONE", "SPECIES_EGG"):
            continue
        name = raw.get(c)
        if name is None:
            # Macro-built entries (e.g. Unown letters): borrow the family name from the longest known prefix.
            known = sorted(set(raw.values()), key=len, reverse=True)
            name = next((k for k in known if c.startswith("SPECIES_" + norm(k) + "_")), None)
            if name is None:
                unresolved.append(c)
                name = c.split("_")[1].capitalize()  # family name; forms get "-Form" below
        if name not in base_of_name:
            base_of_name[name] = c
            names[c] = name
        else:
            base, own = base_of_name[name], "SPECIES_" + norm(name)
            if c.startswith(own + "_"):
                suffix = c[len(own) + 1:]
            elif c.startswith(base + "_"):
                suffix = c[len(base) + 1:]
            else:
                suffix = c[len("SPECIES_"):]
            names[c] = name + "-" + "-".join(w.capitalize() for w in suffix.split("_"))
    return order, names, unresolved


def load_moves():
    text = read("src/data/moves_info.h")
    out = OrderedDict()
    for m in re.finditer(r'\[(MOVE_\w+)\]\s*=\s*\{\s*\.name\s*=\s*COMPOUND_STRING\("([^"]*)"\)', text):
        if m.group(1) != "MOVE_NONE":
            out[m.group(1)] = m.group(2)
    return out


def load_items():
    text = read("src/data/items.h")
    out = OrderedDict()
    for m in re.finditer(r'\[(ITEM_\w+)\]\s*=\s*\{\s*\.name\s*=\s*ITEM_NAME\("([^"]*)"\)', text):
        if m.group(1) != "ITEM_NONE":
            out[m.group(1)] = m.group(2)
    return out


def load_trainer_classes():
    text = read("include/constants/trainers.h")
    block = text[text.index("TRAINER_CLASS_PKMN_TRAINER_1"):]
    consts = re.findall(r"^\s*(TRAINER_CLASS_\w+)\s*[,=]", block, re.M)
    return OrderedDict((c, title_from_const(c, "TRAINER_CLASS_")) for c in consts if c != "TRAINER_CLASS_COUNT")


def load_aliases(header, prefix):
    """#define ITEM_OLD ITEM_NEW style aliases in a constants header."""
    text = read(header)
    aliases = dict(re.findall(r"^#define\s+(" + prefix + r"\w+)\s+(" + prefix + r"\w+)\s*(?://.*)?$", text, re.M))
    aliases.update(re.findall(r"^\s*(" + prefix + r"\w+)\s*=\s*(" + prefix + r"\w+)\s*,", text, re.M))  # enum aliases
    return aliases


def make_lookup(names, prefix, aliases=None):
    """Readable or CONSTANT text -> readable name, via the constant."""
    by_const = dict(names)
    by_name = {}
    for c, n in names.items():
        by_name.setdefault(n.lower(), n)
    aliases = aliases or {}

    def resolve(text):
        text = text.strip()
        if not text:
            return "", None
        c = text if text.startswith(prefix) else prefix + norm(text)
        for _ in range(3):
            c = aliases.get(c, c)
        if c in by_const:
            return by_const[c], None
        if text.lower() in by_name:  # e.g. "Castform" -> base form
            return by_name[text.lower()], None
        return text, "unmatched " + prefix.rstrip("_").lower() + " '" + text + "'"
    return resolve


# ---------------------------------------------------------------- maps and scripts

def load_maps():
    groups = json.loads(read("data/maps/map_groups.json"))
    maps = OrderedDict()
    for g in groups["group_order"]:
        if g.endswith("_Frlg"):  # FireRed/LeafGreen build only
            continue
        for name in groups[g]:
            path = os.path.join(ROOT, "data/maps", name, "map.json")
            if os.path.exists(path):
                maps[name] = json.load(open(path, encoding="utf-8"))
    return maps


def clean_text(s):
    s = s.replace("\\p", " / ").replace("\\n", " ").replace("\\l", " ")
    s = s.replace('\\"', '"').rstrip("$")
    if s.endswith("$"):
        s = s[:-1]
    s = s.replace("$", "")
    return re.sub(r"\s+", " ", s).strip()


LABEL_RE = re.compile(r"^([A-Za-z_][A-Za-z0-9_]*)::?\s*(?:@.*)?$")


def parse_inc_files():
    """Returns (texts: label -> text, blocks: label -> (file, [command lines]))."""
    texts, blocks = {}, OrderedDict()
    files = glob.glob(os.path.join(ROOT, "data/**/*.inc"), recursive=True)
    for path in files:
        rel = os.path.relpath(path, ROOT)
        cur, cur_lines, cur_strings = None, [], []
        def flush():
            if cur is None:
                return
            if cur_strings:
                texts[cur] = clean_text("".join(cur_strings))
            elif cur_lines and cur not in blocks:
                blocks[cur] = (rel, cur_lines)
        for line in open(path, encoding="utf-8"):
            line = line.rstrip("\n")
            m = LABEL_RE.match(line)
            if m:
                flush()
                cur, cur_lines, cur_strings = m.group(1), [], []
                continue
            st = re.match(r'\s*\.string\s+"(.*)"\s*$', line)
            if st:
                cur_strings.append(st.group(1))
                continue
            code = line.split("@")[0].strip()
            if code and cur is not None:
                cur_lines.append(code)
        flush()
    return texts, blocks


def args_of(line):
    parts = line.split(None, 1)
    if len(parts) < 2:
        return parts[0], []
    return parts[0], [a.strip() for a in parts[1].split(",")]


def map_of_file(rel, dir_to_id):
    m = re.match(r"data/maps/([^/]+)/", rel)
    return dir_to_id.get(m.group(1), "") if m else ""


# ---------------------------------------------------------------- trainers.party

def parse_party():
    text = read("src/data/trainers.party")
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    trainers = []
    for chunk in re.split(r"^=== (TRAINER_\w+) ===\s*$", text, flags=re.M)[1:]:
        if isinstance(chunk, str) and re.fullmatch(r"TRAINER_\w+", chunk):
            trainers.append({"id": chunk, "fields": OrderedDict(), "mons": []})
            continue
        paras = [p for p in re.split(r"\n\s*\n", chunk.strip("\n")) if p.strip()]
        if not paras:
            continue
        head, mons = paras[0], paras[1:]
        if not re.match(r"^\s*[A-Za-z ]+:", head.split("\n")[0]):  # no trainer fields, first paragraph is a mon
            mons = paras
            head = ""
        for l in head.split("\n"):
            if ":" in l:
                k, v = l.split(":", 1)
                trainers[-1]["fields"][k.strip()] = v.strip()
        for p in mons:
            lines = [l.strip() for l in p.split("\n") if l.strip()]
            mon = {"header": lines[0], "fields": OrderedDict(), "moves": []}
            for l in lines[1:]:
                if l.startswith("- "):
                    mon["moves"].append(l[2:].strip())
                elif ":" in l:
                    k, v = l.split(":", 1)
                    mon["fields"][k.strip()] = v.strip()
            trainers[-1]["mons"].append(mon)
    return trainers


def parse_mon_header(h):
    item = ""
    if "@" in h:
        h, item = [x.strip() for x in h.split("@", 1)]
    gender = ""
    g = re.search(r"\((M|F)\)\s*$", h)
    if g:
        gender, h = g.group(1), h[:g.start()].strip()
    nick = ""
    n = re.match(r"^(.*)\(([^()]+)\)\s*$", h)
    if n:
        nick, h = n.group(1).strip(), n.group(2).strip()
    return h.strip(), nick, gender, item


# ---------------------------------------------------------------- main

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--story", choices=["slice", "all"], default="all",
                    help="story dialogue for every map (default) or the slice maps only")
    opts = ap.parse_args()
    report = []

    species_order, species, unresolved_species = load_species()
    moves, items, classes = load_moves(), load_items(), load_trainer_classes()
    sp_lookup = make_lookup(species, "SPECIES_", load_aliases("include/constants/species.h", "SPECIES_"))
    mv_lookup = make_lookup(moves, "MOVE_", load_aliases("include/constants/moves.h", "MOVE_"))
    it_lookup = make_lookup(items, "ITEM_", load_aliases("include/constants/items.h", "ITEM_"))
    cls_lookup = make_lookup(classes, "TRAINER_CLASS_")
    maps = load_maps()
    dir_to_id = {d: m["id"] for d, m in maps.items()}
    emerald_maps = set(dir_to_id.values())
    texts, blocks = parse_inc_files()

    # label -> map from map.json object/coord/bg events, and how each label is triggered
    label_map, label_trigger, label_object = {}, {}, {}
    for d, m in maps.items():
        for o in m.get("object_events", []):
            s = o.get("script", "")
            label_map.setdefault(s, m["id"]); label_trigger.setdefault(s, "On talk (NPC)"); label_object.setdefault(s, o)
        for c in m.get("coord_events", []):
            if c.get("script"):
                label_map.setdefault(c["script"], m["id"]); label_trigger.setdefault(c["script"], "On step (trigger tile)")
        for b in m.get("bg_events", []):
            if b.get("script"):
                label_map.setdefault(b["script"], m["id"]); label_trigger.setdefault(b["script"], "On interact (sign/object)")
    for label, (rel, lines) in blocks.items():
        for l in lines:
            cmd, a = args_of(l)
            if cmd == "map_script" and len(a) == 2:
                label_trigger.setdefault(a[1], "On map load (" + a[0].replace("MAP_SCRIPT_", "").lower() + ")")
            elif cmd == "map_script_2" and len(a) == 3:
                label_trigger.setdefault(a[2], "On map load (when " + a[0] + " = " + a[1] + ")")

    # ---- trainer battles in scripts
    battles = {}       # trainer -> dict
    rematch_intro = {}
    trainer_texts = set()
    trainer_labels = set()
    for label, (rel, lines) in blocks.items():
        if "frlg" in rel.lower() or (rel.startswith("data/maps/") and not map_of_file(rel, dir_to_id)):
            continue  # FireRed/LeafGreen scripts
        for i, l in enumerate(lines):
            cmd, a = args_of(l)
            if not cmd.startswith("trainerbattle"):
                continue
            trainer_labels.add(label)
            if cmd == "trainerbattle_lavaridge":
                a = a[1:]
                cmd = "trainerbattle_single"
            if cmd in ("trainerbattle_rematch", "trainerbattle_rematch_double"):
                rematch_intro.setdefault(a[0], a[1])
                trainer_texts.update(a[1:3])
                continue
            if cmd == "trainerbattle_no_intro":
                intro, defeat = "", a[1]
            elif cmd == "trainerbattle_earlyrival":
                intro, defeat = "", a[2]
            elif cmd == "trainerbattle_two_trainers":
                intro, defeat = "", a[1]
            else:
                intro, defeat = a[1], a[2]
            post = ""
            for later in lines[i + 1:]:
                c2, a2 = args_of(later)
                if c2 in ("msgbox", "message") and a2:
                    post = a2[0]
                    break
                if c2 in ("end", "return", "goto", "releaseall"):
                    break
            trainer_texts.update(t for t in (intro, defeat, post) if t)
            if a[0] not in battles:
                battles[a[0]] = {"label": label, "file": rel, "intro": intro, "defeat": defeat, "post": post}
    for t in rematch_intro.values():
        trainer_texts.add(t)

    # trainer -> map: object event that runs the battle, else the scripts file, else the rematch table
    trainer_map = {}
    for t, b in battles.items():
        trainer_map[t] = label_map.get(b["label"]) or map_of_file(b["file"], dir_to_id)
    for m in re.finditer(r"REMATCH\(([^)]*)\)", read("src/battle_setup.c")):
        a = [x.strip() for x in m.group(1).split(",")]
        if len(a) == 6 and a[5].startswith("MAP_"):
            for t in a[:5]:
                trainer_map.setdefault(t, a[5])

    def text_of(label, notes):
        if not label:
            return ""
        if label in texts:
            return texts[label]
        notes.append("text " + label + " is defined in C or not found")
        return ""

    # ---- Trainers tab
    rows, no_mons, party = [], [], parse_party()
    party_names = {}
    for tr in party:
        f = tr["fields"]
        party_names[tr["id"]] = f.get("Name", "")
        if tr["id"] == "TRAINER_NONE":
            continue
        if not tr["mons"]:
            no_mons.append(tr["id"])
            continue
        cls, cls_note = cls_lookup(f.get("Class", "Pkmn Trainer 1"))
        double = f.get("Double Battle", f.get("Battle Type", "No"))
        btype = "Double" if double.lower() in ("yes", "doubles") else "Single"
        extra = [k + ": " + v for k, v in f.items() if k not in ("Name", "Class", "Double Battle", "Battle Type", "AI")]
        for slot, mon in enumerate(tr["mons"], 1):
            notes = []
            sp_text, nick, gender, item_text = parse_mon_header(mon["header"])
            sp, n = sp_lookup(sp_text); notes += [n] if n else []
            held, n = it_lookup(item_text); notes += [n] if n else []
            mv = []
            for x in mon["moves"][:4]:
                name, n = mv_lookup(x); mv.append(name); notes += [n] if n else []
            mv += [""] * (4 - len(mv))
            mf = mon["fields"]
            if nick:
                notes.append("Nickname: " + nick)
            if gender:
                notes.append("Gender: " + gender)
            notes += [k + ": " + v for k, v in mf.items() if k not in ("Level", "Ability", "Nature", "IVs", "EVs")]
            if slot == 1:
                if cls_note:
                    notes.append(cls_note)
                notes = extra + notes
            rows.append([tr["id"], f.get("Name", ""), cls, trainer_map.get(tr["id"], ""), btype, slot, sp,
                         mf.get("Level", ""), *mv, held, mf.get("Ability", ""), mf.get("Nature", ""),
                         mf.get("IVs", ""), mf.get("EVs", ""), f.get("AI", ""), STATUS, "; ".join(notes)])
    counts = {"trainers": len(rows)}
    write_csv("trainers", rows)
    report.append("Trainers: %d rows (%d trainers); skipped %d trainers with no Pokemon: %s"
                  % (len(rows), len({r[0] for r in rows}), len(no_mons), ", ".join(no_mons[:10]) + (" ..." if len(no_mons) > 10 else "")))

    # ---- Trainer dialogue tab
    rows = []
    for t, b in battles.items():
        notes = []
        if t not in party_names:
            notes.append("not in trainers.party")
        rows.append([t, party_names.get(t, ""), trainer_map.get(t, ""), text_of(b["intro"], notes),
                     text_of(b["defeat"], notes), text_of(b["post"], notes),
                     text_of(rematch_intro.get(t, ""), notes), STATUS,
                     "; ".join(["script " + b["label"]] + notes)])
    write_csv("trainer_dialogue", rows)
    counts["trainer_dialogue"] = len(rows)
    report.append("Trainer dialogue: %d rows (one per trainer battled from a script; Rebattle line = vanilla rematch intro)" % len(rows))

    # ---- Encounters tab
    enc = json.loads(read("src/data/wild_encounters.json"))
    method_names = {"land_mons": "Grass", "water_mons": "Water", "rock_smash_mons": "Rock Smash"}
    rod_names = {"old_rod": "Old Rod", "good_rod": "Good Rod", "super_rod": "Super Rod"}
    rows = []
    for group in enc["wild_encounter_groups"]:
        if group.get("label") != "gWildMonHeaders":
            continue
        fields = {f["type"]: f for f in group["fields"]}
        for e in group["encounters"]:
            if e["map"] not in emerald_maps:
                continue
            label = e.get("base_label", "")
            suffix = label.rsplit("_", 1)[-1] if "_" in label else ""
            time = suffix if suffix in ("Morning", "Day", "Evening", "Night") else "Any"
            note = "" if time != "Any" or label == "g" + "".join(w.capitalize() for w in e["map"][4:].split("_")) else "table " + label
            for ftype, fdef in fields.items():
                if ftype not in e:
                    continue
                mons, rates = e[ftype]["mons"], fdef["encounter_rates"]
                if ftype == "fishing_mons":
                    for rod, idxs in fdef["groups"].items():
                        total = sum(rates[i] for i in idxs)
                        for n, i in enumerate(idxs, 1):
                            if i < len(mons):
                                m = mons[i]
                                sp, sn = sp_lookup(m["species"])
                                rows.append([e["map"], rod_names[rod], time, n, round(100 * rates[i] / total), sp,
                                             m["min_level"], m["max_level"], "Emerald", STATUS, "; ".join(x for x in (note, sn) if x)])
                else:
                    for n, m in enumerate(mons, 1):
                        sp, sn = sp_lookup(m["species"])
                        rows.append([e["map"], method_names.get(ftype, ftype), time, n, rates[n - 1] if n <= len(rates) else "",
                                     sp, m["min_level"], m["max_level"], "Emerald", STATUS, "; ".join(x for x in (note, sn) if x)])
    wild_rows = len(rows)
    # full-moon table (src/moon.c)
    moon = re.sub(r"//.*", "", read("src/moon.c"))
    moon_entries = re.findall(r"\{\s*(MAP_\w+)\s*,\s*(SPECIES_\w+)\s*,\s*(\d+)[^}]*\}", moon)
    for mp, s, pct in moon_entries:
        rows.append([mp, "Grass", "Full moon", "", pct, sp_lookup(s)[0], "", "", "Emerald", STATUS, "full-moon override chance (src/moon.c)"])
    # static and gift encounters from scripts
    skipped_var = 0
    starters = re.findall(r"define (?:GRASS|FIRE|WATER)_STARTER\s*\(IS_FRLG \? \w+\s*:\s*(SPECIES_\w+)\s*\)", read("src/starter_choose.c"))
    for n, s in enumerate(starters, 1):
        rows.append(["MAP_ROUTE101", "Gift", "Any", n, "", sp_lookup(s)[0], 5, 5, "Emerald", STATUS,
                     "Starter chosen from Birch's bag (src/starter_choose.c, level from src/battle_setup.c)"])
    for label, (rel, lines) in blocks.items():
        if rel.endswith("debug.inc") or "/test" in rel or "frlg" in rel.lower():
            continue
        if rel.startswith("data/maps/") and not map_of_file(rel, dir_to_id):
            continue  # FireRed/LeafGreen map
        for l in lines:
            cmd, a = args_of(l)
            if cmd not in ("givemon", "setwildbattle", "giveegg") or not a:
                continue
            if not a[0].startswith("SPECIES_"):
                skipped_var += 1
                continue
            mp = label_map.get(label) or map_of_file(rel, dir_to_id)
            notes = ["script " + label]
            if not map_of_file(rel, dir_to_id):
                notes.append("shared script " + rel)
            lvl = a[1] if len(a) > 1 else ""
            if cmd == "giveegg":
                notes.append("Egg")
            extra = [x for x in a[2:] if x and x != "ITEM_NONE"]
            if extra:
                notes.append("extra: " + ", ".join(extra))
            rows.append([mp, "Static" if cmd == "setwildbattle" else "Gift", "Any", "", "", sp_lookup(a[0])[0],
                         lvl, lvl, "Emerald", STATUS, "; ".join(notes)])
    write_csv("encounters", rows)
    counts["encounters"] = len(rows)
    report.append("Encounters: %d rows (%d wild slots, %d full-moon, %d static/gift); time-of-day tables: %s; skipped %d givemon with a species from a variable"
                  % (len(rows), wild_rows, len(moon_entries), len(rows) - wild_rows - len(moon_entries),
                     "none yet (all Any)" if all(r[2] == "Any" for r in rows[:wild_rows]) else "yes", skipped_var))

    # ---- Items on maps + NPCs
    item_rows, npc_rows, skipped_objects = [], [], defaultdict(int)
    not_npc_gfx = {"OBJ_EVENT_GFX_CUTTABLE_TREE": "cuttable tree", "OBJ_EVENT_GFX_BREAKABLE_ROCK": "breakable rock",
                   "OBJ_EVENT_GFX_PUSHABLE_BOULDER": "boulder"}
    for d, m in maps.items():
        mid = m["id"]
        for idx, o in enumerate(m.get("object_events", []), 1):
            s, gfx = o.get("script", ""), o.get("graphics_id", "")
            if s == "Common_EventScript_FindItem":
                it, n = it_lookup(o.get("trainer_sight_or_berry_tree_id", ""))
                item_rows.append([mid, o["x"], o["y"], it, "Visible", 1, "", STATUS, "; ".join(x for x in ("flag " + o.get("flag", ""), n) if x)])
                continue
            if s == "BerryTreeScript":
                item_rows.append([mid, o["x"], o["y"], "", "Berry tree", "", "", STATUS, "berry tree " + o.get("trainer_sight_or_berry_tree_id", "")])
                continue
            if gfx in not_npc_gfx:
                skipped_objects[not_npc_gfx[gfx]] += 1
                continue
            if s in trainer_labels or o.get("trainer_type", "TRAINER_TYPE_NONE") != "TRAINER_TYPE_NONE":
                skipped_objects["trainer"] += 1
                continue
            lid = o.get("local_id") or str(idx)
            if s in ("0x0", "0", ""):
                role, ref, note = title_from_const(gfx, "OBJ_EVENT_GFX_"), "", "no talk script (scripted actor)"
            else:
                short = re.sub(r"^.*?_EventScript_", "", s)
                role = re.sub(r"(?<=[a-z])(?=[A-Z0-9])", " ", short)
                ref, note = s, ""
            if o.get("flag") not in (None, "0"):
                note = "; ".join(x for x in (note, "hidden by " + o["flag"]) if x)
            npc_rows.append([mid + ":" + lid, mid, o["x"], o["y"], gfx.replace("OBJ_EVENT_GFX_", ""), role, ref, STATUS, note])
        for b in m.get("bg_events", []):
            if b.get("type") == "hidden_item":
                it, n = it_lookup(b.get("item", ""))
                item_rows.append([mid, b["x"], b["y"], it, "Hidden", b.get("quantity", 1), "", STATUS,
                                  "; ".join(x for x in ("flag " + b.get("flag", ""), n) if x)])
    write_csv("items_on_maps", item_rows)
    write_csv("npcs", npc_rows)
    counts["items_on_maps"], counts["npcs"] = len(item_rows), len(npc_rows)
    report.append("Items on maps: %d rows (incl. %d berry trees)" % (len(item_rows), sum(1 for r in item_rows if r[4] == "Berry tree")))
    report.append("NPCs: %d rows; not included: %s" % (len(npc_rows), ", ".join("%d %s" % (v, k) for k, v in sorted(skipped_objects.items()))))

    # ---- Story dialogue
    def story_rows(map_filter):
        out = []
        for label, (rel, lines) in blocks.items():
            mm = re.match(r"data/maps/([^/]+)/scripts.inc", rel)
            if not mm or mm.group(1) not in dir_to_id or not map_filter(mm.group(1)):
                continue
            msgs = []
            for l in lines:
                cmd, a = args_of(l)
                if cmd in ("msgbox", "message") and a and a[0] not in trainer_texts:
                    msgs.append(a[0])
            if not msgs:
                continue
            req = sorted({a[0] for cmd, a in map(args_of, lines)
                          if cmd in ("goto_if_set", "goto_if_unset", "call_if_set", "call_if_unset", "checkflag") and a and a[0].startswith("FLAG_")})
            sets = sorted({a[0] for cmd, a in map(args_of, lines) if cmd == "setflag" and a})
            obj = label_object.get(label)
            speaker = ""
            if obj is not None:
                speaker = re.sub(r"(?<=[a-z])(?=[A-Z0-9])", " ", re.sub(r"^.*?_EventScript_", "", label))
            trigger = label_trigger.get(label, "Called from another script")
            for n, t in enumerate(msgs, 1):
                notes = []
                line = text_of(t, notes)
                named = re.match(r"^([A-Z][A-Z0-9 .'&]{1,20}): ", line)  # e.g. "PROF. BIRCH: ..."
                out.append([label, dir_to_id.get(mm.group(1), ""), trigger, named.group(1) if named else speaker, n, line,
                            ", ".join(req), ", ".join(sets), STATUS, "; ".join(["text " + t] + notes)])
        return out
    all_story = story_rows(lambda d: True)
    rows = all_story if opts.story == "all" else story_rows(lambda d: d.startswith(SLICE_MAP_PREFIXES))
    write_csv("story_dialogue", rows)
    counts["story_dialogue"] = len(rows)
    report.append("Story dialogue: %d rows written (%s); full set across all maps would be %d rows in %d scenes"
                  % (len(rows), "slice maps Littleroot to Rustboro" if opts.story == "slice" else "all maps",
                     len(all_story), len({r[0] for r in all_story})))

    # ---- Lists
    cols = [[species[c] for c in species_order if c in species], list(moves.values()), list(items.values()),
            [m["id"] for m in maps.values()], list(classes.values())]
    n = max(len(c) for c in cols)
    rows = [[c[i] if i < len(c) else "" for c in cols] for i in range(n)]
    write_csv("lists", rows)
    counts["lists"] = n
    report.append("Lists: %d species, %d moves, %d items, %d maps, %d trainer classes%s"
                  % (*[len(c) for c in cols], "; species names guessed from constants: " + ", ".join(unresolved_species) if unresolved_species else ""))

    print("\n".join(report))
    print("\nCSV files in", os.path.relpath(OUT, ROOT))


if __name__ == "__main__":
    sys.exit(main())
