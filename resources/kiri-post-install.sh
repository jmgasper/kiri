#!/bin/sh
set -eu

# Refresh an older per-user MIME record after a package upgrade.
KIRI_EXECUTABLE=$(findpaths -p "$0" B_FIND_PATH_APPS_DIRECTORY Kiri)
mimeset -a -f "$KIRI_EXECUTABLE"
