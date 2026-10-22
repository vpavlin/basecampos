# ADR-056: Layouts are proven by rendering at 390 / 700 / 1280, not asserted

**Status:** Accepted (experiment) · 2026-10-22

## Context
"Should work on mobile" kept being wrong in this session (Settings looked fine at 1280 and overflowed at 700 before anyone touched it). A rule nobody can check isn't a rule.

## Decision
- The DS ships a **responsive lab** (`.#responsive-lab`): common layouts built only from DS controls, and a shot mode that renders any screen at any width and reports **horizontal overflow** and **tap targets under 44** (buttons, mouse areas, inputs, spin-box parts). `check-all.sh` runs every screen at 390 touch / 700 touch / 1280.
- Rendering is headless on a sway session with mesa's llvmpipe (CPU OpenGL), because the software backend draws `LogosIcon` blank (MultiEffect). Each shot gets its own settings (density had been leaking between runs).
- App screens outside the lab use the QML inspector: click by objectName, never by visible text.
- Done = empty reports at all widths **and** the three PNGs looked at.

## Consequences
+ Regressions show up as a FAIL line with a rect, not as a user screenshot.
- The checker had false negatives (clipped rect compared to the window) and a false positive (a ListView's empty highlight); both fixed, and it can still miss what it doesn't model.

## Evidence
PR #1 / #2; `check-all.sh` output; the Settings run's 700 px overflow found and fixed.
