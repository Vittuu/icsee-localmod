# Build and reproduction scope

The on-device build was produced in Linux/WSL with an ARM little-endian static musl toolchain and QEMU user emulation. Dropbear source: 2026.94; musl: 1.2.5. Dependency archive SHA-256 values recorded during development:

```
dropbear-2026.94.tar.bz2 e098034a843699200c8c977a991fff73159735bf795d5f72ef672c41a6b1ae81
musl-1.2.5.tar.gz        a9a118bbe84d8764da0ea0d28b3ab3fae8477fc7e4085d90102b8596fc7c75e4
```

Obtain sources from their upstream projects and verify their signatures/checksums independently. Archives and prebuilt executables are not bundled. cJSON's license is preserved in its vendored source and `licenses/`.

For the panel, build the matching LibTomCrypt library from Dropbear with the same compiler/ABI, then:

```sh
export CC=/path/to/arm-musl/bin/musl-gcc
export DROPBEAR_SOURCE=/path/to/dropbear-2026.94
sh build.sh
python3 tests/test_web.py
gcc -D_GNU_SOURCE -Isrc tests/test_readonly_root.c -o build/test_readonly_root
./build/test_readonly_root
```

`tests/test_web.py` runs the ARM binary with `qemu-arm` against a loopback fake DVRIP backend. Its hardcoded `tester` / `test-only-password` pair belongs only to that simulation. It does not connect to the camera.

For SSH, review `ssh/dropbear-localoptions.h` and run `python3 ssh/patch_dropbear.py /path/to/dropbear-2026.94` on a clean source tree. Configure/build static Dropbear with your toolchain and these local options. The patch is specific to the tested stock root ownership and adds diagnostic logging in volatile RAM. It must not be applied blindly to other filesystems/owners.

Runtime layout from the tested installation:

```
/usr/lib/localmod/dropbearmulti       # generated, not distributed
/mnt/custom/localmod/localmod-web    # generated, not distributed
/mnt/custom/localmod/local-routes.sh
/mnt/custom/extapp.sh                # stock hook on tested firmware
/mnt/mtd/Config/localmod/host_key     # private, generated per device
/mnt/mtd/Config/localmod/authorized_keys
```

These paths describe the result. They are not commands to write flash. First validate executable/kernel ABI, mounts, native boot hook, available image space and recovery method on your own unit. The stock root can have vendor metadata that generic filesystem rebuilding loses.
