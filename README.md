# NixFS

Access any Nix derivation through filesystem paths with NixFS.

NixFS is a FUSE-based filesystem that allows you to access any Nix derivation by simply accessing a corresponding path. This can be helpful for exploring the Nix store and easily accessing the derivations without having to explicitly build them first.

## How it works

```
/nixfs
├── expr
│   ├── b64
│   ├── str
│   └── urlenc
└── flake
    ├── b64
    ├── str
    └── urlenc
```

### Flakes

When you attempt to access /nixfs/flake/str/nixpkgs#hello or any subpaths of it, NixFS will automatically trigger a nix-build of nixpkgs#hello. The path will then appear as a symlink to the store path of the build result.

For flakes with / in their URL, you can use `/nixfs/flake/urlenc/<url encoded flake URL>` to access them.
You can also use `base64url` encoding (i.e. b64 with `+/` replaced by `-_`) at `/nixfs/flake/b64/<base64url encoded flake URL>`.

Example:
```
$ /nixfs/flake/str/nixpkgs#hello/bin/hello
Hello, world!

```

### Expressions

There is no easy way to build arbitrary expressions in the context of a flake right now. See [here](https://github.com/NixOS/nix/issues/5567). You can build and access nix expressions using the `/nixfs/expr` path. However, the build will be impure because of the above limitation.

Example:
```
$ /tmp/nixfs/expr/str/'(import <nixpkgs> { }).hello'/bin/hello
Hello, world!
```

See the usage section below for more flake and expr examples.

## Passing flags and args to nix build

You can pass flags to the nix build by appending them to the path just before the flake url or expression. Flags starting with a dash (`-`) should be added directly, while arguments not starting with a dash should be prefixed with a hash (`#`). For example:

```
/nixfs/flake/str/--optional-flag-starting-with-dash/#optional-flag-not-starting-with-dash/nixpkgs#hello
```

For example:

```
/nixfs/flake/str/--refresh/nixpkgs#hello
```

## Usage

```
nixfs [--debug] [<fuse mount options>] /mount/path
```

This will mount the NixFS filesystem to the specified mount path.

```
$ ls /tmp/nixfs/flake/urlenc/$(echo -n github:illustris/nixfs | jq -sRr @uri)/bin
mount.fuse.nixfs  mount.nixfs  nixfs
$ ls /tmp/nixfs/flake/b64/$(echo -n github:illustris/nixfs | base64 -w0 | tr '+/' '-_')/bin
mount.fuse.nixfs  mount.nixfs  nixfs
$ /nixfs/expr/str/'(import <nixpkgs> { }).python3.withPackages (p: [p.requests])'/bin/python -c 'import requests; print(requests.__version__)'
2.32.3
$ /nixfs/expr/b64/$(echo -n '(import <nixpkgs> { }).python3.withPackages (p: [p.requests])' | base64 -w0 | tr '+/' '-_')/bin/python -c 'import requests; print(requests.__version__)'
2.32.3
$ /nixfs/expr/urlenc/$(echo -n '(import <nixpkgs> { }).python3.withPackages (p: [p.requests])' | jq -sRr @uri)/bin/python -c 'import requests; print(requests.__version__)'
2.32.3
```

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
