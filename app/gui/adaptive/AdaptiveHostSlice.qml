import ".." as Legacy

// The adaptive route is deliberately a direct PcView specialization. Keeping
// it as the StackView item preserves the established activation, pairing,
// polling, focus, persistence, and destruction ordering without a second root
// or workflow owner.
Legacy.PcView {
    shellRouteId: "adaptive-host"
}
