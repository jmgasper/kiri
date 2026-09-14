# Kiri 0.0.1 alpha

The first public release of Kiri, a native C++ project editor for Haiku with
source editing, Git and tabbed terminals.

## Install

Download `kiri-0.0.1~alpha-1-x86_64.hpkg` from the release assets and run on
Haiku R1/beta5 x86_64:

```sh
pkgman install ./kiri-0.0.1~alpha-1-x86_64.hpkg
/boot/system/apps/Kiri
```

The package declares its Scintilla, Lexilla and Git dependencies. Kiri appears
in Deskbar's Applications menu and in Tracker's **Open with** menu for folders.

## Included

- A launcher with recent folders and files, plus session restoration and draft recovery.
- Source editing with syntax highlighting, search, preferences and ten themes.
- Independent terminal tabs with **Close all** and **Close others** tab menus.
- Git status, staging, commits, history, diffs and GitHub line permalinks.
- Prettier formatting, language-server completion and current-file symbols.
- Image and binary previews, and the native blue and mint Kiri icon.

Formatting and language servers are optional external tools. The setup commands
and supported configurations are in
[the language tools guide](https://github.com/jmgasper/kiri/blob/v0.0.1-alpha/docs/LANGUAGE_TOOLS.md).

## Build and verification

The Haiku executable and package are built natively with GCC 13.3 on Haiku
R1/beta5 x86_64. The source archive includes the build scripts, tests, vendored
dependencies and editable icon sources. `SHA256SUMS` covers the downloadable
package, executable, debug executable and source archive.

Fresh builds passed 163 core checks on Linux and Haiku, 233 native
editor/file/worker checks, and 98 language checks on Linux / 108 on Haiku with
real Prettier and language servers. The launcher and folder-opening check also
passed. The package's version resources, Tracker metadata and icon were checked. See
[the verification record](https://github.com/jmgasper/kiri/blob/v0.0.1-alpha/docs/STATUS.md)
and [current limits](https://github.com/jmgasper/kiri/blob/v0.0.1-alpha/README.md#files-recovery-and-limits).

Earlier beta labels in the verification history describe local development
builds. The public version series starts with **0.0.1 alpha**.
