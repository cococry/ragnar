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

      packages.ragnarwm = pkgs.stdenv.mkDerivation {
        name = "ragnarwm";
        src = ./.;

        makeFlags = [
          "CC=${pkgs.stdenv.cc}/bin/cc"
        ];

        buildInputs = libs;
        hardeningDisable = [ "format" "fortify" ];

        installPhase = ''
          runHook preInstall

          mkdir -p $out/bin
          mkdir -p $out/share/applications
          mkdir -p $out/share/xessions

	      cp -f ragnar $out/bin
	      cp -f ragnar.desktop $out/share/applications
	      cp -f ragnar.desktop $out/share/xsessions
	      cp -f ragnarstart $out/bin
	      chmod 755 $out/bin/ragnar

          runHook postInstall
        '';
      };

      packages.default = self.packages.${system}.ragnarwm;
    });
}
