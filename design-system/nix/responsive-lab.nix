{ pkgs, common }:

# The responsive lab (docs/RESPONSIVE.md): app layouts built from design-system
# controls, rendered at any width, with an overflow / touch-target report.
pkgs.stdenv.mkDerivation rec {
  pname = "logos-responsive-lab";
  version = "1.0.0";

  inherit (common) src nativeBuildInputs buildInputs preConfigure;

  cmakeFlags = common.baseCmakeFlags ++ [
    "-DLOGOS_DS_BUILD_RESPONSIVE_LAB=ON"
  ];

  dontWrapQtApps = pkgs.stdenv.hostPlatform.isWindows;

  configurePhase = ''
    runHook preConfigure
    cmake -S . -B build $cmakeFlags
    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    cmake --build build --target LogosResponsiveLab
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/bin $out/lib/lab
    cp "$(find build -maxdepth 3 -type f -name LogosResponsiveLab | head -1)" $out/bin/
    cp -r ${src}/responsive-lab/qml/. $out/lib/lab/
    runHook postInstall
  '';

  meta = {
    description = "Check Logos design-system app layouts across window sizes";
    mainProgram = "LogosResponsiveLab";
  };
}
