# Basecamp theming (experimental)



https://github.com/user-attachments/assets/2dcf22f0-e728-4583-9942-d6a6f9d2af27



Logos Basecamp 0.3.2 with a data-only theme engine: themes are JSON files (colours, corner
rounding, font family), applied live, picked or uploaded in **Settings → Appearance**.

- `design-system/`: logos-design-system at `88330d0` (the revision Basecamp 0.3.2 pins) + theming commits
- `basecamp/`: logos-basecamp `0.3.2` + theming commits

## Build

Requires Nix with flakes enabled.

```sh
./build.sh
./result/bin/LogosBasecamp
```

## Try a theme

Example theme files are in `design-system/examples/themes/`. Upload one in
**Settings → Appearance**.
