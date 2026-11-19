# Third-party notices

Keep the upstream copyright headers and license texts when distributing
Moonwalk. The table below covers notices installed from this source tree.
Review a built package separately for its additional libraries and runtime.

## Notices installed with the Linux client

Linux `make install` copies these files unchanged beneath
`$PREFIX/$DATADIR/licenses/moonwalk/` (normally `/usr/local/share/licenses/moonwalk/`,
or `/app/share/licenses/moonwalk/` in Flatpak). Each component gets its own directory
so files named `LICENSE` cannot overwrite each other. Keep submodules populated
with `git submodule update --init --recursive` before building.

| Component directory | Source notice | License text in that file |
| --- | --- | --- |
| `moonwalk` | [Moonwalk/Moonlight Qt](../LICENSE) | GPL version 3 |
| `lucide` | [Lucide icons](../app/res/lucide/LICENSE) | ISC and MIT for Feather-derived icons |
| `moonlight` | [moonlight-common-c](../moonlight-common-c/moonlight-common-c/LICENSE.txt) | GPL version 3 |
| `enet` | [ENet](../moonlight-common-c/moonlight-common-c/enet/LICENSE) | MIT |
| `nanors` | [NanoRS](../moonlight-common-c/moonlight-common-c/nanors/LICENSE) | MIT |
| `qmdnsengine` | [QMdnsEngine](../qmdnsengine/qmdnsengine/LICENSE.txt) | MIT |
| `h264bitstream` | [h264bitstream](../h264bitstream/h264bitstream/LICENSE) | LGPL version 2.1 |
| `controllerdb` | [SDL GameControllerDB](../app/SDL_GameControllerDB/LICENSE) | zlib-style notice |

This installation rule applies to current source builds. Older downloaded alpha
bundles may not contain these files. macOS/Windows client packaging is not covered
by this Linux rule.

## Review before distributing a build

Flatpak Builder collects license files from most modules in the
[manifest](../flatpak/com.starlight.Starlight.yaml), including FFmpeg, SDL,
libplacebo, dav1d, and QAdwaitaDecorations. The SDL2_ttf module also lists
the notices for its vendored FreeType and HarfBuzz sources explicitly. The
libdrm module includes the notice-bearing source and headers for libdrm and
libdrm_intel under the package’s license directory.

Run the [bundle verifier](releasing.md#build-and-test-the-candidate) to see the
license files that made it into a particular package. Review those files against
all packaged components and their exact sources. Qt and other runtime
components come from the selected runtime or system.

The application icon, screenshots and other artwork also need provenance review.
Check the rights to distribute the artwork and use the name separately. See the [release checklist](releasing.md).
