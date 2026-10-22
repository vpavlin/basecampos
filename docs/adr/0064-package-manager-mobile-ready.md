# ADR-064: Package Manager: filters become pickers, the table becomes cards, details become rows

**Status:** Accepted (experiment) · 2026-10-22

## Context
Third real screen, in its own repo (`logos-package-manager-ui`), running in its own QML engine inside Basecamp. Before: 390 and 700 clipped (sidebar, header and search off the edge); at 1280 the table scrolled sideways with Uninstall off-screen.

## Decision
- **Filters are not navigation:** below expanded the categories / types sidebar becomes two dropdowns; Install Local Package, Reload, Manage Repositories and Show unavailable (a checkable item with the DS check mark) move into a ⋯ menu.
- **Table pattern:** expanded keeps the table but drops Description instead of scrolling sideways (and the columns the details panel repeats); medium keeps Package · Available · Action + ⋯; compact shows cards (name + action, a facts line, the action's tooltip text as a visible line, ⋯), grouped by repository.
- **One details panel placed by width:** beside the table (expanded), a sheet over its right side (medium), full width in place of the list (compact) with "‹ Packages" in the page's own navigation row; below desktop the details are label / value rows and the per-row version picker and Uninstall move into an **action strip, last**.
- Search folds into the compact navigation row; the paginator compacts by itself; `Theme.compactWindow` bound at the root (own engine); the private `_d.fullMinWidth` binding and other DS workarounds removed.
- The repo's UI tests are fixed (they read a baseline before the catalog loaded, clicked by visible text, and read the wrong `d`): 49 / 49; unit tests 39 / 39.

## Consequences
+ 0 overflow at all widths; 0 targets < 44 at 390 comfortable and at 700 touch; desktop the same or better.
- Needs the DS fork's PR #2 (`LogosSearchToggle`, `Theme.compactWindow`): stock Basecamp's DS can't build it.
- Esc / Back closing the details verified through the shortcut's handler only (the inspector can't send key events).

## Evidence
`xAlisher/logos-package-manager-ui` branch `mobile-ready`: 36cfe64, 8efce4b, 2b6de2d, 2ce7c63.
