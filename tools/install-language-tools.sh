#!/usr/bin/env bash
set -euo pipefail

# Installs JavaScript tools in Kiri's own directory, without global npm changes.
KIRI_LANGUAGE_SETTINGS=${1:-/boot/home/config/settings/Kiri}
if ! command -v node >/dev/null || ! command -v npm >/dev/null; then
    printf '%s\n' 'Install Node.js and npm first: pkgman install nodejs20 npm' >&2
    exit 1
fi
npm install --prefix "$KIRI_LANGUAGE_SETTINGS/tools" --no-audit --no-fund \
    prettier@3.9.6 typescript-language-server@4.3.4 typescript@5.9.3 \
    vscode-langservers-extracted@4.10.0
printf '%s\n' 'Installed Prettier and JavaScript, TypeScript, HTML, CSS and JSON language servers.' \
    'In Kiri, choose Edit → Language Tools → Apply to restart language servers.'
