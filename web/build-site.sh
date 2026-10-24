#!/usr/bin/env sh
# Assembles the static site: playground at the root, gallery under gallery/.
# Usage: web/build-site.sh <theosc> <out-dir>   (web/dist must already hold the WebAssembly build)
set -eu
THEOSC="$1"
OUT="$2"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

rm -rf "$OUT"
mkdir -p "$OUT/gallery" "$OUT/examples" "$OUT/library"
cp "$ROOT/web/index.html" "$ROOT/web/app.js" "$ROOT/web/theos.css" "$ROOT/web/playground.css" "$OUT/"
cp -r "$ROOT/web/dist" "$OUT/dist"
cp "$ROOT"/examples/*.theo "$OUT/examples/"
cp -r "$ROOT/library/arguments" "$OUT/library/arguments"
cp "$ROOT/web/theos.css" "$ROOT/web/gallery.css" "$OUT/gallery/"
"$THEOSC" site "$ROOT/library/arguments" -o "$OUT/gallery"
