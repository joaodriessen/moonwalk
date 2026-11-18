import "../.." as Legacy

// The first adapter is behaviorally identical to the existing generic
// settings page. Later platform adapters can replace this route only after
// their page-specific parity gates pass.
Legacy.SettingsView {
    shellRouteId: "adaptive-settings"
}
