# NixFS

Access any Nix derivation through filesystem paths with NixFS.

NixFS is a FUSE-based filesystem that allows you to access any Nix derivation by simply accessing a corresponding path. This can be helpful for exploring the Nix store and easily accessing the derivations without having to explicitly build them first.

## How it works

```
/nixfs
└── flake
    ├── b64
    └── str
```

When you attempt to access /nixfs/flake/str/nixpkgs#hello or any subpaths of it, NixFS will automatically trigger a nix-build of nixpkgs#hello. The path will then appear as a symlink to the store path of the build result.

For flakes with / in their URL, you can use `/nixfs/flake/b64/<base64 encoded flake URL>` to access them.

## Usage

```
nixfs [--debug] [<fuse mount options>] /mount/path
```

This will mount the NixFS filesystem to the specified mount path.

## NixOS module

You can also integrate NixFS into your NixOS configuration as a module.

To do this, add the following to your flake.nix:

```
{
	inputs.nixfs.url = "github:illustris/nixfs";
	
	outputs = {nixpkgs, nixfs, ...}: {
		nixosConfigurations.my_machine = {
			imports = [ nixfs.nixosModules.nixfs ];
			services.nixfs.enable = true;
		};
	};
}
```

This will enable the NixFS service on your NixOS machine, and the filesystem will be automatically mounted on startup.

## Contributing

If you'd like to contribute to the development of NixFS, please feel free to submit issues or pull requests on the GitHub repository.
