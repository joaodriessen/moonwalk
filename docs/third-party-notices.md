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

The [Flatpak manifest](../flatpak/com.starlight.Starlight.yaml) also selects libraries
such as FFmpeg, SDL, libplacebo, dav1d and desktop integration plugins. Qt and other
components come from the selected runtime or system. For each actual candidate,
record the exact sources, build options and installed notices, including nested
dependencies; the list above does not cover them all. Source availability and
distribution obligations need review against that candidate.

The application icon, screenshots and other artwork also need provenance review.
Check the rights to distribute the artwork and use the name separately. See the [release checklist](releasing.md).
