{
  description = "BasecampOS System: the view for power, sound and network";

  inputs = {
    logos-module-builder.url = "github:logos-co/logos-module-builder";
    # Named as in metadata.json's dependencies: the builder reads its LIDL contract.
    basecampos_system = {
      url = "path:../core";
      inputs.logos-module-builder.follows = "logos-module-builder";
    };
  };

  outputs = inputs@{ logos-module-builder, ... }:
    logos-module-builder.lib.mkLogosQmlModule {
      src = ./.;
      configFile = ./metadata.json;
      flakeInputs = inputs;
    };
}
