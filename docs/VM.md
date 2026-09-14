# Native test environment

All VM state is local to this repository in ignored `.vm/` and `.cache/` folders.
It does not use the separate Haiku hardware/ARM64 workspace.

## Existing VM

```sh
bash tools/run-vm.sh
bash tools/haiku.sh 'uname -a'
bash tools/build-in-vm.sh
bash tools/haiku.sh 'open /boot/home/kiri/build-haiku/Kiri'
bash tools/haiku.sh 'hey Kiri let Window 0 do oppr with path=/boot/home/kiri'
python3 tools/vm.py screenshot
```

VNC: `127.0.0.1:5904`. SSH: `127.0.0.1:2224`. QMP: `.vm/qmp.sock`.
The SSH key is dedicated to this VM and is not checked in.

To check UI changes while preserving an open Kiri session, build the separate
workspace harness and give it a dedicated settings directory and test project:

```sh
bash tools/haiku.sh 'cd /boot/home/kiri && make workspace-smoke'
bash tools/haiku.sh 'nohup /boot/home/kiri/build-haiku/kiri_workspace_smoke /boot/home/kiri/ui-test-settings /boot/home/KiriTabsTest >/boot/home/kiri/ui-test.log 2>&1 </dev/null &'
bash tools/haiku.sh 'hey application/x-vnd.Kiri-workspace-tests count View of View "terminal host" of Window 0'
bash tools/haiku.sh 'hey application/x-vnd.Kiri-workspace-tests quit'
```

The harness uses the production workspace code with a different application
signature. An optional third argument after the project sets another signature
(for example `application/x-vnd.Kiri-preferences-tests`) when an earlier test
instance is in use. Use a different settings directory and project as well.
Create the disposable project before launching it. Each card host's
child count includes one empty placeholder in addition to the open tabs.
For automatic pane/tab integration coverage, run `make check-workspace` inside
the VM. The smoke harness's `lprb` message reports every pane's views, paths,
preview/dirty flags and positions, plus the active editor's text up to 8 KiB.
`tools/vm.py right-click X Y WIDTH HEIGHT` opens native context menus; use
`screenshot -s -w OUTPUT.png` inside Haiku to capture only the active window.
`tools/vm.py drag X Y END_X END_Y WIDTH HEIGHT` performs a real pointer drag.
Send `lprb` directly to the smoke application to inspect all workspace windows,
including detached windows; each pane reports its tab-strip and panel coordinates.

Stop gracefully with `bash tools/haiku.sh shutdown`. Check the live QMP handle
before attempting to start another instance; do not infer termination from an SSH timeout.

## Recreating it

1. Download the x86_64 R1/beta5 anyboot ISO from an official Haiku mirror.
   Verify SHA-256 `22ae312a38e98083718b6984186e753d15806bd6ea44542144fdcef42c4dcb69`.
2. Create a qcow2 overlay of the ISO and a blank 24 GiB qcow2 disk. Boot the
   overlay as IDE master with the work disk as slave. Use the device, networking
   and display flags in `tools/run-vm.sh`.
3. In the live desktop, initialize **only the new blank disk** as BFS, then use
   Installer to install Haiku with development tools to that disk. Select gcc,
   binutils, make, makefile_engine and their dependencies from the optional packages.
4. Start the installed system. Install `scintilla_devel` and `lexilla_devel`.
5. Generate a dedicated ed25519 key on the host in `.vm/id_ed25519`. Put its
   public key in `/boot/home/config/settings/ssh/authorized_keys` in the VM.
   Set that directory to mode 700 and the key file to mode 600.
6. Start `/boot/system/bin/sshd`. The Haiku account name is `user`.
   Enable SSH for subsequent boots in Network preferences or UserBootscript.
7. Confirm SSH host keys before building with `tools/build-in-vm.sh`.

During this session the default beta5 HaikuPorts URL returned invalid repository
metadata. The working mirror was
`https://mirror.truenetwork.ru/haiku/haikuports/x86_64/current/`.
Current gcc/CMake dependencies on that mirror require a newer Haiku release;
the compiler bundled in the beta5 ISO avoids that mismatch. Scintilla 5.3.4 and
Lexilla 5.4.6 install on beta5.

The current test disk was installed by copying the release packages and optional
development packages, writable settings and home directories, creating all five
packagefs writable directories (`cache`, `non-packaged`, `packages`, `settings`,
`var`), marking first boot processing and running `makebootable`. Prefer Installer
when recreating the disk. Omitting `system/var` prevents the initial userspace
process from launching; the kernel log identifies `/var/shared_memory` as missing.
