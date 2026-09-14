#!/usr/bin/env bash
set -euo pipefail
KIRI_ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$KIRI_ROOT"
KIRI_PACKAGE_NAME=$(awk '$1 == "name" { print $2; exit }' resources/Kiri.PackageInfo)
KIRI_PACKAGE_VERSION=$(awk '$1 == "version" { print $2; exit }' resources/Kiri.PackageInfo)
KIRI_PACKAGE_ARCH=$(awk '$1 == "architecture" { print $2; exit }' resources/Kiri.PackageInfo)
KIRI_PACKAGE_FILE="$KIRI_ROOT/artifacts/$KIRI_PACKAGE_NAME-$KIRI_PACKAGE_VERSION-$KIRI_PACKAGE_ARCH.hpkg"
make -j4
KIRI_STAGE=$(mktemp -d /tmp/kiri-package-XXXXXX)
trap 'rm -rf -- "$KIRI_STAGE"' EXIT
mkdir -p "$KIRI_STAGE/apps" "$KIRI_STAGE/documentation/packages/kiri/vendor/libvterm" \
    "$KIRI_STAGE/documentation/packages/kiri/vendor/nlohmann" "$KIRI_STAGE/documentation/packages/kiri/tools" \
    "$KIRI_STAGE/data/deskbar/menu/Applications" "$KIRI_STAGE/boot/post-install" "$KIRI_ROOT/artifacts"
cp build-haiku/Kiri "$KIRI_STAGE/apps/Kiri"
strip --strip-debug "$KIRI_STAGE/apps/Kiri"
# GNU strip removes the appended Haiku resources; restore the application
# signature, icon, supported types and version on the release executable.
xres -o "$KIRI_STAGE/apps/Kiri" build-haiku/Kiri.rsrc
cp resources/Kiri.PackageInfo "$KIRI_STAGE/.PackageInfo"
cp README.md LICENSE "$KIRI_STAGE/documentation/packages/kiri/"
cp -R docs "$KIRI_STAGE/documentation/packages/kiri/"
cp vendor/libvterm/LICENSE vendor/libvterm/UPSTREAM.md "$KIRI_STAGE/documentation/packages/kiri/vendor/libvterm/"
cp vendor/nlohmann/LICENSE.MIT vendor/nlohmann/UPSTREAM.md "$KIRI_STAGE/documentation/packages/kiri/vendor/nlohmann/"
cp tools/install-language-tools.sh "$KIRI_STAGE/documentation/packages/kiri/tools/"
cp resources/kiri-post-install.sh "$KIRI_STAGE/boot/post-install/kiri.sh"
ln -s ../../../../apps/Kiri "$KIRI_STAGE/data/deskbar/menu/Applications/Kiri"
# Tracker queries file attributes and the MIME database for supported handlers.
# Generate both in the package, as HaikuPorter does, without changing the host DB.
(
    cd "$KIRI_STAGE"
    # Force a complete staged record even when the fallback DB already has Kiri.
    mimeset --all -f --mimedb data/mime_db --mimedb /boot/system/data/mime_db apps/Kiri
)
# Refuse a package with a missing or stale application/MIME icon.
catattr -r BEOS:ICON "$KIRI_STAGE/apps/Kiri" > "$KIRI_STAGE/.icon-check"
cmp resources/branding/kiri-icon.hvif "$KIRI_STAGE/.icon-check"
catattr -r META:ICON "$KIRI_STAGE/data/mime_db/application/x-vnd.kiri-editor" > "$KIRI_STAGE/.icon-check"
cmp resources/branding/kiri-icon.hvif "$KIRI_STAGE/.icon-check"
rm "$KIRI_STAGE/.icon-check"
package create -C "$KIRI_STAGE" "$KIRI_PACKAGE_FILE"
printf '%s\n' "$KIRI_PACKAGE_FILE"
