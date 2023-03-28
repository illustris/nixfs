# NixFS

Access any derivation through filesystem paths

## How it works

```
/nixfs
└── flake
    ├── b64
    └── str
```

Attempting to access `/nixfs/flake/str/nixpkgs#hello` or any subpaths of it will trigger a nix-build
of nixpkgs#hello, and make the path appear as a symlink to the store path of the build result.

`/nixfs/flake/b64/<base64 encoded flake URL>` can be used to access flakes with `/` in the URL.

## Usage

```
nixfs [--debug] [<fuse mount options>] /mount/path
```

## NixOS module

`flake.nix`:

```
{
	inputs.nixfs.url = "github:illustris/flake";
	
	outputs = {nixpkgs, nixfs, ...}: {
		nixosConfigurations.my_machine = {
			imports = [ nixfs.nixosModules.nixfs ];
			services.nixfs.enable = true;
		};
	};
}
```
