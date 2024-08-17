{
  description = "Ragnar Window Manager";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-23.05";
    utils.url = "github:numtide/flake-utils";
  };

  outputs = {
    self,
    nixpkgs,
    flake-utils,
    ...
  }:
    flake-utils.lib.eachDefaultSystem (system: let
      pkgs = nixpkgs.legacyPackages.${system};

      libs = with pkgs; [
        fontconfig
      ] ++ (with pkgs.xorg; [
        libX11
        libXft
        libXcursor
        libXcomposite
      ]);

    in {
      devShells.default = pkgs.mkShell {
        packages = with pkgs; [
          gnumake
          gcc
          glibc
        ] ++ libs;
      };
    });
}
