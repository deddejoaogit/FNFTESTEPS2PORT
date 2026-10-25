#!/usr/bin/env python3
"""Check that every asset the port reads at runtime is on disk, and that its
size matches the size Google Drive reports.

    ./check_assets.py assets                    # check one directory
    ./check_assets.py assets ../FNFPS2-assets   # check several, in order
    ./check_assets.py --missing-only            # just list what is absent

The sizes come from assets-manifest.json, so a truncated download -- which is
the usual way a pendrive copy goes wrong -- is caught here rather than as a
crash on the console. Nothing is modified.

More than one directory is accepted because the assets are the same for every
version of the port: they are converted textures and audio, not code. Keeping
them once in a shared folder and pointing every version at it means a 65 MB
download happens once, not once per package. The first directory is where a
missing file is told to go; the rest are only searched.
"""
import argparse
import json
import os
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
MANIFEST = os.path.normpath(os.path.join(HERE, "..", "assets-manifest.json"))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("directory", nargs="*", default=["assets"],
                    help="directories to look in, most specific first")
    ap.add_argument("--missing-only", action="store_true")
    ap.add_argument("--json", action="store_true", dest="as_json")
    ap.add_argument("--links", action="store_true",
                    help="write assets-faltando.html with one download link per "
                         "missing asset, so a browser that is already signed in "
                         "can fetch them without copying ids by hand")
    ap.add_argument("--stage-into", metavar="DIR",
                    help="copy every correctly sized asset into DIR, creating "
                         "it. Only files the manifest lists as required are "
                         "staged, so repository bookkeeping such as .gitkeep "
                         "and LEIA-ME.txt never reaches the memory card. A "
                         "directory earlier on the command line wins, so the "
                         "project's own assets/ overrides the shared copy")
    args = ap.parse_args()

    dirs = args.directory or ["assets"]
    primary = dirs[0]

    with open(MANIFEST) as fh:
        man = json.load(fh)

    assets = {n: m for n, m in man["assets"].items() if m.get("required")}
    required = len(assets)
    missing, wrong_size, ok = [], [], []

    for name, meta in sorted(assets.items()):
        hit = None
        for d in dirs:
            path = os.path.join(d, name)
            if not os.path.isfile(path):
                continue
            got = os.path.getsize(path)
            if got == meta["bytes"]:
                hit = path
                break
            # Right name, wrong size: remember it and keep looking, in case
            # another directory holds the complete copy. A truncated file in
            # the project's own assets/ must not hide a good one in the shared
            # folder -- that is the whole point of searching more than one.
            if hit is None:
                hit = ("wrong", path, got)
        if isinstance(hit, str):
            ok.append(name)
        elif hit is not None:
            wrong_size.append((name, meta, hit[2], hit[1]))
        else:
            missing.append((name, meta))

    if args.stage_into:
        return _stage(dirs, assets, args.stage_into)

    if args.as_json:
        print(json.dumps(
            {"required": required, "present": ok, "missing": [m[0] for m in missing],
             "wrong_size": [w[0] for w in wrong_size]},
            indent=2))
        return 0

    # A page with one link per missing asset. Drive will not hand a file to an
    # unauthenticated wget, and the browser is already signed in, so a page of
    # links is the reliable way to get them -- no ids to copy by hand.
    if args.links and (missing or wrong_size):
        _write_links(man, missing, wrong_size, primary)

    if not args.missing_only:
        print("%d of %d required assets present and correctly sized" % (len(ok), required))
        for name in ok:
            print("  ok      %s" % name)

    if wrong_size:
        print()
        print("%d asset(s) are the wrong size -- the download was interrupted:" % len(wrong_size))
        for name, meta, got, path in wrong_size:
            print("  %s" % name)
            print("      expected %d bytes, found %d in %s (Drive id %s)"
                  % (meta["bytes"], got, path, meta["drive_id"]))

    if missing:
        print()
        print("%d required asset(s) are missing:" % len(missing))
        for name, meta in missing:
            print("  %s  (%d bytes)" % (name, meta["bytes"]))
            print("      Drive file id: %s" % meta["drive_id"])
        print()
        print("Download them from %s" % man["drive_folder_url"])
        print("and put them in %s/" % primary.rstrip("/"))
        if len(dirs) > 1:
            print()
            print("(also searched: %s)" % ", ".join(dirs[1:]))

    if missing or wrong_size:
        return 1
    if args.missing_only:
        print("all %d required assets are present and correctly sized" % required)
    return 0




def _stage(dirs, assets, dest):
    """Copy every correctly sized asset into dest.

    Driven by the manifest rather than by what happens to be in the directory,
    for two reasons. The obvious one is that a stray file should not end up on
    the memory card -- .gitkeep and LEIA-ME.txt were being copied there. The
    less obvious one is precedence: a truncated file in the project's assets/
    must not shadow a complete copy in the shared folder, and comparing against
    the expected size is the only way to tell them apart.
    """
    os.makedirs(dest, exist_ok=True)
    copied, skipped = [], []
    for name, meta in sorted(assets.items()):
        target = os.path.join(dest, name)
        if os.path.isfile(target) and os.path.getsize(target) == meta["bytes"]:
            continue
        for d in dirs:
            src = os.path.join(d, name)
            if os.path.isfile(src) and os.path.getsize(src) == meta["bytes"]:
                os.makedirs(os.path.dirname(target) or dest, exist_ok=True)
                shutil.copyfile(src, target)
                copied.append(name)
                break
        else:
            skipped.append(name)
    print("staged %d asset(s) into %s" % (len(copied), dest))
    if skipped:
        print("  not staged (missing or wrong size): %s" % ", ".join(skipped))
    return 0


def _write_links(man, missing, wrong_size, primary):
    """Write assets-faltando.html: one clickable link per asset still needed."""
    rows = []
    for name, meta in missing:
        rows.append((name, meta["bytes"], "faltando",
                     "https://drive.google.com/file/d/%s/view" % meta["drive_id"]))
    for name, meta, got, path in wrong_size:
        rows.append((name, meta["bytes"], "tamanho errado (%d B)" % got,
                     "https://drive.google.com/file/d/%s/view" % meta["drive_id"]))

    out = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                       "..", "assets-faltando.html")
    out = os.path.normpath(out)
    with open(out, "w", encoding="utf-8") as fh:
        fh.write("""<!doctype html>
<meta charset="utf-8">
<title>Assets que faltam</title>
<style>
 body { font: 15px/1.5 system-ui, sans-serif; max-width: 60em; margin: 2em auto; padding: 0 1em; }
 h1 { font-size: 1.3em; }
 p  { color: #555; }
 li { margin: .5em 0; }
 a  { color: #06c; }
 .sz { color: #888; font-size: .85em; }
 .bad { color: #a60; }
</style>
<h1>Assets que faltam</h1>
<p>Clique em cada link, baixe o arquivo, e mova todos para a pasta
<code>%s/</code> ao lado desta pagina. Depois rode
<code>./build.sh --check-assets</code> de novo &mdash; ele confere o tamanho
byte a byte, ent&atilde;o um download pela metade &eacute; pego.</p>
<ol>
""" % primary.rstrip("/"))
        for name, size, why, url in rows:
            fh.write('<li><a href="%s">%s</a> <span class="sz">%s &middot; '
                     '%d bytes</span>%s</li>\n'
                     % (url, name, why, size,
                        ' <span class="bad">&larr; baixar de novo, veio '
                        'incompleto</span>' if why != "faltando" else ""))
        fh.write('</ol>\n<p>Ou a pasta inteira, se preferir: '
                 '<a href="%s">%s</a></p>\n'
                 % (man["drive_folder_url"], man["drive_folder_url"]))
    print("\nLinks escritos em: assets-faltando.html")


if __name__ == "__main__":
    sys.exit(main())