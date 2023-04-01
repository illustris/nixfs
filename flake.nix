{
	description = "NixFS: Every derivation, everywhere";

	inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

	outputs = { self, nixpkgs }: with nixpkgs.lib; let
		archs = [
			"x86_64-linux"
			"aarch64-linux"
			"riscv64-linux"
		];
	in {
		packages = genAttrs archs (system: with nixpkgs.legacyPackages.${system}; rec {
			nixfs = stdenv.mkDerivation {
				pname = "nixfs";
				version = "0.1.0";
				src = ./nixfs;
				nativeBuildInputs = [
					cmake
					pkg-config
				];
				buildInputs = [
					fuse
					openssl
				];
			};
			default = nixfs;
		});
		devShells = genAttrs archs (system: with nixpkgs.legacyPackages.${system}; rec {
			default = mkShell {
				buildInputs = [ inotify-tools ];
				shellHook = "alias debug='bash ${./debugrun.sh}'";
			};
		});
		nixosModules.nixfs = { config, pkgs, lib, ... }: with lib; {
			options.services.nixfs = {
				enable = mkEnableOption "NixFS service";
				mountPath = mkOption {
					type = types.path;
					default = "/nixfs";
					description = "Path to mount the NixFS filesystem.";
				};
			};

			config = mkIf config.services.nixfs.enable {
				system.fsPackages = [ self.packages.${pkgs.system}.nixfs ];
				systemd = {
					services.nixfs = {
						wantedBy = [ "multi-user.target" ];
						path = [
							pkgs.nix
							self.packages.${pkgs.system}.nixfs
						];
						script = "nixfs ${config.services.nixfs.mountPath} -f";
					};
					tmpfiles.rules = [ "d ${config.services.nixfs.mountPath} 555 root root" ];
				};
			};
		};
		checks = genAttrs archs (system: with nixpkgs.legacyPackages.${system}; {
			default = nixosTest {
				name = "nixfs-test";
				nodes.n = {
					imports = [ self.nixosModules.nixfs ];
					services.nixfs.enable = true;
				};
				testScript = "n.succeed('tree /nixfs; false')";
			};
		});
	};
}
