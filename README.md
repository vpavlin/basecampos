# BasecampOS

A NixOS-based Linux distribution where [Logos Basecamp](https://github.com/logos-co/logos-basecamp) **is** the
desktop. It boots straight into Basecamp, full screen on a bar-less Sway, with Alisher's shells from
[basecamp-experiments](https://github.com/xAlisher/basecamp-experiments): **Basecamp** (Basecampium,
browser-like tabs; the default) or **Basecamp Legacy** (dock, app windows, split view). Native apps open as
Basecamp tabs ("Browser (in Basecamp)", "Terminal (in Basecamp)": Brave and qterminal inside the tab) or
alone on their own workspace ("Browser", "Terminal"). See [PLAN.md](PLAN.md) for the analysis, status,
findings and roadmap.

Status: experimental. Works in VMs (x86_64); arm64 builds on CI, and the tablet port is in progress.

## What's in it

| Path | What |
|---|---|
| `flake.nix` | Inputs (nixpkgs 26.05, Alisher's Basecamp + design system, shrooms, duet-kit, logos-module-builder) and outputs for x86_64 and aarch64 |
| `modules/basecamp-shell.nix` | `basecampos.shell`: the session (`sway`, the default: bar-less Sway with Basecamp as the only UI; `cage`: kiosk, no native apps), portable Basecamp + nix-ld, bundled apps and repositories installed at first boot |
| `apps/system/` | The **System** Basecamp app: a core module (power, network/Wi-Fi, volume, native app launching, URLs) and its view |
| `apps/external/`, `apps/external-embedded/` | Launchers of native apps (Browser = Brave, Terminal = qterminal): on their own workspace, or inside a Basecamp tab (a small Wayland compositor) |
| `catalog/apps.json` | Catalogue apps preinstalled into the image, pinned by `scripts/pin-catalog.py` |
| `modules/shrooms/` | `services.shrooms`: [shrooms](https://github.com/vpavlin/shrooms) mesh daemon, DNS, `shrooms-agent` |
| `modules/duet/` | Lenovo IdeaPad Duet (MT8183) support from [duet-kit](https://github.com/vpavlin/duet-kit) |
| `hosts/` | `vm` (dev VM), `live` (ISO), `desktop` (disk image; also the generic `basecampos-arm` aarch64 system), `duet-vm` |

## Build

```sh
nix build .#iso           # live ISO (x86_64, UEFI/BIOS)
nix build .#disk-image    # dd-able raw UEFI image, persistent (x86_64)
nix build .#packages.aarch64-linux.disk-image   # arm64 UEFI image (built on CI, see .github/workflows)
nix run .#vm              # dev VM (KVM; scripts/vm-*.sh drive it)
```

The desktop image's user `basecamp` logs in automatically; its password (only needed for sudo) is
`basecamp`, so change it with `passwd`.

## Following upstream

`nix flake update basecamp alisher-ds` pulls Alisher's latest work; `nix flake update duet-kit` pulls the
Duet fixes. `scripts/pin-catalog.py <apps…>` re-pins the preinstalled catalogue apps.
