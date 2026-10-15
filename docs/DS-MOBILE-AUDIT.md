# Design system: mobile-readiness audit

**Scope:** `logos-design-system` upstream `master` @ `a4f3014`, all 53 controls in
`src/qml/Logos/Controls` + `Theme/` · 2026-10-15 · static audit (code read + scripted scan), no
device testing yet

## TL;DR

- **The foundation is good:** every control reads `Theme.*` tokens, has pressed and keyboard-focus
  states, and sizes are logical pixels (HiDPI-scaled). `LogosDrawer` and `LogosStackView` exist, the two
  core mobile navigation pieces.
- **Main gap 1, touch targets:** 10 controls fail the 44 px minimum (iOS 44 pt, Android 48 dp) and 8
  more are near it (36 to 40 px). Worst: `LogosInfoButton` 16, `LogosRadioButton` 22, `LogosCheckbox` 24,
  `LogosCopyButton` 24. Sizes are **hardcoded literals**, not tokens, so there's no single switch.
- **Main gap 2, hover-only behaviour:** tooltips are opened by hover (documented pattern), links only
  show their underline on hover, table rows act on **double-click**. On touch these are unreachable or
  invisible.
- **Main gap 3, no adaptive layer:** no breakpoints or window-size class, no adaptive navigation, no
  safe-area or soft-keyboard handling, `inputMethodHints` used once (SpinBox).
- **The fix is one token layer, shared with themes v2:** density + control-size tokens (desktop values =
  today's, so desktop stays pixel-identical), a hit area separate from the visual size, three
  hover replacements, and a small set of layout primitives. Plan in §5.

## 1. Touch targets

Minimum: **44 px** (logical). Visual size may stay smaller if the **hit area** is ≥ 44.

| Control | Today (px) | Verdict |
|---|---|---|
| `LogosInfoButton` | 16 × 16 | ❌ fail |
| `LogosSlider` | track 4 high | ❌ fail (handle size depends on style) |
| `LogosScrollBar` | 6 thick | ⚠️ fine for flick-scroll; not draggable by finger |
| `LogosRadioButton` | indicator 22 (+4 padding) | ❌ fail |
| `LogosCheckbox` | indicator 24 (+4 padding) | ❌ fail |
| `LogosCopyButton` | 24 | ❌ fail |
| `LogosSwitch` | 40 × 24 | ❌ fail (height) |
| `LogosSpinBox` | 32 high, buttons 28 wide | ❌ fail |
| `LogosComboBox` | 32 | ❌ fail |
| `LogosMenuItem` | 32 | ❌ fail |
| `LogosButton` (`compact`) | 32 | ❌ fail |
| `LogosItemDelegate` | 36 | ⚠️ near |
| `LogosToolButton` | 36 | ⚠️ near |
| `LogosPaginator` | 36 (page buttons) | ⚠️ near |
| `LogosTable` header | 36 | ⚠️ near (rows are 64 ✅) |
| `LogosIconButton` | 40 (default `size`) | ⚠️ near |
| `LogosTile` | 40 | ⚠️ near |
| `LogosTabButton` | 40 | ⚠️ near |
| `LogosTextField` | 40 | ⚠️ near |
| `LogosButton` (default) | 44 | ✅ |
| `LogosSearchBar` | 54 | ✅ |

**Root cause:** heights are literals in each file (`implicitHeight: 32`), not `Theme.*` tokens, so
nothing can switch them globally. Spacing *is* tokenised (`Theme.spacing.*` used in 30+ controls), so
the pattern already exists; sizes just never joined it.

## 2. Hover-dependent behaviour

Hover-driven **colours** are harmless on touch (they just never show; every control also has a
pressed state). These are the ones that **lose function**:

| Control | Behaviour | On touch | Fix |
|---|---|---|---|
| `LogosToolTip` | Documented usage: `visible: hover.hovered` | Never opens | Long-press (`pressAndHold`) opens it; tap elsewhere closes |
| `LogosLink` | Underline only while hovered/pressed | Links look like plain text | Underline always when the pointer isn't a mouse (or a token) |
| `LogosTable` | `rowDoubleClicked` | Double-tap is undiscoverable and conflicts with zoom | Tap = select, long-press or explicit action button = open |
| `LogosToast` | Pauses auto-dismiss while hovered | Can't pause | Acceptable; optionally tap to pin |
| `LogosScrollBar` | Shows on hover or activity | Shows on activity only | Acceptable |

`cursorShape` (11 controls) is harmless on touch.

## 3. Fixed widths and type scale

- **Implicit widths** (`LogosTextField` 200, `LogosSearchBar` 400, `LogosSlider` 200) are defaults that
  layouts override: fine, but `LogosSearchBar` at 400 is wider than a 360 to 390 px phone if a layout
  doesn't constrain it.
- **Dialogs:** the shell's `ConfirmationDialog` is a fixed 560 px. Dialogs need
  `width: Math.min(preferred, window.width - 2 * margin)` (a DS helper, so apps get it free).
- **Type scale:** `badgeText` 8 px is illegible on a phone; `secondaryText` 12 is at the lower bound;
  `mainTitleText` 256 and `pageTitleText` 36 need a smaller step on compact windows. v1 already reserves
  `typography.sizes` (validated, not applied): the same token can carry a per-density scale.

## 4. Missing primitives

| Need | Today | Note |
|---|---|---|
| Window-size class / breakpoints | none | e.g. compact < 600 px, medium < 840, expanded (Material's classes) |
| Adaptive navigation | Basecamp sidebar is fixed (46 to 56 px items); no DS nav component | Sidebar ↔ bottom bar or drawer by size class; `LogosDrawer` is the base |
| Safe areas (notch, home indicator) | none | Qt 6.9 adds a `SafeArea` attached property (verify on the iOS/Android builds) |
| Soft keyboard | no avoidance; `inputMethodHints` only in `LogosSpinBox` | `LogosTextField` should expose `inputMethodHints` and enter-key type; a helper keeps the focused field above `Qt.inputMethod.keyboardRectangle` |
| Long-press / context actions | none (no right-click either) | Needed for tooltips, table rows, list items |
| Density | none | One token, three values; see §5 |

## 5. Plan: one token layer for mobile and themes v2

The same mechanism serves both tracks: **component tokens** (each control reads its own size and style
values from `Theme`) let a theme restyle widgets without code, and let a density setting resize them for
touch.

| # | Change | Effort | Desktop impact |
|---|---|---|---|
| 1 | **Size tokens:** `Theme.size.controlHeight{Small,Medium,Large}`, `indicator`, `iconButton`, `touchTarget`; replace the ~20 height literals | M | None: desktop values = today's literals (test asserts it) |
| 2 | **Density:** `Theme.density` = `compact` \| `comfortable` \| `touch`; `touch` raises heights and type scale; default chosen by platform | S (after 1) | None (desktop default = today) |
| 3 | **Hit area ≥ `touchTarget`** independent of visual size (expanded `containmentMask` or transparent padding) for indicators, info and copy buttons | M | None when `touchTarget` = 0 on desktop |
| 4 | **Hover replacements:** tooltip on long-press, link underline on touch, table tap/long-press | S | None |
| 5 | **Window-size class** in `Theme` (from the root window width) + dialog max-width helper | S | None |
| 6 | **`LogosAdaptiveNav`:** sidebar on expanded, bottom bar or drawer on compact; Basecamp shell and apps use it | L | None if desktop keeps the sidebar |
| 7 | **Input:** `inputMethodHints` / enter key on text controls; keyboard-avoidance helper; `SafeArea` margins | M | None |

**Verification:** run the existing 693-test suite at each density; add one test per control for
"hit area ≥ touchTarget in `touch`"; render key screens offscreen at 390 × 844 and 360 × 800; then
real-device checks on a mobile build (hover-free behaviour can't be proven headless).

## 6. Open questions

1. **Ownership:** design-system tokens and primitives vs Basecamp shell navigation, and how this meets
   the Basecamp mobile port.
2. **Density default by platform** (Android/iOS → `touch`) or by window size class? Probably platform,
   overridable in Settings.
3. **Do apps opt in** to adaptive navigation, or does the shell own navigation and apps only fill a page?
4. **Order with themes v2:** size tokens (1) first, since both tracks need them; gradients can run in
   parallel.
