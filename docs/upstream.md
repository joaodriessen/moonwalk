# Upstream source

Moonwalk started as Starlight, a fork of
[Moonlight Qt](https://github.com/moonlight-stream/moonlight-qt). The nearest
Moonlight Qt ancestor of the original Moonwalk snapshot was
[`c0c4d605`](https://github.com/moonlight-stream/moonlight-qt/commit/c0c4d6056569bba40ac4458a3c225c05ff86df6d).
Moonlight Qt's repository contains its original history and contributor
credits. Moonwalk carries the GPL license and the upstream copyright and
license notices. Find pinned dependencies in [.gitmodules](../.gitmodules)
and the [Flatpak recipe](../flatpak/com.starlight.Starlight.yaml).

To bring in an upstream fix, fetch Moonlight Qt separately and review the
relevant diff. Apply the patch on a Moonwalk branch with its attribution and
original commit link. Compare larger updates against the shared upstream base
and rerun the affected checks.
