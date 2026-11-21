"""Descarga la última CEF estable (distribución "standard", windows64) desde cef-builds.spotifycdn.com.

Uso: python fetch-cef.py <carpeta destino, p. ej. .cef>
Deja la distribución en <destino>\\<nombre> y escribe su ruta en <destino>\\current.txt.
"""
import hashlib, json, os, sys, tarfile, urllib.request

BASE = "https://cef-builds.spotifycdn.com/"
PLATFORM = "windows64"


def main():
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    with urllib.request.urlopen(BASE + "index.json") as r:
        versions = json.load(r)[PLATFORM]["versions"]
    stable = next(v for v in versions if v["channel"] == "stable")  # el índice va de más nueva a más vieja
    f = next(f for f in stable["files"] if f["type"] == "standard")
    name = f["name"].removesuffix(".tar.bz2")
    dest = os.path.join(out, name)
    if not os.path.isdir(dest):
        tmp = os.path.join(out, f["name"])
        print("Descargando", BASE + f["name"], f"({f['size'] >> 20} MB)", flush=True)
        h = hashlib.sha1()
        with urllib.request.urlopen(BASE + urllib.request.quote(f["name"])) as r, open(tmp, "wb") as w:
            while chunk := r.read(1 << 20):
                h.update(chunk)
                w.write(chunk)
        if h.hexdigest() != f["sha1"]:
            os.remove(tmp)
            raise SystemExit("SHA-1 incorrecto: " + f["name"])
        print("Descomprimiendo...", flush=True)
        with tarfile.open(tmp, "r:bz2") as t:
            t.extractall(out, filter="data")
        os.remove(tmp)
    with open(os.path.join(out, "current.txt"), "w") as w:
        w.write(name)
    print("CEF listo en", dest)


if __name__ == "__main__":
    main()
