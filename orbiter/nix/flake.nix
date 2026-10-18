{
  description = "Local Orbiter/NASSP native port build tools";
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
  outputs = { self, nixpkgs }:
    let
      systems = [ "aarch64-darwin" "x86_64-darwin" "aarch64-linux" "x86_64-linux" ];
    in {
      devShells = nixpkgs.lib.genAttrs systems (system:
        let pkgs = import nixpkgs { inherit system; };
        in {
          default = pkgs.mkShellNoCC {
            packages = with pkgs; [ cmake ninja pkg-config python3 cargo rustc ]
              ++ lib.optionals stdenv.hostPlatform.isLinux [ clang zlib ];
            # Use Apple's compiler and SDK on macOS. Nix only supplies tools.
            shellHook = nixpkgs.lib.optionalString pkgs.stdenv.hostPlatform.isDarwin ''
              export CC="$(/usr/bin/xcrun --find clang)"
              export CXX="$(/usr/bin/xcrun --find clang++)"
            '';
          };
        });
    };
}
