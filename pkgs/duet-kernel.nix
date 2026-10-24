# The Duet's kernel, rebuilt the way hexdump0815 builds the "stb-cbm" kernel
# the velvet-os kukui image ships (and the tablet runs as 6.12.50-stb-cbm-cam):
# upstream 6.12.50 + his mt8173/mt8183/mt81xx patches for v6.12 + his full
# config for exactly that release. NixOS then carries modules that match.
# The camera drivers (vpavlin/mt8183-camera: config fragment, patches, DT, out-
# of-tree modules) are not in yet.
# `...`: linuxPackagesFor re-calls this with features/randstructSeed overrides.
{ lib, fetchurl, linuxManualConfig, hexkernel, ... }:

let
  patchDir = "${hexkernel}/misc.cbm/patches/v6.12";
  patches = lib.filter
    (n: builtins.match "(mt8173|mt8183|mt81xx).*\\.patch" n != null)
    (builtins.attrNames (builtins.readDir patchDir));
in
linuxManualConfig {
  version = "6.12.50";
  modDirVersion = "6.12.50-stb-cbm";
  src = fetchurl {
    url = "mirror://kernel/linux/kernel/v6.x/linux-6.12.50.tar.xz";
    sha256 = "19bjzhxasj4r6m1lhsa486a96axfigbm06kqa2lwa7y2s5sbsdf4";
  };
  configfile = "${hexkernel}/config.cbm-6.12.50-stb-cbm+";
  kernelPatches = map (n: { name = n; patch = "${patchDir}/${n}"; }) patches;
  # Lets NixOS read the config to check what it requires (needs IFD).
  allowImportFromDerivation = true;
}
