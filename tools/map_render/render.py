#!/usr/bin/env python3
"""Render each area's map from the game files, with numbered markers for trainers, items, NPCs and berry trees.

    python tools/map_render/render.py                    # all areas in areas.json -> build/maps/
    python tools/map_render/render.py --area "Route 104"
    python tools/map_render/render.py --upload           # also push each PNG to its Notion page

Everything is read from the repo: map.json, layouts.json + map.bin, tileset tiles/metatiles/palettes and
include/fieldmap.h. Nothing is taken from screenshots.
"""

import argparse
import datetime
import json
import os
import re
import subprocess
import sys

from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
OUT_DIR = os.path.join(ROOT, "build", "maps")

sys.path.insert(0, os.path.join(ROOT, "tools", "content_export"))
import export as cx  # noqa: E402  (reuse the content-export parsers for trainers.party, scripts and items)

METATILE_PX = 16
COLORS = {  # (fill, text)
    "T": ((214, 40, 40), (255, 255, 255)),
    "I": ((245, 200, 20), (0, 0, 0)),
    "IH": ((190, 130, 0), (255, 255, 255)),   # hidden item: darker shade of the item colour
    "N": ((40, 100, 220), (255, 255, 255)),
    "B": ((30, 150, 60), (255, 255, 255)),
    "W": ((140, 50, 190), (255, 255, 255)),
    "S": ((130, 80, 35), (255, 255, 255)),
}
SIGHT_RGBA = (230, 20, 20, 115)
DIRS = {"UP": (0, -1), "DOWN": (0, 1), "LEFT": (-1, 0), "RIGHT": (1, 0)}


def rel(path):
    return os.path.join(ROOT, path)


def read(path):
    with open(rel(path), encoding="utf-8") as f:
        return f.read()


def font(size, bold=False):
    names = ["DejaVuSans-Bold.ttf", "arialbd.ttf"] if bold else ["DejaVuSans.ttf", "arial.ttf"]
    for n in names:
        try:
            return ImageFont.truetype(n, size)
        except OSError:
            pass
    return ImageFont.load_default(size=size)


# ---------------------------------------------------------------- constants and tileset lookup

def fieldmap_constants():
    text = read("include/fieldmap.h")
    want = ["NUM_TILES_IN_PRIMARY", "NUM_METATILES_IN_PRIMARY", "NUM_PALS_IN_PRIMARY", "NUM_TILES_PER_METATILE"]
    out = {}
    for name in want:
        m = re.search(r"^#define\s+%s\s+(\d+)\b" % name, text, re.M)
        if not m:
            sys.exit("include/fieldmap.h: %s not found" % name)
        out[name] = int(m.group(1))
    gm = read("include/global.fieldmap.h")
    m = re.search(r"#define\s+MAPGRID_METATILE_ID_MASK\s+(0x[0-9A-Fa-f]+|\d+)", gm)
    out["MAPGRID_METATILE_ID_MASK"] = int(m.group(1), 0)
    return out


class TilesetIndex:
    """Maps a tileset symbol (gTileset_General) to its tiles.png, palette files and metatiles.bin."""

    def __init__(self):
        headers = read("src/data/tilesets/headers.h")
        self.headers = {}
        for m in re.finditer(r"const struct Tileset (gTileset_\w+)\s*=\s*\{(.*?)\};", headers, re.S):
            fields = dict(re.findall(r"\.(\w+)\s*=\s*([^,\n]+)", m.group(2)))
            self.headers[m.group(1)] = {k: v.strip() for k, v in fields.items()}
        gfx = read("src/graphics.c") + read("src/data/tilesets/graphics.h")
        self.tiles = {m.group(1): m.group(2) for m in
                      re.finditer(r"(gTilesetTiles_\w+)\[\]\s*=\s*INC\w+\(\s*\"([^\"]+)\"", gfx)}
        self.palettes = {}
        for m in re.finditer(r"(gTilesetPalettes_\w+)\[\]\[16\]\s*=\s*\{(.*?)\};", gfx, re.S):
            self.palettes[m.group(1)] = re.findall(r"\"([^\"]+\.pal)\"", m.group(2))
        mt = read("src/data/tilesets/metatiles.h")
        self.metatiles = {m.group(1): m.group(2) for m in
                          re.finditer(r"(gMetatiles_\w+)\[\]\s*=\s*INCBIN_U16\(\s*\"([^\"]+)\"", mt)}

    def files(self, symbol):
        h = self.headers[symbol]
        tiles = self.tiles[h["tiles"]]
        tiles = re.sub(r"\.(4bpp|8bpp)(\.\w+)*$", ".png", tiles)
        return tiles, self.palettes[h["palettes"]], self.metatiles[h["metatiles"]]


def load_pal(path):
    lines = read(path).split()
    assert lines[0] == "JASC-PAL", path
    n = int(lines[2])
    vals = list(map(int, lines[3:3 + n * 3]))
    return [tuple(vals[i:i + 3]) for i in range(0, n * 3, 3)]


class Tileset:
    def __init__(self, index, symbol):
        tiles_path, pal_paths, mt_path = index.files(symbol)
        self.symbol = symbol
        img = Image.open(rel(tiles_path))
        if img.mode != "P":
            sys.exit("%s is not an indexed PNG" % tiles_path)
        self.tiles_img = img
        self.num_tiles = (img.width // 8) * (img.height // 8)
        self.palettes = [load_pal(p) for p in pal_paths]
        with open(rel(mt_path), "rb") as f:
            data = f.read()
        self.metatile_words = [int.from_bytes(data[i:i + 2], "little") for i in range(0, len(data), 2)]

    def tile(self, n):
        cols = self.tiles_img.width // 8
        x, y = (n % cols) * 8, (n // cols) * 8
        return self.tiles_img.crop((x, y, x + 8, y + 8))


class MetatileRenderer:
    """Draws a metatile the way the GBA does: backdrop colour, then each layer of 2x2 tiles with colour 0 transparent."""

    def __init__(self, primary, secondary, consts):
        self.p, self.s, self.c = primary, secondary, consts
        self.layers = consts["NUM_TILES_PER_METATILE"] // 4
        self.tile_cache, self.mt_cache = {}, {}
        self.backdrop = primary.palettes[0][0]

    def palette(self, idx):
        src = self.p if idx < self.c["NUM_PALS_IN_PRIMARY"] else self.s
        return src.palettes[idx] if idx < len(src.palettes) else [(255, 0, 255)] * 16

    def tile(self, word):
        if word in self.tile_cache:
            return self.tile_cache[word]
        n, hflip, vflip, pal = word & 0x3FF, (word >> 10) & 1, (word >> 11) & 1, (word >> 12) & 0xF
        src, local = (self.p, n) if n < self.c["NUM_TILES_IN_PRIMARY"] else (self.s, n - self.c["NUM_TILES_IN_PRIMARY"])
        if local >= src.num_tiles:
            img = None  # tile index past the end of the sheet: nothing drawn (matches the hardware's empty VRAM)
        else:
            t = src.tile(local).copy()
            colours = self.palette(pal)
            flat = [v for rgb in colours for v in rgb] + [0] * (768 - len(colours) * 3)
            t.putpalette(flat)
            t.info["transparency"] = 0
            img = t.convert("RGBA")
            if hflip:
                img = img.transpose(Image.FLIP_LEFT_RIGHT)
            if vflip:
                img = img.transpose(Image.FLIP_TOP_BOTTOM)
        self.tile_cache[word] = img
        return img

    def metatile(self, mid):
        if mid in self.mt_cache:
            return self.mt_cache[mid]
        src, local = (self.p, mid) if mid < self.c["NUM_METATILES_IN_PRIMARY"] else (self.s, mid - self.c["NUM_METATILES_IN_PRIMARY"])
        per = self.c["NUM_TILES_PER_METATILE"]
        words = src.metatile_words[local * per:(local + 1) * per]
        img = Image.new("RGBA", (METATILE_PX, METATILE_PX), self.backdrop + (255,))
        if len(words) == per:
            for layer in range(self.layers):
                for i in range(4):
                    t = self.tile(words[layer * 4 + i])
                    if t is not None:
                        img.alpha_composite(t, ((i % 2) * 8, (i // 2) * 8))
        self.mt_cache[mid] = img
        return img


# ---------------------------------------------------------------- map + events

def load_map_by_id(map_id):
    for d in sorted(os.listdir(rel("data/maps"))):
        p = rel(os.path.join("data/maps", d, "map.json"))
        if os.path.exists(p):
            with open(p, encoding="utf-8") as f:
                m = json.load(f)
            if m.get("id") == map_id:
                return d, m
    sys.exit("map %s not found under data/maps" % map_id)


def load_layout(layout_id):
    with open(rel("data/layouts/layouts.json"), encoding="utf-8") as f:
        for l in json.load(f)["layouts"]:
            if l.get("id") == layout_id:
                return l
    sys.exit("layout %s not found" % layout_id)


def render_map_image(layout, index, consts):
    prim = Tileset(index, layout["primary_tileset"])
    sec = Tileset(index, layout["secondary_tileset"])
    mr = MetatileRenderer(prim, sec, consts)
    w, h = layout["width"], layout["height"]
    with open(rel(layout["blockdata_filepath"]), "rb") as f:
        data = f.read()
    img = Image.new("RGBA", (w * METATILE_PX, h * METATILE_PX))
    mask = consts["MAPGRID_METATILE_ID_MASK"]
    for y in range(h):
        for x in range(w):
            i = (y * w + x) * 2
            mid = int.from_bytes(data[i:i + 2], "little") & mask
            img.paste(mr.metatile(mid), (x * METATILE_PX, y * METATILE_PX))
    return img


def pretty_const(c, prefix):
    return " ".join(w.capitalize() for w in c[len(prefix):].split("_")) if c.startswith(prefix) else c


class Content:
    """Lazy-loaded game text: trainers.party, script blocks, item names."""

    def __init__(self):
        self.items = cx.load_items()
        self.trainers = {t["id"]: t for t in cx.parse_party()}
        self.texts, self.blocks = cx.parse_inc_files()

    def sign_text(self, label):
        """Text a sign shows: the first msgbox/message in its script (one hop through goto/call)."""
        todo, seen = [label], set()
        while todo:
            cur = todo.pop(0)
            if cur in seen or cur not in self.blocks:
                continue
            seen.add(cur)
            for line in self.blocks[cur][1]:
                cmd, args = cx.args_of(line)
                if cmd in ("msgbox", "message") and args and args[0] in self.texts:
                    text = self.texts[args[0]]
                    for token, ch in (("UP", "↑"), ("DOWN", "↓"), ("LEFT", "←"), ("RIGHT", "→")):
                        text = text.replace("{%s_ARROW}" % token, ch)
                    return text
                if cmd in ("goto", "call") and args and len(seen) < 3:
                    todo.append(args[0])
        return None

    def trainer_of_script(self, label, depth=0):
        block = self.blocks.get(label)
        if not block:
            return None
        for line in block[1]:
            cmd, args = cx.args_of(line)
            if cmd.startswith("trainerbattle"):
                for a in args:
                    if re.fullmatch(r"TRAINER_\w+", a) and not a.startswith("TRAINER_BATTLE_"):
                        return a
        if depth < 1:  # one hop through goto/call, for scripts that branch first
            for line in block[1]:
                cmd, args = cx.args_of(line)
                if cmd in ("goto", "call") and args:
                    t = self.trainer_of_script(args[0], depth + 1)
                    if t:
                        return t
        return None

    def trainer_summary(self, tid):
        t = self.trainers.get(tid)
        if not t:
            return tid
        name = t["fields"].get("Name", tid)
        cls = t["fields"].get("Class", "")
        team = []
        for mon in t["mons"]:
            species = cx.parse_mon_header(mon["header"])[0]
            lvl = mon["fields"].get("Level", "?")
            team.append("%s Lv %s" % (species, lvl))
        return "%s (%s) – %s" % (name, cls, ", ".join(team)) if cls else "%s – %s" % (name, ", ".join(team))

    def item_name(self, const):
        return self.items.get(const, pretty_const(const, "ITEM_"))


def facing_dirs(movement):
    """Directions a trainer looks in for a fixed-facing movement type; None if it turns/wanders (draw a ring)."""
    m = movement[len("MOVEMENT_TYPE_"):]
    if m.startswith("FACE_") or "IN_PLACE" in m or re.fullmatch(r"WALK_(UP|DOWN|LEFT|RIGHT)_AND_(UP|DOWN|LEFT|RIGHT)", m):
        dirs = [d for d in DIRS if re.search(r"(^|_)%s(_|$)" % d, m)]
        return dirs or None
    return None


def collect_markers(m, content):
    markers = {"T": [], "I": [], "N": [], "B": [], "W": [], "S": []}
    for b in m.get("bg_events", []):
        if b.get("type") == "sign":
            script = b.get("script", "")
            text = content.sign_text(script)
            if not text:  # picture signs (Poké Mart / Pokémon Center) have no text: name them from the script label
                name = re.sub(r"^.*EventScript_(Show)?", "", script) or "no script"
                text = "(%s)" % re.sub(r"(?<=[a-z])(?=[A-Z])", " ", name).capitalize()
            else:
                text = '"%s"' % text
            markers["S"].append({"x": b["x"], "y": b["y"], "text": text})
    for w in m.get("warp_events", []):
        dest = re.sub(r"(\D)(\d)", r"\1 \2", pretty_const(w.get("dest_map", ""), "MAP_"))
        markers["W"].append({"x": w["x"], "y": w["y"], "text": "to %s (warp %s)" % (dest, w.get("dest_warp_id", "?"))})
    for o in m.get("object_events", []):
        gfx, x, y = o.get("graphics_id", ""), o.get("x"), o.get("y")
        if o.get("type") == "clone" or x is None:
            continue
        if gfx == "OBJ_EVENT_GFX_BERRY_TREE":
            tree = str(o.get("trainer_sight_or_berry_tree_id", ""))
            berry = re.sub(r"^BERRY_TREE_\w+?_(?=[A-Z]+(_\d+)?$)", "", tree)
            berry = re.sub(r"_\d+$", "", berry)
            label = "Empty soil" if berry == "SOIL" else "%s Berry" % berry.capitalize()
            markers["B"].append({"x": x, "y": y, "text": "%s (%s)" % (label, tree)})
        elif gfx == "OBJ_EVENT_GFX_ITEM_BALL":
            item = str(o.get("trainer_sight_or_berry_tree_id", ""))
            markers["I"].append({"x": x, "y": y, "text": content.item_name(item), "hidden": False})
        elif o.get("trainer_type", "TRAINER_TYPE_NONE") != "TRAINER_TYPE_NONE":
            tid = content.trainer_of_script(o.get("script", ""))
            sight = int(str(o.get("trainer_sight_or_berry_tree_id", "0")), 0) if str(o.get("trainer_sight_or_berry_tree_id", "0")).isdigit() else 0
            markers["T"].append({"x": x, "y": y, "text": content.trainer_summary(tid) if tid else o.get("script", "?"),
                                 "sight": sight, "dirs": facing_dirs(o.get("movement_type", ""))})
        else:
            sprite = pretty_const(gfx, "OBJ_EVENT_GFX_")
            lid = str(o.get("local_id", ""))
            if gfx.startswith("OBJ_EVENT_GFX_VAR_") and lid:  # sprite chosen at runtime (e.g. the rival): use the object's name
                sprite = pretty_const(re.sub(r"^LOCALID_%s_" % m["name"].upper(), "LOCALID_", lid), "LOCALID_")
            script = o.get("script", "")
            script = "no script" if script in ("0x0", "0", "NULL", "") else script
            markers["N"].append({"x": x, "y": y, "text": "%s (%s)" % (sprite, script)})
    for b in m.get("bg_events", []):
        if b.get("type") == "hidden_item":
            markers["I"].append({"x": b["x"], "y": b["y"], "text": "%s (hidden)" % content.item_name(b.get("item", "")),
                                 "hidden": True})
    for k, lst in markers.items():
        for n, mk in enumerate(lst, 1):
            mk["id"] = "%s%d" % (k, n)
    return markers


# ---------------------------------------------------------------- drawing

def git_info():
    def g(*args):
        try:
            return subprocess.check_output(["git", "-c", "safe.directory=*"] + list(args), cwd=ROOT,
                                           stderr=subprocess.DEVNULL, text=True).strip()
        except (OSError, subprocess.CalledProcessError):
            return "unknown"
    return g("rev-parse", "--short", "HEAD"), datetime.date.today().isoformat()


def wrap(draw, text, fnt, width):
    words, lines, cur = text.split(" "), [], ""
    for w in words:
        trial = (cur + " " + w).strip()
        if draw.textlength(trial, font=fnt) <= width or not cur:
            cur = trial
        else:
            lines.append(cur)
            cur = "   " + w
    lines.append(cur)
    return lines


def compose(area, map_id, map_img, markers, commit, date):
    margin_l, margin_t, title_h, legend_w, pad = 30, 22, 48, 520, 12
    mw, mh = map_img.size
    tiles_w, tiles_h = mw // METATILE_PX, mh // METATILE_PX

    # overlays drawn on the map itself
    over = Image.new("RGBA", map_img.size, (0, 0, 0, 0))
    od = ImageDraw.Draw(over)
    for t in markers["T"]:
        if t["sight"] <= 0:
            continue
        if t["dirs"]:
            for d in t["dirs"]:
                dx, dy = DIRS[d]
                for s in range(1, t["sight"] + 1):
                    tx, ty = t["x"] + dx * s, t["y"] + dy * s
                    if 0 <= tx < tiles_w and 0 <= ty < tiles_h:
                        od.rectangle([tx * 16, ty * 16, tx * 16 + 15, ty * 16 + 15], fill=SIGHT_RGBA)
    for x in range(tiles_w + 1):
        od.line([(x * 16, 0), (x * 16, mh)], fill=(0, 0, 0, 110 if x % 10 == 0 else 28), width=1)
    for y in range(tiles_h + 1):
        od.line([(0, y * 16), (mw, y * 16)], fill=(0, 0, 0, 110 if y % 10 == 0 else 28), width=1)
    base = map_img.copy()
    base.alpha_composite(over)

    md = ImageDraw.Draw(base)
    mark_fonts = [font(s, bold=True) for s in (9, 8, 7, 6)]

    def fit(label):  # largest font that keeps the label inside its own tile, so neighbouring markers don't overlap
        for f in mark_fonts:
            if md.textlength(label, font=f) <= 14:
                return f
        return mark_fonts[-1]
    for mk in markers["W"]:  # frame around the warp tile; stays visible if another marker sits on the same tile
        x0, y0 = mk["x"] * 16, mk["y"] * 16
        md.rectangle([x0 - 1, y0 - 1, x0 + 16, y0 + 16], outline=COLORS["W"][0] + (255,), width=2)
    for k in ("W", "S", "B", "N", "I", "T"):  # trainers drawn last so they stay on top
        for mk in markers[k]:
            fill, txt = COLORS["IH" if mk.get("hidden") else k]
            cx_, cy_ = mk["x"] * 16 + 8, mk["y"] * 16 + 8
            f_mark = fit(mk["id"])
            w2 = 8
            if k == "T" and mk["sight"] > 0 and not mk["dirs"]:
                md.ellipse([cx_ - 13, cy_ - 13, cx_ + 13, cy_ + 13], outline=(214, 40, 40, 255), width=2)
            md.rounded_rectangle([cx_ - w2, cy_ - 7, cx_ + w2, cy_ + 7], radius=4, fill=fill + (255,),
                                 outline=(255, 255, 255, 255), width=1)
            md.text((cx_, cy_), mk["id"], font=f_mark, fill=txt, anchor="mm")

    # legend lines
    f_leg, f_head = font(12), font(13, bold=True)
    tmp = ImageDraw.Draw(Image.new("RGB", (1, 1)))
    sections = [("Trainers", "T"), ("Items", "I"), ("NPCs", "N"), ("Berry trees", "B"), ("Warps (entrances/exits)", "W"), ("Signs", "S")]
    legend = []  # (kind, text, colour)
    for head, k in sections:
        if not markers[k]:
            continue
        legend.append(("head", head, None))
        for mk in markers[k]:
            colour = COLORS["IH" if mk.get("hidden") else k][0]
            extra = ""
            if k == "T":
                extra = "" if mk["sight"] <= 0 else ("  [sight %d %s]" % (mk["sight"], "/".join(d.lower() for d in mk["dirs"]))
                                                     if mk["dirs"] else "  [sight %d, turns]" % mk["sight"])
            for i, line in enumerate(wrap(tmp, "%s  %s%s  (%d,%d)" % (mk["id"], mk["text"], extra, mk["x"], mk["y"]),
                                          f_leg, legend_w - 2 * pad - 14)):
                legend.append(("line" if i == 0 else "cont", line, colour))
        legend.append(("gap", "", None))
    legend_h = sum(20 if k == "head" else 8 if k == "gap" else 15 for k, _, _ in legend) + 2 * pad

    W = margin_l + mw + 8 + legend_w
    H = title_h + max(margin_t + mh + 8, legend_h)
    img = Image.new("RGB", (W, H), (250, 250, 247))
    d = ImageDraw.Draw(img)
    d.text((pad, 8), "%s  ·  %s" % (area, map_id), font=font(18, bold=True), fill=(20, 20, 20))
    d.text((pad, 30), "Rendered from commit %s on %s  ·  1 tile = 16 px  ·  X/Y match the content sheet" % (commit, date),
           font=font(11), fill=(90, 90, 90))
    ox, oy = margin_l, title_h + margin_t
    img.paste(base.convert("RGB"), (ox, oy))
    f_ax = font(10)
    for x in range(0, tiles_w, 10):
        d.text((ox + x * 16 + 2, oy - 4), str(x), font=f_ax, fill=(60, 60, 60), anchor="ld")
    for y in range(0, tiles_h, 10):
        d.text((ox - 4, oy + y * 16 + 1), str(y), font=f_ax, fill=(60, 60, 60), anchor="ra")

    lx, ly = ox + mw + 8 + pad, title_h + pad
    for kind, text, colour in legend:
        if kind == "head":
            d.text((lx, ly), text, font=f_head, fill=(20, 20, 20))
            ly += 20
        elif kind == "gap":
            ly += 8
        else:
            if kind == "line":
                d.rectangle([lx, ly + 2, lx + 9, ly + 11], fill=colour)
            d.text((lx + 14, ly), text, font=f_leg, fill=(30, 30, 30))
            ly += 15
    return img


def render_area(area, index, consts, content, commit, date):
    _, m = load_map_by_id(area["map"])
    layout = load_layout(m["layout"])
    map_img = render_map_image(layout, index, consts)
    markers = collect_markers(m, content)
    img = compose(area["name"], area["map"], map_img, markers, commit, date)
    os.makedirs(OUT_DIR, exist_ok=True)
    path = os.path.join(OUT_DIR, area["name"].replace(" ", "_") + ".png")
    img.save(path, optimize=True)
    return path, markers


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--area", help='render one area by name, e.g. "Route 104"')
    ap.add_argument("--upload", action="store_true", help="upload each PNG to its Notion page (needs NOTION_TOKEN)")
    opts = ap.parse_args()

    with open(os.path.join(HERE, "areas.json"), encoding="utf-8") as f:
        areas = json.load(f)["areas"]
    if opts.area:
        areas = [a for a in areas if a["name"].lower() == opts.area.lower()]
        if not areas:
            sys.exit("no area named %r in areas.json" % opts.area)

    consts, index, content = fieldmap_constants(), TilesetIndex(), Content()
    commit, date = git_info()
    uploader = None
    if opts.upload:
        import notion_upload  # noqa: E402
        uploader = notion_upload.Uploader()
    for a in areas:
        path, markers = render_area(a, index, consts, content, commit, date)
        counts = ", ".join("%s %d" % (k, len(v)) for k, v in markers.items())
        print("%-16s -> %s  (%s)" % (a["name"], os.path.relpath(path, ROOT), counts))
        if uploader:
            uploader.publish(a["notion_page"], path, commit, date)
    return 0


if __name__ == "__main__":
    sys.exit(main())
