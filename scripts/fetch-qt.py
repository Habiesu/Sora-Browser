"""Descarga Qt (msvc2022_64) con WebEngine, WebChannel y Positioning desde download.qt.io.

Sustituye a aqtinstall, que (3.3.0) no entiende la estructura del repositorio de Qt 6.12
(una carpeta por arquitectura y WebEngine como "extension" aparte).

Uso: python fetch-qt.py 6.12.0 <carpeta destino, p. ej. .qt\\6.12.0\\msvc2022_64>
Requiere py7zr (pip install py7zr).
"""
import hashlib, io, os, re, sys, urllib.request, xml.etree.ElementTree as ET

import py7zr

ARCH = "win64_msvc2022_64"
BASE = "https://download.qt.io/online/qtsdkrepository/windows_x86/"
SKIP = ("qtdoc-",)


def get(url):
    with urllib.request.urlopen(url) as r:
        return r.read()


def updates(repo):
    root = ET.fromstring(get(repo + "Updates.xml"))
    return {p.findtext("Name"): p for p in root.iter("PackageUpdate")}


def newest_webengine_repo(tag):
    # Qt vuelve a publicar WebEngine en subcarpetas numeradas (p. ej. 6120/61400/) cuando
    # recompila qtbase; la de 6120/msvc2022_64/ puede quedarse con un ABI antiguo
    # (en 6.12.0 importaba QAccessibleInterface::QAccessibleInterface(), que qtbase ya no exporta).
    base = f"{BASE}extensions/qtwebengine/{tag}/"
    repos = [base + "msvc2022_64/"]
    repos += [f"{base}{d}/msvc2022_64/" for d in re.findall(r'href="(\d+)/"', get(base).decode())]
    best = None
    for repo in repos:
        try:
            pkgs = updates(repo)
        except Exception:
            continue
        for name, p in pkgs.items():
            if name.endswith(ARCH) and "debug" not in name:
                key = p.findtext("ReleaseDate") or ""
                if best is None or key > best[0]:
                    best = (key, repo, name)
    if not best:
        raise SystemExit(f"No se encuentra Qt WebEngine para {tag}")
    return best[1], best[2]


def install(repo, name, pkgs, out):
    p = pkgs[name]
    ver = p.findtext("Version")
    archives = [a.strip() for a in (p.findtext("DownloadableArchives") or "").split(",") if a.strip()]
    for arc in archives:
        if arc.startswith(SKIP):
            continue
        url = f"{repo}{name}/{ver}{arc}"
        print("Descargando", url, flush=True)
        data = get(url)
        sha1 = get(url + ".sha1").decode().split()[0]
        if hashlib.sha1(data).hexdigest() != sha1:
            raise SystemExit(f"SHA-1 incorrecto: {url}")
        with py7zr.SevenZipFile(io.BytesIO(data)) as z:
            z.extractall(out)


def main():
    version, out = sys.argv[1], sys.argv[2]
    tag = version.replace(".", "")
    desktop = f"{BASE}desktop/qt6_{tag}/qt6_{tag}_msvc2022_64/"
    pkgs = updates(desktop)
    for name in (f"qt.qt6.{tag}.{ARCH}",
                 f"qt.qt6.{tag}.addons.qtwebchannel.{ARCH}",
                 f"qt.qt6.{tag}.addons.qtpositioning.{ARCH}"):
        install(desktop, name, pkgs, out)
    we_repo, we_name = newest_webengine_repo(tag)
    install(we_repo, we_name, updates(we_repo), out)

    # Estos dos paquetes se extraen en la raíz en lugar de en bin\
    for f in ("d3dcompiler_47.dll", "opengl32sw.dll"):
        if os.path.exists(os.path.join(out, f)):
            os.replace(os.path.join(out, f), os.path.join(out, "bin", f))
    if not os.path.exists(os.path.join(out, "bin", "Qt6WebEngineCore.dll")):
        raise SystemExit("Falta Qt6WebEngineCore.dll tras la descarga")
    print("Qt listo en", out)


if __name__ == "__main__":
    main()
