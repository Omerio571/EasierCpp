#pragma once

#include <filesystem>

namespace cpplivelab
{
// Copies supported metadata in atomic units. Originals and conflicting new data
// remain untouched. Build/download caches are deliberately excluded.
void migrate_state_metadata(
    const std::filesystem::path& legacy, const std::filesystem::path& destination);
} // namespace cpplivelab
