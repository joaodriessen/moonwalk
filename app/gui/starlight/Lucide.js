.pragma library

// A Lucide glyph from res/lucide in a given colour, drawn by the C++
// LucideIconProvider. Use it as an Image source or a control's icon.source;
// pass the colour the glyph should be in its current state.
function url(name, color) {
    // QML formats colours as #RRGGBB, or #AARRGGBB when not opaque.
    var hex = String(color).replace("#", "")
    if (hex.length === 6)
        hex = "ff" + hex
    return "image://lucide/" + name + "/" + hex
}
