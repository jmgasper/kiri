#!/usr/bin/env bash
set -euo pipefail
KIRI_ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$KIRI_ROOT"
make -j4
KIRI_STAGE=$(mktemp -d /tmp/kiri-package-XXXXXX)
trap 'rm -rf -- "$KIRI_STAGE"' EXIT
mkdir -p "$KIRI_STAGE/apps" "$KIRI_STAGE/documentation/packages/kiri/vendor/libvterm" \
    "$KIRI_STAGE/documentation/packages/kiri/vendor/nlohmann" "$KIRI_STAGE/documentation/packages/kiri/tools" \
    "$KIRI_STAGE/data/deskbar/menu/Applications" "$KIRI_ROOT/artifacts"
cp build-haiku/Kiri "$KIRI_STAGE/apps/Kiri"
strip --strip-debug "$KIRI_STAGE/apps/Kiri"
# GNU strip removes the appended Haiku resources; restore the application
# signature, supported types and version on the release executable.
xres -o "$KIRI_STAGE/apps/Kiri" build-haiku/Kiri.rsrc
cp resources/Kiri.PackageInfo "$KIRI_STAGE/.PackageInfo"
cp README.md LICENSE "$KIRI_STAGE/documentation/packages/kiri/"
cp -R docs "$KIRI_STAGE/documentation/packages/kiri/"
cp vendor/libvterm/LICENSE vendor/libvterm/UPSTREAM.md "$KIRI_STAGE/documentation/packages/kiri/vendor/libvterm/"
cp vendor/nlohmann/LICENSE.MIT vendor/nlohmann/UPSTREAM.md "$KIRI_STAGE/documentation/packages/kiri/vendor/nlohmann/"
cp tools/install-language-tools.sh "$KIRI_STAGE/documentation/packages/kiri/tools/"
ln -s ../../../../apps/Kiri "$KIRI_STAGE/data/deskbar/menu/Applications/Kiri"
package create -C "$KIRI_STAGE" "$KIRI_ROOT/artifacts/kiri-0.1.0~beta6-1-x86_64.hpkg"
printf '%s\n' "$KIRI_ROOT/artifacts/kiri-0.1.0~beta6-1-x86_64.hpkg"
