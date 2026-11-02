#!/bin/bash
# Baut die .deb-Pakete von TANGENTA in Containern (Podman oder Docker) und prüft jedes durch
# eine Installation in einem zweiten, frischen Container.
#
#   packaging/linux/package-linux.sh [ziel ...]     Vorgabe: alle
#   ziele: debian12 debian13 ubuntu24.04 ubuntu26.04
#
# Ergebnis: build/packages/tangenta_<version>-1~<ziel>_amd64.deb
set -euo pipefail

SRC=$(cd "$(dirname "$0")/../.." && pwd)
OUT="$SRC/build/packages"
CACHE=${TANGENTA_CACHE:-$HOME/.cache/tangenta-pkg}
ENGINE=$(command -v podman || command -v docker || true)
[ -n "$ENGINE" ] || { echo "Podman oder Docker fehlt (sudo apt install podman)"; exit 1; }
mkdir -p "$OUT" "$CACHE"

image() {
    case "$1" in
        debian12) echo docker.io/library/debian:12 ;;
        debian13) echo docker.io/library/debian:13 ;;
        ubuntu24.04) echo docker.io/library/ubuntu:24.04 ;;
        ubuntu26.04) echo docker.io/library/ubuntu:26.04 ;;
        *) echo "Unbekanntes Ziel: $1" >&2; exit 1 ;;
    esac
}

TARGETS=("$@")
[ ${#TARGETS[@]} -gt 0 ] || TARGETS=(debian12 debian13 ubuntu24.04 ubuntu26.04)
for t in "${TARGETS[@]}"; do
    img=$(image "$t")
    echo "##### $t ($img)"
    "$ENGINE" run --rm -v "$SRC:/src:ro" -v "$CACHE:/cache" -v "$OUT:/out" "$img" \
        bash /src/packaging/linux/build-deb.sh "$t" 2>&1 | tee "$OUT/build-$t.log"
    deb=$(ls -t "$OUT"/tangenta_*~"${t}"_amd64.deb | head -1)
    echo "##### Probeinstallation $(basename "$deb")"
    "$ENGINE" run --rm -v "$OUT:/out:ro" "$img" bash -c "
        set -e
        export DEBIAN_FRONTEND=noninteractive
        apt-get update -q >/dev/null
        apt-get install -y -q /out/$(basename "$deb") >/dev/null
        test -x /usr/bin/tangenta
        ! ldd /opt/tangenta/bin/tangenta | grep 'not found'
        for p in /opt/tangenta/plugins/platforms/*.so; do ! ldd \$p | grep 'not found'; done
        rc=0; QT_QPA_PLATFORM=offscreen timeout 8 tangenta || rc=\$?
        [ \$rc = 124 ] && echo 'Startprobe: läuft (nach 8 s beendet)' || { echo \"Startprobe: Ende mit Code \$rc\"; exit 1; }
    " 2>&1 | tee "$OUT/install-$t.log"
done
ls -l "$OUT"/*.deb
