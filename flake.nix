{
  description = "turns thinkpad trackpoint buttons into keyboard thumb clusters";
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };
  outputs = { self, nixpkgs, flake-utils }:
    let module = pkg: {
          systemd.user.services.trackpoint-clusters = {
            Unit = {
              Description = "turns thinkpad trackpoint buttons into keyboard thumb clusters";
              After = [ "graphical-session-pre.target" ];
              PartOf = [ "graphical-session.target" ];
            };
            Service = {
              Type="simple";
              ExecStart="${pkg}/bin/trackpoint-clusters";
            };
            Install.WantedBy = [ "graphical-session.target" ];
          };
        };
    in
      flake-utils.lib.eachDefaultSystem (
        system: rec {
          packages.trackpoint-clusters = nixpkgs.legacyPackages.${system}.callPackage ./package.nix { };
          defaultPackage = packages.trackpoint-clusters;

          # can't have both this and the part below.
          # TODO: fit this into the flake structure somehow
          # nixosModule = module packages.trackpoint-clusters;
          # hmModule = nixosModule;
        }
      ) // rec { # not system dependent
        overlay = final: prev: {
          trackpoint-clusters = final.callPackage ./package.nix { };
        };
        nixosModule = { pkgs, ... }: module pkgs.trackpoint-clusters;
        hmModule = nixosModule;
      };
}
