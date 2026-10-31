#include "process/output_decoder.hpp"
#include "process/windows_support.hpp"

namespace cpplivelab
{
namespace
{
size_t complete_prefix_size(const std::string& bytes)
{
    if (bytes.empty())
    {
        return 0;
    }

    // Walk back over UTF-8 continuation bytes (10xxxxxx) to their leading byte.
    size_t sequence_start = bytes.size() - 1;

    while (sequence_start > 0 && (static_cast<unsigned char>(bytes[sequence_start]) & 0xC0) == 0x80)
    {
        --sequence_start;
    }

    const auto leading_byte = static_cast<unsigned char>(bytes[sequence_start]);
    size_t expected_bytes = 1;

    if (leading_byte >= 0xC2 && leading_byte <= 0xDF)
    {
        expected_bytes = 2;
    }
    else if (leading_byte >= 0xE0 && leading_byte <= 0xEF)
    {
        expected_bytes = 3;
    }
    else if (leading_byte >= 0xF0 && leading_byte <= 0xF4)
    {
        expected_bytes = 4;
    }

    if (bytes.size() - sequence_start < expected_bytes)
    {
        return sequence_start;
    }

    return bytes.size();
}
} // namespace

std::wstring decode_console_text(std::string_view bytes)
{
    if (bytes.empty())
    {
        return {};
    }

    UINT code_page = CP_UTF8;
    int character_count = MultiByteToWideChar(
        code_page, MB_ERR_INVALID_CHARS, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);

    if (character_count == 0)
    {
        code_page = CP_ACP;
        character_count = MultiByteToWideChar(
            code_page, 0, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);
    }

    std::wstring decoded_text(character_count, L'\0');
    MultiByteToWideChar(code_page,
        0,
        bytes.data(),
        static_cast<int>(bytes.size()),
        decoded_text.data(),
        character_count);

    return decoded_text;
}

std::wstring ConsoleOutputDecoder::append(std::string_view bytes, bool final_chunk)
{
    pending_bytes_.append(bytes);

    // At EOF/Stop there is no next read. Flush any remaining bytes through the
    // existing legacy-code-page fallback rather than silently discarding them.
    const size_t complete_bytes =
        final_chunk ? pending_bytes_.size() : complete_prefix_size(pending_bytes_);
    const auto decoded_text =
        decode_console_text(std::string_view(pending_bytes_).substr(0, complete_bytes));

    pending_bytes_.erase(0, complete_bytes);
    return decoded_text;
}

void ConsoleOutputDecoder::reset()
{
    pending_bytes_.clear();
}
} // namespace cpplivelab
