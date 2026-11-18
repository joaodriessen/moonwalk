# Upstream provenance

Moonwalk is based on [Moonlight Qt](https://github.com/moonlight-stream/moonlight-qt).
The initial Moonwalk tree was prepared from the former Starlight development line.
Its nearest Moonlight Qt ancestor was
[`c0c4d605`](https://github.com/moonlight-stream/moonlight-qt/commit/c0c4d6056569bba40ac4458a3c225c05ff86df6d).
The upstream repository preserves the original commit history and contributor
credits. This repository carries the GPL license and the upstream copyright and
license notices. The pinned dependency sources are listed in
[.gitmodules](../.gitmodules) and the
[Flatpak recipe](../flatpak/com.starlight.Starlight.yaml).

For upstream fixes, fetch Moonlight Qt separately, review the individual
changes, and apply the relevant patch on a Moonwalk branch. Preserve upstream
attribution and link to its original commit. Use a public project contact or
GitHub no-reply address in new Git metadata. Compare larger updates against the
upstream base and re-run the tests for the parts you touch.
