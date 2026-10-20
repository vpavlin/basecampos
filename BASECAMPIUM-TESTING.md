# Testing Basecampium (a shell package)

A **shell package** replaces Basecamp's whole UI. Your apps, keys and theme stay the same.
**Basecampium** is the first one: Basecamp with browser-like tabs. This is a prototype, not an
official Logos release.

What we want to learn: **does installing and switching a shell feel safe and clear, and does
Basecampium feel right to use?**

## 1. Download

From the [Basecampium test release](https://github.com/xAlisher/basecamp-theming/releases/tag/basecampium-3):

| File | What |
|---|---|
| `basecamp-shells-x86_64.AppImage` | Basecamp that can use shell packages (Linux x86_64) |
| `basecamp-shells-macos-arm64.zip` | the same for macOS (Apple Silicon) |
| `basecampium.lgx` | the Basecampium shell package (one file for Linux and macOS) |

## 2. Start Basecamp

Quit your regular Basecamp first. Then run the test build in its own folder (your regular
Basecamp is not changed):

Linux:

```sh
chmod +x basecamp-shells-x86_64.AppImage
./basecamp-shells-x86_64.AppImage --user-dir ~/basecamp-shells-test
```

macOS: unzip `basecamp-shells-macos-arm64.zip` and keep `LogosBasecamp.app` in Downloads (not in
Applications: it has the same name as your regular Basecamp). It isn't signed by an Apple developer,
so the first time macOS warns: on macOS 15 or later open it once, then **System Settings → Privacy &
Security → Open Anyway**; on macOS 14 or earlier right-click → **Open**; or run
`xattr -dr com.apple.quarantine ~/Downloads/LogosBasecamp.app`. Then start it with:

```sh
open -n ~/Downloads/LogosBasecamp.app --args --user-dir ~/basecamp-shells-test
```

It starts with the **default shell**: the Basecamp you know. Install a couple of apps from
Applications so there is something to open in tabs. (To use your real data instead, see
[TESTING.md](TESTING.md) §2, option B.)

## 3. Install the shell

**Package Manager → Install Local Package** → pick `basecampium.lgx`.

Installing doesn't switch anything yet. You can also get here from **Settings → Customisation →
Shell → Add shell…**.

## 4. Switch to it

**Settings → Customisation → Shell** lists *Default by Logos*, *Desktop by Logos* and, once
installed, *Basecampium by Alisher*. The two built-ins switch without any dialog (just **Restart
now**). Choose **Basecampium**: before anything changes, Basecamp shows:

- what a shell can see: everything shown in Basecamp and everything you type into it;
- what it checked (signature, access to the Logos runtime, network libraries, QML network access),
  marked as hints, not guarantees;
- how to go back.

Tick **I trust this builder and use this shell at my own risk**, then **Switch and restart**.

## 5. Try Basecampium

- One bar: tabs, **+** after the last tab, window buttons on the right. No title bar, no sidebar.
- **New tab** is the launcher: search, your apps, and Applications / Settings / Package Manager as
  round buttons at the bottom.
- Apps, Settings, Applications and Package Manager all open as tabs.
- Drag the empty part of the bar to move the window, double-click it to maximise, resize from the
  edges. Drag tabs to reorder; middle-click or right-click a tab to close.
- **Tear a tab off:** drag it well out of the bar (or right-click → **Move to new window**) and the
  app opens in its own window, with the same bar as the main one. **Back to tabs** (the button left
  of minimise, or Ctrl+Shift+D) puts it back as a tab. Closing that window closes the app.

| Keys | Action |
|---|---|
| Ctrl+T | New tab |
| Ctrl+W | Close tab |
| Ctrl+Shift+T | Reopen last closed tab |
| Ctrl+Tab / Ctrl+PgDown | Next tab |
| Ctrl+Shift+Tab / Ctrl+PgUp | Previous tab |
| Ctrl+1 … Ctrl+8 | Go to tab 1-8 |
| Ctrl+9 | Last tab |
| Ctrl+K | New tab with search focused |
| Ctrl+, | Settings tab |
| F11 | Full screen |

Your tabs come back the next time Basecamp starts.

## 6. Go back to the default shell

Any of these:

- **Settings → Customisation → Shell → Default by Logos** → Restart now;
- **safe mode:** hold **Shift** while Basecamp starts, or start it with `--default-shell`;
- uninstall Basecampium in **Package Manager** and restart.

If a shell fails to load or doesn't survive start-up, Basecamp uses the default one by itself and
says why.

## 7. Known limits

- Signatures are shown (who signed it, as the signer names themselves) but **not verified** yet.
- The checks are hints: a shell runs inside Basecamp's process, so Basecamp can't prove what it
  does. Only running shells in their own sandboxed process would.
- Package Manager doesn't label shell packages as "Shell" yet.
- Holding Shift at start may not be seen on some Wayland desktops; `--default-shell` always works.
- macOS: Basecamp draws its own traffic lights over the top-left corner; check they don't cover
  Basecampium's tabs (not yet verified on a Mac screen).

## 8. Make your own shell

- How shell packages work and what Basecamp has to change: [docs/SHELL-PACKAGES-SPEC.md](docs/SHELL-PACKAGES-SPEC.md)
- Basecampium's design: [docs/BASECAMPIUM-SPEC.md](docs/BASECAMPIUM-SPEC.md)
- Packaging: `basecamp/tools/make-shell-lgx.sh <main_ui library> <out.lgx> [key]` builds and signs
  a `ui_shell` package with the `lgx` tool.

## 9. Feedback

Open an issue at https://github.com/xAlisher/basecamp-theming/issues: your OS and screen, what you
tried, what you expected, what happened, a screenshot if it's visual. Especially: was the warning
too much, too little, or just right?
