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
				version = "0.2.1";
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
			# to speed up nixos test
			inherit hello;
			updateRelease = writeScriptBin "update-release" (builtins.readFile ./utils/update-release.sh);
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
				evalUser = mkOption {
					type = types.str;
					default = "nobody";
					description = "User to run nix evaluations as. Limits file access during evaluation.";
				};
			};

			config = mkIf config.services.nixfs.enable (let
				cfg = config.services.nixfs;
			in {
				system.fsPackages = [ self.packages.${pkgs.system}.nixfs ];
				systemd.services.nixfs = {
					description = "NixFS FUSE filesystem";
					wantedBy = [ "multi-user.target" ];
					serviceConfig = {
						ExecStartPre = "+${pkgs.writeShellScript "nixfs-setup" ''
							${pkgs.coreutils}/bin/install -d -o nixfs -g nixfs ${cfg.mountPath}
							${pkgs.coreutils}/bin/install -d -o ${cfg.evalUser} -g nogroup /var/cache/nixfs-eval
						''}";
						ExecStart = "${lib.getExe self.packages.${pkgs.system}.nixfs} -f -o allow_other --eval-user=${cfg.evalUser} ${cfg.mountPath}";
						ExecStop = "${pkgs.fuse}/bin/fusermount -u ${cfg.mountPath}";
						User = "nixfs";
						Group = "nixfs";
						AmbientCapabilities = [ "CAP_SETUID" "CAP_SETGID" ];
						Environment = [
							"PATH=${lib.makeBinPath [ pkgs.nix ]}:/run/current-system/sw/bin"
							"XDG_CACHE_HOME=/var/cache/nixfs"
							"NIXFS_EVAL_CACHE=/var/cache/nixfs-eval"
						];
						CacheDirectory = "nixfs";
					};
				};
				users.users.nixfs = {
					isSystemUser = true;
					group = "nixfs";
				};
				users.groups.nixfs = {};
				environment.etc."fuse.conf".text = "user_allow_other\n";
				nix.settings.allowed-users = [ cfg.evalUser ];
			});
		};
		# module for [system-manager](https://github.com/numtide/system-manager)
		systemModules.nixfs = { config, pkgs, lib, ... }: {
			options.services.nixfs = {
				enable = mkEnableOption "NixFS service";
				mountPath = mkOption {
					type = types.path;
					default = "/nixfs";
					description = "Path to mount the NixFS filesystem.";
				};
			};
			config = mkIf config.services.nixfs.enable {
				systemd = {
					mounts = [{
						what = "none";
						where = config.services.nixfs.mountPath;
						type = "fuse.nixfs";
						options = "allow_other";
						wantedBy = [ "system-manager.target" ];
					}];
					tmpfiles.rules = map (x: "L+ /usr/bin/${x}nixfs - - - - ${lib.getExe self.packages.${pkgs.system}.default}") [
						"mount." "mount.fuse." ""
					];
				};
			};
		};
		checks = genAttrs archs (system: with nixpkgs.legacyPackages.${system}; {
			default = nixosTest {
				name = "nixfs-test";
				nodes.n = { pkgs, ... }: {
					imports = [ self.nixosModules.nixfs ];
					services.nixfs.enable = true;
					# ensure the build result nixfs will access is already present in the VM
					system.extraDependencies = [
						self.inputs.nixpkgs
						self
						self.packages.${system}.hello
					];
					# useful for debugging
					systemd.services.execsnoop = {
						script = "execsnoop";
						path = with pkgs; [
							bcc
							gnutar
							kmod
							xz
						];
						wantedBy = [ "multi-user.target" ];
					};
				};
				# use the store path of the nixpkgs flake to avoid downloading from the internet
				testScript = concatStringsSep "\n" [
					"n.wait_for_unit('execsnoop.service')"
					"assert 'Hello, world!' in n.succeed('set -x; /nixfs/flake/b64/--offline/$(printf ${self}#hello | base64 -w0)/bin/hello')"
				];
			};
		});
	};
}
