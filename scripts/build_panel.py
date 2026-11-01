"""Python port of web/build.js (without terser): builds the gzipped panel, its side files and web.h.

python -I build_panel.py <firmware-src dir>
Writes web/out/* (publish to the site's panel/ folder) and smalltv-pomodoro/web.h.
"""
import base64, gzip, hashlib, io, json, os, re, sys

root = os.path.abspath(sys.argv[1])
web = os.path.join(root, "web")
out = os.path.join(web, "out")
os.makedirs(out, exist_ok=True)


def gz(data: bytes) -> bytes:
    buf = io.BytesIO()
    with gzip.GzipFile(fileobj=buf, mode="wb", compresslevel=9, mtime=0) as f:
        f.write(data)
    g = bytearray(buf.getvalue())
    g[9] = 3  # OS byte as Node writes it (cosmetic)
    return bytes(g)


def sri(alg, data):
    return f"{alg}-" + base64.b64encode(hashlib.new(alg, data).digest()).decode()


html = open(os.path.join(web, "index.html"), encoding="utf-8").read()

d = open(os.path.join(web, "i18n.json"), "rb").read()
ver = hashlib.sha1(d).hexdigest()[:10]
html = html.replace("__I18V__", ver).replace("__I18I__", sri("sha384", d))
open(os.path.join(out, f"i18n-{ver}.json"), "wb").write(d)

tz = open(os.path.join(web, "tz.json"), "rb").read()
tzv = hashlib.sha1(tz).hexdigest()[:10]
html = html.replace("__TZV__", tzv).replace("__TZI__", sri("sha384", tz))
open(os.path.join(out, f"tz-{tzv}.json"), "wb").write(tz)

share = open(os.path.join(web, "share.js"), "rb").read()
shv = hashlib.sha1(share).hexdigest()[:10]
html = html.replace("__SHV__", shv)
open(os.path.join(out, f"share-{shv}.js"), "wb").write(share)
html = html.replace("__SHI__", sri("sha384", share))

html = re.sub(r"<!--[\s\S]*?-->", "", html)
lines = [l.strip() for l in html.split("\n")]
mini = "\n".join(l for l in lines if l and not l.startswith("//")).encode()
g = gz(mini)
h = hashlib.sha256(g)
phash = h.hexdigest()[:24]
psri = "sha256-" + base64.b64encode(h.digest()).decode()
pfile = f"panel-{phash}.html.gz"
open(os.path.join(out, pfile), "wb").write(g)

loader = open(os.path.join(web, "loader.html"), encoding="utf-8").read()
loader = re.sub(r"<!--[\s\S]*?-->", "", loader).replace("__PANEL_FILE__", pfile).replace("__PANEL_SRI__", psri)
loader = "\n".join(l.strip() for l in loader.split("\n") if l.strip()).encode()
lg = gz(loader)

hdr = "// Gerado por tools/build_panel.py (weatherbox) a partir de web/index.html e web/loader.html — não edite à mão.\n"
hdr += "#pragma once\n#include <pgmspace.h>\n\n"
hdr += f"// painel desta versão: {pfile} ({len(g)} bytes), SRI {psri}\n"
hdr += f"// conteúdo do painel antes do gzip: sha256 {hashlib.sha256(mini).hexdigest()}\n"
hdr += f'#define PANEL_HASH "{phash}"  // 24 primeiros hex do SHA-256: nome do arquivo em /www\n'
hdr += f"static const uint8_t LOADER_HTML_GZ[{len(lg)}] PROGMEM = {{"
for i, b in enumerate(lg):
    hdr += ("" if i % 24 else "\n  ") + f"0x{b:02x},"
hdr += "\n};\n"
open(os.path.join(root, "smalltv-pomodoro", "web.h"), "w", encoding="utf-8").write(hdr)
json.dump({"panel": pfile, "psri": psri, "psize": len(g),
           "site": [pfile, f"i18n-{ver}.json", f"tz-{tzv}.json", f"share-{shv}.js"]},
          open(os.path.join(out, "panel.json"), "w"))
print(f"panel {len(mini)/1024:.1f} KB -> {len(g)/1024:.1f} KB gz: {pfile}; loader {len(lg)} bytes")
