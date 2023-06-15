{
  description = "turns thinkpad trackpoint buttons into keyboard thumb clusters";
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };
  outputs = { self, nixpkgs, flake-utils }:
  let
    in
      flake-utils.lib.eachDefaultSystem (
        system: rec {
          packages = {
            trackpoint-clusters = nixpkgs.legacyPackages.${system}.callPackage ./package.nix { };
            default = packages.trackpoint-clusters;
          };
        }
      ) // rec { # not system dependent
        overlay = final: prev: {
          trackpoint-clusters = final.callPackage ./package.nix { };
        };
        nixosModules.default = { lib, pkgs, config, ... }:
          let cfg = config.services.trackpoint-clusters; in
          {
            options.services.trackpoint-clusters = {
              enable = lib.mkEnableOption "trackpoint-clusters";
              package = lib.mkOption {
                type = lib.types.package;
                default = self.packages.${pkgs.system}.trackpoint-clusters;
              };
            };

            config.systemd.user.services.trackpoint-clusters = lib.mkIf (cfg.enable) {
              Unit = {
                Description = "turns thinkpad trackpoint buttons into keyboard thumb clusters";
                After = [ "graphical-session-pre.target" ];
                PartOf = [ "graphical-session.target" ];
              };
              Service = {
                Type="simple";
                ExecStart="${cfg.package}/bin/trackpoint-clusters";
              };
              Install.WantedBy = [ "graphical-session.target" ];
            };
          };
        # I think the same module works for both nixos and home-manager
        hmModules.default = nixosModules.default;
      };
}
