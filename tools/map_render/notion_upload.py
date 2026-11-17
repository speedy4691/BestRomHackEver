"""Upload a rendered map PNG to an area's Notion page.

On the page, the map lives in a "Map" heading placed directly above the "Live game data" heading:

    ## Map
    [image]
    Rendered from commit <hash> on <date>. Markers match ...

The first run inserts those three blocks. Later runs swap the image and update the caption line in place, so
re-running never adds a second image. Nothing else on the page is touched.

The token comes from the NOTION_TOKEN environment variable, or from a git-ignored .env file at the repo root.
It is never printed.
"""

import json
import os
import time
import urllib.error
import urllib.request
import uuid

API = "https://api.notion.com/v1"
NOTION_VERSION = "2022-06-28"
MAP_HEADING = "Map"
ANCHOR_HEADING = "Live game data"
CAPTION_PREFIX = "Rendered from commit"
HEADINGS = ("heading_1", "heading_2", "heading_3")

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))


def load_token():
    token = os.environ.get("NOTION_TOKEN", "").strip()
    env_path = os.path.join(ROOT, ".env")
    if not token and os.path.exists(env_path):
        with open(env_path, encoding="utf-8") as f:
            for line in f:
                key, sep, value = line.strip().partition("=")
                if sep and key.strip().upper() == "NOTION_TOKEN":
                    token = value.strip().strip('"').strip("'")
    if not token:
        raise SystemExit("NOTION_TOKEN is not set (environment variable or .env at the repo root).")
    return token


def plain(block):
    data = block.get(block["type"], {})
    return "".join(t.get("plain_text", "") for t in data.get("rich_text", [])).strip()


class NotionError(Exception):
    def __init__(self, status, body):
        super().__init__("Notion API %s: %s" % (status, body))
        self.status = status


class Uploader:
    def __init__(self):
        self.token = load_token()

    # ------------------------------------------------------------ HTTP
    def _request(self, method, path, payload=None, data=None, content_type=None):
        url = path if path.startswith("http") else API + path
        headers = {"Authorization": "Bearer " + self.token, "Notion-Version": NOTION_VERSION}
        if payload is not None:
            data = json.dumps(payload).encode("utf-8")
            content_type = "application/json"
        if content_type:
            headers["Content-Type"] = content_type
        for attempt in range(5):
            req = urllib.request.Request(url, data=data, method=method, headers=headers)
            try:
                with urllib.request.urlopen(req, timeout=60) as r:
                    body = r.read()
                    return json.loads(body) if body else {}
            except urllib.error.HTTPError as e:
                body = e.read().decode("utf-8", "replace")
                if e.code in (429, 502, 503) and attempt < 4:  # rate limited / transient: back off and retry
                    time.sleep(float(e.headers.get("Retry-After", 2 ** attempt)))
                    continue
                raise NotionError(e.code, body) from None

    def children(self, block_id):
        out, cursor = [], None
        while True:
            q = "?page_size=100" + ("&start_cursor=" + cursor if cursor else "")
            r = self._request("GET", "/blocks/%s/children%s" % (block_id, q))
            out.extend(r["results"])
            if not r.get("has_more"):
                return out
            cursor = r["next_cursor"]

    def upload_file(self, path):
        name = os.path.basename(path)
        fu = self._request("POST", "/file_uploads", {"filename": name, "content_type": "image/png"})
        boundary = uuid.uuid4().hex
        with open(path, "rb") as f:
            content = f.read()
        body = (("--%s\r\nContent-Disposition: form-data; name=\"file\"; filename=\"%s\"\r\n"
                 "Content-Type: image/png\r\n\r\n" % (boundary, name)).encode("utf-8")
                + content + ("\r\n--%s--\r\n" % boundary).encode("utf-8"))
        self._request("POST", "/file_uploads/%s/send" % fu["id"], data=body,
                      content_type="multipart/form-data; boundary=" + boundary)
        return fu["id"]

    # ------------------------------------------------------------ page edits
    @staticmethod
    def image_block(upload_id):
        return {"object": "block", "type": "image",
                "image": {"type": "file_upload", "file_upload": {"id": upload_id}}}

    @staticmethod
    def caption_block(text):
        return {"object": "block", "type": "paragraph",
                "paragraph": {"rich_text": [{"type": "text", "text": {"content": text}}]}}

    def insert_after(self, page_id, after_id, blocks):
        return self._request("PATCH", "/blocks/%s/children" % page_id, {"children": blocks, "after": after_id})["results"]

    def publish(self, page_id, png_path, commit, date):
        caption = ("Rendered from commit %s on %s. Markers match the trainer, item and NPC rows in the view below."
                   % (commit, date))
        blocks = self.children(page_id)
        maps = [i for i, b in enumerate(blocks) if b["type"] in HEADINGS and plain(b) == MAP_HEADING]
        anchors = [i for i, b in enumerate(blocks) if b["type"] in HEADINGS and plain(b) == ANCHOR_HEADING]
        upload_id = self.upload_file(png_path)

        if maps:  # update in place
            i = maps[0]
            nxt = blocks[i + 1] if i + 1 < len(blocks) else None
            if nxt and nxt["type"] == "image":
                try:
                    self._request("PATCH", "/blocks/%s" % nxt["id"],
                                  {"image": {"type": "file_upload", "file_upload": {"id": upload_id}}})
                    image_id = nxt["id"]
                except NotionError:
                    # This API version can't swap the file on an existing image: put the new image in the same
                    # spot, then delete the old one, so there is never more than one image under the heading.
                    image_id = self.insert_after(page_id, blocks[i]["id"], [self.image_block(upload_id)])[0]["id"]
                    self._request("DELETE", "/blocks/%s" % nxt["id"])
                cap = blocks[i + 2] if i + 2 < len(blocks) else None
            else:
                image_id = self.insert_after(page_id, blocks[i]["id"], [self.image_block(upload_id)])[0]["id"]
                cap = nxt
            if cap and cap["type"] == "paragraph" and plain(cap).startswith(CAPTION_PREFIX):
                self._request("PATCH", "/blocks/%s" % cap["id"], {"paragraph": self.caption_block(caption)["paragraph"]})
            else:
                self.insert_after(page_id, image_id, [self.caption_block(caption)])
            print("   Notion: replaced the map image on page %s" % page_id)
            return

        if not anchors:
            raise SystemExit("Page %s has no %r heading; not changing it." % (page_id, ANCHOR_HEADING))
        a = anchors[0]
        if a == 0:
            raise SystemExit("Page %s: %r is the first block, so nothing can be inserted above it via the API."
                             % (page_id, ANCHOR_HEADING))
        level = blocks[a]["type"]
        heading = {"object": "block", "type": level,
                   level: {"rich_text": [{"type": "text", "text": {"content": MAP_HEADING}}]}}
        self.insert_after(page_id, blocks[a - 1]["id"],
                          [heading, self.image_block(upload_id), self.caption_block(caption)])
        print("   Notion: added the Map section to page %s" % page_id)
