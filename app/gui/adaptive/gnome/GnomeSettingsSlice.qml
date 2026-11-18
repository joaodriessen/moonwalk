import "../generic" as Generic

// Preserve the characterized settings workflow while GNOME semantic controls
// are gated independently. This keeps the legacy route immediately reversible.
Generic.GenericSettingsSlice {
    readonly property string platformAdapterName: "gnome"
}
