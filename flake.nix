{
  description = "A smart proxy for MUD/MOO connections.";

  inputs = {
    # NixOS official package source, using the nixos-24.11 branch here
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-24.05";
  };


  outputs = {
    self,
    nixpkgs,
  }: let
    supportedSystems = ["x86_64-linux"];
    forAllSystems = nixpkgs.lib.genAttrs supportedSystems;
  in {
    devShells = forAllSystems (system: let
      pkgs = import nixpkgs {
        inherit system;
      };
    in {
      default = pkgs.mkShell {
        buildInputs = with pkgs; [gcc gnumake];
        shellHook = ''
          echo "cc"
          which cc
          cc --version
          echo "make"
          which make
          make --version
        '';
      };
    });

    packages = forAllSystems (system: let
      pkgs = import nixpkgs {
        inherit system;
      };
    in {
      default = pkgs.stdenv.mkDerivation {
        name = "mooproxy";
        src = ./.;
        # buildInputs = with pkgs; [gcc openssl libxcrypt-legacy git-crypt gnumake pkg-config];
        buildInputs = with pkgs; [gcc libxcrypt-legacy];
        buildPhase = ''
          # gcc mooproxy.o misc.o config.o daemon.o world.o network.o command.o mcp.o log.o accessor.o timer.o resolve.o crypt.o line.o panic.o recall.o -Wall -lcrypt -o mooproxy 
          # gcc mooproxy.c -Wall -lcrypt -o mooproxy 
          make
        '';
        installPhase = ''
          install -D -t $out/bin mooproxy
        '';
      };
    });
  };
}
