# basecampos-vm: the development VM on the dev host.
#
# Run:   nix run .#vm            (disk image basecampos-vm.qcow2 persists in $PWD)
# Reach: ssh -p 2222 -i ~/.ssh/basecampos_vm_ed25519 basecamp@127.0.0.1
#        VNC on 127.0.0.1:5907 (display :7)
#
# No secrets here: everyone on the mesh can drive this VM's agent.
{ config, lib, pkgs, modulesPath, logosQtMcp, ... }:

{
  imports = [ ./vm-common.nix ];

  networking.hostName = "basecampos-vm";

  basecampos.shell.enable = true;
  basecampos.shell.session = "sway";

  services.shrooms = {
    enable = true;
    name = "basecampos-vm";
    mode = "Edge";
    agent = {
      enable = true;
      user = "basecamp";
      path = [ pkgs.claude-code ];
    };
  };

  environment.systemPackages = with pkgs; [ tmux nodejs claude-code ];

  # The in-VM agent drives Basecamp through its QML inspector (on by default in
  # the dev build, localhost:3768).
  # User scope, not managed-mcp.json: a managed file would shut out the
  # shrooms MCP tools the agent adds per session.
  systemd.user.services.qml-inspector-mcp = {
    wantedBy = [ "default.target" ];
    before = [ "shrooms-agent.service" ];
    unitConfig.ConditionUser = "basecamp";
    path = [ pkgs.claude-code ];
    serviceConfig.Type = "oneshot";
    script = ''
      claude mcp remove --scope user qml-inspector >/dev/null 2>&1 || true
      # Name first: -e takes every following word as KEY=value.
      claude mcp add --scope user qml-inspector \
        -e QML_INSPECTOR_HOST=localhost -e QML_INSPECTOR_PORT=3768 \
        -- ${pkgs.nodejs}/bin/node ${logosQtMcp}/mcp-server/index.mjs
    '';
  };

  virtualisation.vmVariant.virtualisation.qemu.options = [
    "-device virtio-vga-gl,xres=1920,yres=1080"
  ];
}
