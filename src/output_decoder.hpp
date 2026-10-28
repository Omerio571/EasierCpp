#pragma once

#include <string>
#include <string_view>

namespace cpplivelab
{
// Decode a complete diagnostic message. Older tools may emit the Windows ANSI code
// page, so keep that fallback when the byte sequence is not valid UTF-8.
std::wstring decode_console_text(std::string_view bytes);

// Pipe reads have arbitrary byte boundaries, not character boundaries. Retain an
// incomplete UTF-8 suffix until the next read instead of displaying broken characters.
class ConsoleOutputDecoder
{
public:
    std::wstring append(std::string_view bytes, bool final_chunk = false);
    void reset();

private:
    std::string pending_bytes_;
};
} // namespace cpplivelab
