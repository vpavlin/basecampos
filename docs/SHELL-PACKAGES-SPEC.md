# Shell packages: spec

Status: draft, 2026-10-20. Based on logos-basecamp `master` @ `5f8cca8` + the theming / Basecamp OS
commits (`basecamp-os-theme`), logos-package-manager `d3af297`, logos-package-manager-module `6955a09`.
File references are to that tree.

## TL;DR

- A **shell package** replaces Basecamp's whole UI: navigation, how apps are shown, search, the home
  screen, Settings. Apps, core modules, keys and themes stay as they are.
- Basecamp is already built for this: its UI is a plugin (`main_ui`) behind a small interface. What
  is missing is everything around it: choosing a shell, installing one as a package, surviving a bad
  one, and trusting who built it.
- **Ten changes in Basecamp** (§5). Four are needed for a demo (shell choice, load order, fallback
  instead of crash, a backend API version). The rest are needed before strangers can publish shells,
  with **signature enforcement** the hard gate.
- Path: demo with a local shell (no package, no trust model) → `ui_shell` package type behind a
  developer flag → public shells once Basecamp checks signatures.

## 1. Terms

| Term | What it changes | Code? | Who ships it |
|---|---|---|---|
| **Theme** | how things look (colours, fonts, gradients, icons, wallpaper) | no, data only | anyone, as a `.theme.json` |
| **Layout** | how the default shell arranges things: Window (default), Desktop, later mobile | built into Basecamp | Logos |
| **Shell package** | the whole UI, replaced by another one | yes, a compiled plugin | builders, as a signed package |

A shell is code that sees every app window and every keystroke. That is why it needs a trust model
that themes don't.

## 2. How Basecamp loads its UI today

- The UI is `plugins/main_ui/main_ui.<so|dylib|dll>`, loaded by `Window::setupUi`
  (`app/window.cpp:106-160`).
- **Search order:** the embedded copy next to the app first; the user's `plugins/main_ui` only if
  there is no embedded one (`window.cpp:118-124`). In a normal install the built-in shell always wins.
- **Every failure stops the app** with `qFatal`: plugin won't load (131-135), doesn't implement
  `IShellView` (141-145), ABI mismatch (151-154). The comment at 156-160 says this is deliberate: a
  missing shell was a broken install.
- The app owns the window, tray, menu bar, quit, the backend and the adapter; it hands the shell a
  host object and puts the shell's widget in the middle (`setCentralWidget(createShell(host))`).
- The packaged shell has no `manifest.json` (`nix/main-ui.nix:57-77`); its plugin metadata declares
  `"type": "ui"` (`src/metadata.json`).

## 3. The contract a shell gets

### 3.1 The versioned part

`IShellView` (`app/interfaces/IShellView.h`), implemented by the shell:

- `QWidget* createShell(IShellHost* host)`
- `void destroyShell(QWidget*)` (must `setObserver(nullptr)` first)
- `int hostAbiVersion() const` (must return the `IShellHost_abi` it was built with)
- IID `com.logos.basecamp.IShellView/1.0`

`IShellHost` (`app/interfaces/IShellHost.h`, `IShellHost_abi = 3`), given to the shell:

- `backendObject()`, `currentSectionIndex()`, `setCurrentSectionIndex(int)`
- `loadUiModule(name)`, `unloadUiModule(name)`, `setCurrentVisibleApp(name)`
- `displayNameFor(name)`, `setObserver(IShellObserver*)`

`IShellObserver`, implemented by the shell, called by the host: section changed, navigate to apps,
an app window to show / remove, present an app.

### 3.2 The unversioned part (the real gap)

The shell's QML talks to `MainUIBackend` through `backendObject()`: its properties, invokables and
signals (`app/MainUIBackend.h:70-444`) are the apps list, catalog, install / uninstall, intents,
dialogs, repositories. **None of this is covered by `IShellHost_abi`.** A shell built against one
Basecamp can load in the next one and break at runtime with no error. A third-party shell can't be
supported until this has a version (§5.4).

### 3.3 What keeps a shell in its lane today

- Build rules: the shell may include only `app/interfaces/` and link only Qt + the design system
  (`src/CMakeLists.txt:5-17,31-38`).
- `nix/symbol-gate.nix`: the shell may not define the Logos runtime types (`TokenManager`,
  `LogosAPI`...), so it can't talk to core modules, keys or the network directly.
- **Both are build-time checks on the bundled shell only.** A shell installed from outside is never
  checked. This matters for §5.7.

## 4. What a shell must provide

Everything in `src/` lives inside `main_ui`, so a replacement shell has to reimplement it or reuse
its source:

| Piece | Today | Notes |
|---|---|---|
| Navigation and app presentation | `MainContainer`, `WorkspaceArea`, `DesktopArea`, sidebar / dock QML | the part a shell exists to change |
| Settings page | `Basecamp/Settings/*.qml` (Appearance, Repositories...) | a shell without Settings leaves users unable to change theme or repos |
| Overlay and intent dialogs | `OverlayDialogs.qml`, `IntentChooserDialog`, `IntentInstallDialog`, `ConfirmationDialog`, `UninstallDialog`, driven by `backend` signals | security-relevant: install / uninstall confirmations |
| Shell intents | `basecamp.settings.open`, `.apps.open`, `.packages.open`, `.repositories.manage` answered in `ContentViews.qml:72-96` | apps call these; a shell that ignores them breaks those apps |
| Package Manager | separate sandboxed `package_manager_ui`, loaded with `loadUiModule` | reusable as is |
| Shortcuts | `ShortcutBridge` | needed for keys across app processes |
| Theme upload | `ThemeInstaller` (only the shell can write themes) | |
| Layout / wallpaper prefs | `ShellPrefs` (inside the shell) | each shell keeps its own prefs |

The app keeps: window, tray, menu bar, quit, `MainUIBackend`, plugin and package management, intent
routing (`IntentBroker`, `IntentRegistry`, `UIPluginPresenter`).

## 5. What Basecamp has to change

### 5.1 Shell choice

Settings → Appearance → **Shell**: Default, or an installed shell (name, builder, version). Same
pattern as Layout: saved in settings, applied with **Restart now**. A developer override for local
testing: `BASECAMP_SHELL=<path to a main_ui folder>`.

The choice must be stored **outside** the shell (today `ShellPrefs` lives inside `main_ui`), because
the app reads it before any shell is loaded.

### 5.2 Load order

Load the chosen shell; if none is chosen, the embedded default. Never let an installed shell take
over just by existing (today the reverse order would make any file at `plugins/main_ui` win).

### 5.3 Fallback instead of crash

Replace the three `qFatal`s with: log the reason, load the embedded default shell, and tell the user
("Your shell *X* couldn't start: *reason*. Basecamp is using the default shell."). The default shell
still failing stays fatal: that really is a broken install.

### 5.4 A versioned backend API

Give the backend surface a version, checked like `IShellHost_abi`:

- either declare a `shellApiVersion` the shell requires (in its manifest) and refuse to load a shell
  built for a different major version;
- or, better long-term, move what a shell needs from `MainUIBackend` behind a documented interface
  (`IShellBackend`) with its own ABI number, and keep the rest of `MainUIBackend` private.

Without this, every Basecamp release can silently break every shell.

### 5.5 Package type `ui_shell`

- New manifest `type: "ui_shell"`, installed by the package manager to its own folder
  (`plugins/shells/<name>/`), never to `plugins/main_ui`.
- Today the package manager accepts `core | ui | ui_qml` (`package_manager_lib.h:142`) and puts every
  non-core type in the UI plugins folder (`package_manager_lib.cpp:455-476`), with no allowlist. It
  needs the new type, its folder, and a rule that a `ui_shell` is never loaded as an app.
- Manifest fields: `name`, `version`, `type: "ui_shell"`, `hostAbiVersion`, `shellApiVersion`,
  `displayName`, `description`, `icon`, `screenshots` (shown before install), variants per platform.
- Package Manager / Applications: shells listed separately ("Shells"), never mixed with apps.

### 5.6 Safe mode

- **Crash at start:** write a "starting shell X" marker before loading and clear it once the shell
  has shown its first frame. If the marker is still there at the next start, use the default shell
  and say why.
- **Forced:** a key held at launch (e.g. Shift) and a `--default-shell` flag start the default
  shell regardless of settings.
- Hangs: the same marker catches a shell that never shows a frame (killed by the user).

### 5.7 Signing and trust (the hard gate)

What exists: the package manager has `SignaturePolicy {NONE, WARN, REQUIRE}` with a local keyring
(`package_manager_lib.cpp:344-412`). Basecamp never sets it, so the policy is WARN: unsigned packages
install, and repository `trustedSigners` are "parsed and deliberately never consulted". Basecamp says
so itself (`MainUIBackend.cpp:679-684`: "Every package is unsigned today").

For shells:

- **Signed by the builder, not verified by Logos.** A shell is distributed like an app; the
  signature says who built it and that it wasn't altered. It doesn't mean Logos reviewed it.
- **`REQUIRE` for `ui_shell`, always**: an unsigned or altered shell is refused, whatever the global
  policy is.
- **Before install and before switching:** show the signer, version, what a shell can see ("every app
  window and everything you type in Basecamp"), and require an explicit confirmation.
- **Runtime check:** verify the installed files against the signed hashes at every load, not only at
  install (a shell is the one package where tampering after install matters most).
- **Lane checks at install:** run the symbol-gate rules (no Logos runtime symbols) on the shell
  binary before it can be selected, so a shell can't reach core modules or keys directly.
- Users decide which builders to trust and are responsible for the shells they install.

### 5.8 Hard-coded `"main_ui"`

`basecamp.*` capabilities are registered under the name `"main_ui"` (`MainUIBackend.cpp:470,
490-494`), and `UIPluginManager` special-cases that name (`UIPluginManager.cpp:239, 717`). These must
refer to "the active shell", whatever its package name.

### 5.9 A shared shell library

Ship the reusable parts of the default shell as a library a shell links against, so a builder
writes navigation and presentation, not the plumbing: app hosting, `ShortcutBridge`, overlay and
intent dialogs, Settings pages (as components a shell can place anywhere), `ThemeInstaller`. Without
it every shell copies `src/` and drifts from security fixes in the dialogs.

### 5.10 Required behaviour (a shell's checklist)

A shell must, to be selectable:

- answer the `basecamp.*` shell intents (open Settings, apps, packages, repositories);
- show the install / uninstall / intent confirmation dialogs (it may restyle them, not skip them);
- offer a way to reach Settings, at least theme, shell choice and repositories;
- apply the current theme (design system), so themes keep working across shells.

Some of this can't be enforced in code; the shared library (§5.9) makes doing it right the easy path,
and the warning in §5.7 covers the rest.

### 5.11 Settings, Applications and Package Manager as system apps

Proposed with Khushboo, 2026-10-20. Today Settings and Applications are drawn by the shell
(`main_ui`); only Package Manager is already a separate, sandboxed package (`package_manager_ui`)
that the shell asks the host to load. Move all three out of the shell and ship them as
**preinstalled system apps**; a shell only provides **launch points** for them.

**What it gains**

- **A smaller shell.** A shell becomes the frame: navigation, how apps are shown, launch points.
  §4's table loses Settings, Applications and the Package Manager page; the shared shell library
  (§5.9) shrinks accordingly.
- **Sensitive screens leave shell code.** The shell-switch warning (§6.2), theme install,
  repositories and uninstall confirmations are Logos code in every shell. A hostile shell can no
  longer fake or skip the warning for the next switch, or quietly add a repository through its
  own Settings page.
- **One Settings everywhere.** Customisation → Shell → Default is in the same place whatever shell
  runs, so "how to go back" is reliable.
- **Launch points already exist:** the `basecamp.settings.open`, `basecamp.apps.open` and
  `basecamp.packages.open` intents. §5.10's checklist becomes "offer launch points for the system
  apps".

**What it doesn't change.** The shell still hosts every app's window, the system apps' included,
and sees every keystroke. A hostile shell could hide the Settings window or overlay a fake one.
The warning before switching, safe mode (§5.6) and the crash fallback stay essential: this
**reduces** a shell's reach; it doesn't make shells safe.

**What Basecamp needs**

1. **A system capability** (e.g. `basecamp.system`): the privileged backend API Settings and
   Applications use today (theme install, shell preferences, repositories, catalog, install /
   uninstall), granted only to embedded, signed system apps.
2. **Not replaceable:** system apps are preinstalled, can't be uninstalled, and a catalog package
   can't replace them (the same class of problem as logos-basecamp#478, where a third-party catalog
   can replace a shared core module).
3. **Live theme across processes:** apps in their own process switch theme only after a restart
   today; the host must broadcast theme changes so a Settings app reflects them at once.
4. **Order:** Package Manager is already out; **Settings next** (Customisation and the shell-switch
   warning: the biggest safety gain), then Applications.

## 6. Install and switch: user experience

Agreed 2026-10-20; prototyped on the `basecampium` branch.

### 6.1 Install

- **Where:** Package Manager (from a catalog, or *Install Local Package* with a `.lgx`), and
  Settings → Customisation → Shell → **Add shell…**, which leads to the same installer. Not
  Applications: a shell isn't an app you open.
- **Recognised as a shell:** manifest `type: "ui_shell"`. Basecamp shows a **Shell** badge in
  Package Manager and the catalog, and a screenshot preview. Shells never appear in the app grid,
  the dock or search.
- **Installing doesn't switch.** Install gets the normal light confirmation (name, builder,
  version). Switching is the moment that needs real consent.

### 6.2 Switch: the warning

Settings → Customisation → Shell → pick a shell → confirmation:

```
Use "Basecampium" by <builder> as your Basecamp?

A shell draws all of Basecamp. It can see everything shown in Basecamp and everything you
type into it, including passwords and recovery phrases you enter in apps. Logos has not
reviewed it.

What Basecamp checked (hints, not guarantees)
  • Signature: signed by <signer> (name given by the signer); not verified yet
  • Logos runtime: no access to core modules or keys found
  • Network: no network library found
  • QML network access: no XMLHttpRequest or WebSocket found
  • Files: not checkable; a shell runs inside Basecamp

To go back: Customisation → Shell → Default, or hold Shift while starting Basecamp
(or start it with --default-shell). Uninstalling the shell and restarting also works.

[ ] I trust this builder and use this shell at my own risk

                                              [Cancel]  [Switch and restart]
```

- **Exact scope:** "everything shown in or typed into Basecamp", not "all your keystrokes": a
  shell can't see other programs. Overstating it teaches people to ignore warnings.
- **Recovery never depends on the shell.** A broken or hostile shell may not show Package Manager,
  so the main escape is **safe mode** (Shift at launch, `--default-shell`) plus the automatic
  fallback when a shell fails or doesn't survive start-up. Uninstalling is the third way.
- "Switch and restart" stays disabled until the checkbox is ticked. Switching back to Default
  needs only a plain confirmation.

### 6.3 Restart

A shell is the whole UI, so switching restarts Basecamp ("Switch and restart", using the existing
Restart now mechanism).

### 6.4 What Basecamp can check

The shell runs inside Basecamp's process, so these are **hints, never guarantees**: worded as
"found no…", never "safe".

| Check | When | Catches | Limit |
|---|---|---|---|
| Signature valid, builder known | install + every start | tampering, impersonation | says *who*, not *safe* |
| No Logos runtime symbols (the existing symbol gate) | install | direct access to core modules, keys | |
| Imported libraries: Qt Network, sockets, curl | install | obvious networking | bypassable (runtime loading, raw syscalls) |
| QML networking (XMLHttpRequest, WebSocket) | install | obvious network use from QML | native code can still do it |
| Writes outside its own folder | runtime | logging to disk | not enforceable in-process |

The real guarantee is running the shell in its own sandboxed process, as Basecamp does for apps:
the long-term answer, and a big change.

### 6.5 Uninstall and updates

- Uninstalling the active shell: Basecamp uses the default shell after a restart (and says so).
- Update from the same signer: installs quietly with a "Shell updated" note. A different signer is
  a new shell: full warning again.

### 6.6 Naming

Settings → **Appearance** becomes **Customisation**: theme and density (look), layout and wallpaper
(arrangement, default shell only), shell. It matches the three layers: themes, layouts, shell
packages.

### 6.7 Prototype status (`basecampium` branch)

| Piece | State |
|---|---|
| `ui_shell` package built and signed with `lgx` (`tools/make-shell-lgx.sh`) | done |
| Installed by the stock package manager into `plugins/<name>/` with `manifest.sig` | done (no package-manager change needed) |
| Customisation rename, Shell section, "Add shell…" → Package Manager | done |
| Warning with checks, recovery note, checkbox; Switch and restart | done |
| Safe mode (Shift, `--default-shell`, `BASECAMP_SAFE_MODE`) and crash-at-start fallback | done |
| Shell badge in Package Manager / catalog | not done (lives in `package_manager_ui`) |
| Signature verification | not done: Basecamp shows the self-asserted signer and says it isn't verified |

## 7. Developer experience

- **Template repo:** a minimal shell (one window, list of apps, Settings) built with the shared
  library, with a Nix flake that produces the `ui_shell` package for Linux and macOS.
- **Run locally:** `BASECAMP_SHELL=./result/plugins/main_ui ./LogosBasecamp --user-dir /tmp/shell-dev`,
  with the default shell as fallback when it fails.
- **Docs:** the contract (§3), the checklist (§5.10), the manifest (§5.5), how to sign.
- Basecamp OS (the Desktop layout) is the first real example: it could be published as a shell
  package once §5.7 exists, which proves the mechanism end to end.

## 8. Phases

| Phase | Changes in Basecamp | Result |
|---|---|---|
| **0. Demo** | §5.1 developer override only, §5.2, §5.3 | a second shell runs from a local folder; a bad one falls back instead of crashing |
| **1. Package, developers only** | §5.4, §5.5, §5.6, §5.8, Settings → Shell behind a developer flag | shells install from a repository; safe mode; versioned API |
| **1b. System apps** | §5.11: Settings, then Applications as preinstalled system apps; system capability; live theme across processes | a shell is only the frame; the warning, repositories and installs are the same Logos code in every shell |
| **2. Public** | §5.7 (signing enforced for shells), §5.9, §5.10, template + docs | anyone can publish a signed shell; users see who built it |

Phase 0 is small and local to `app/window.cpp` plus one setting. Phase 2 depends on Basecamp checking
signatures at all, which is wider than shells.

## 9. Open questions

1. **Who owns signing?** Signature enforcement touches every package type; shells would be the first
   user. Needs an owner on the Basecamp / package-manager side.
2. **Backend API shape:** version the existing `MainUIBackend` surface (fast) or define
   `IShellBackend` (cleaner)? Affects how stable shells are across releases.
3. **Qt / ABI compatibility:** a compiled shell must match Basecamp's Qt version and build keys per
   platform. Do shells rebuild for every Basecamp release, or is there a stable window?
4. **Settings ownership:** should Settings move out of the shell into the host (so every shell has
   the same, trusted Settings), or stay a shell part provided by the shared library?
5. **Mobile:** does the mobile port use the same `IShellView` contract (a mobile shell), or a layout
   of the default shell?
6. **Review:** Logos doesn't review third-party apps; is a shell different enough to warrant a
   curated list (e.g. "featured shells") on top of signing?
