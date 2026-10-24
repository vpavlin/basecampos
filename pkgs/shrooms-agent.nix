{ lib, buildGoModule, src }:

buildGoModule {
  pname = "shrooms-agent";
  version = "unstable";
  inherit src;
  subPackages = [ "cmd/shrooms-agent" ];
  vendorHash = "sha256-3TDf7XsR1/P+NYAxxtpCEIjfOIXE/ReQMeRF/UWA65k=";
  env.CGO_ENABLED = 0;
  doCheck = false;
  meta.mainProgram = "shrooms-agent";
}
