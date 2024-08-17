{
  description = "Ragnar Window Manager";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system: let
      pkgs = nixpkgs.legacyPackages.${system};

      libs = with pkgs; [
        libGL
        mesa
        libconfig
      ] ++ (with pkgs.xorg; [
        libX11
        libxcb
        xcbutil
        xcbutilwm
        xorgproto
        xcb-util-cursor
        xcbutilkeysyms
      ]);

    in {
      devShells.default = pkgs.mkShell {
        packages = with pkgs; [
          gnumake
          gcc
        ] ++ libs;
      };
    });
}
