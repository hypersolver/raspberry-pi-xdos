# PXLAYOUT source and build

The Release source archive contains the generated sources compiled for the tested ARM32 image, the Circle and LVGL changes from that build, and the player and XAPP source files. It excludes build objects, firmware and device configuration.

| Dependency | Revision |
| --- | --- |
| [Circle](https://github.com/rsta2/circle) | 7a81e1b480fe7d19fc1fc695b89bff0a0848f44b |
| [LVGL](https://github.com/lvgl/lvgl) | c016f72d4c125098287be5e83c0f1abed4706ee5 |
| [Pi1MHz](https://github.com/dp111/Pi1MHz) | 1f6954eee2d1f72eab7e187e0a2d941763a54345 |
| [minimp4](https://github.com/lieff/minimp4) | 65fab0a062c3cd5befd98ecbaa890108cd45c226 |

Circle and the Pi1MHz decoder sources retain their GPL notices. The minimp4 single header includes its license information. See the included notices and [repository license information](../../LICENSE.md).

Use the ARM32 WSL toolchain described in [OPERATIONS_ZH.md](OPERATIONS_ZH.md). For the source snapshot, check out Circle and its LVGL submodule at the revisions above, apply circle.patch and lvgl.patch to their respective trees, and copy the included Config.mk to the Circle root. Place the included xdos directory under Circle's sample directory and build with XDOS_BUFFER_ROWS=128. Shared prerequisites include Circle core, input, FAT/SD, network, USB, LVGL and the VideoCore interface libraries. The recipe in tools/build-wsl.sh shows their build order.

For a generated 128-row build tree, tools/vc4/build_player.sh builds the player image and binds the four XAPPs to its kernel fingerprint. Invoke it in WSL with the build directory, the directory containing the pinned Pi1MHz and minimp4 checkouts, and the staging prefix Q9. Its output VC4PLAY.IMG was published as PXLAYOUT.IMG. Use the paired XAPPs emitted by the same build, since compiler and link changes can change the fingerprint.
