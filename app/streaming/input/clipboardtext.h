#pragma once

#include <cstddef>

namespace ClipboardText {

constexpr std::size_t MaxBytes = 1024 * 1024;
enum class Status { Ready, Empty, TooLarge };
struct Result { Status status; std::size_t length; };

// SDL owns a null-terminated UTF-8 buffer. Bound work before compacting CRLF
// in place; a single pass preserves lone CRs and all other UTF-8 bytes.
inline Result prepare(char* text)
{
    if (!text || !*text) return {Status::Empty, 0};
    std::size_t length = 0;
    while (length <= MaxBytes && text[length]) ++length;
    if (length > MaxBytes) return {Status::TooLarge, 0};

    std::size_t output = 0;
    for (std::size_t input = 0; input < length; ++input) {
        if (text[input] == '\r' && input + 1 < length && text[input + 1] == '\n') continue;
        text[output++] = text[input];
    }
    text[output] = '\0';
    return {Status::Ready, output};
}

}
