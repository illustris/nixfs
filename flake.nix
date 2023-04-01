{
	description = "NixFS: Every derivation, everywhere";

	inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

	outputs = { self, nixpkgs }: with nixpkgs.lib; {
		packages = genAttrs [
			"x86_64-linux"
			"aarch64-linux"
			"riscv64-linux"
		] (system: let
			pkgs = nixpkgs.legacyPackages.${system};
		in rec {
			nixfs = pkgs.stdenv.mkDerivation {
				pname = "nixfs";
				version = "0.1.0";
				src = ./nixfs;
				nativeBuildInputs = with pkgs; [
					cmake
					pkg-config
				];
				buildInputs = with pkgs; [
					fuse
					openssl
				];
			};
			default = nixfs;
		});
		devShells = genAttrs [
			"x86_64-linux"
			"aarch64-linux"
			"riscv64-linux"
		] (system: with nixpkgs.legacyPackages.${system}; rec {
			default = pkgs.mkShell {
				buildInputs = with pkgs; [ inotify-tools ];
				shellHook = "alias debug='bash ${./debugrun.sh}'";
			};
		});
	};
}
