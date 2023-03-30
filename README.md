# NixFS

WIP

Mount:
```
mkdir -p /tmp/nixfs && nix run github:illustris/nixfs -- /tmp/nixfs --debug -f -s
```

Access:
```
$ /tmp/nixfs/flake/str/nixpkgs#hello/bin/hello
Hello, world!
```
