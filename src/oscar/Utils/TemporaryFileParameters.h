#pragma once

#include <filesystem>
#include <ios>
#include <optional>
#include <string_view>

namespace osc
{
    // parameters for constructing a `TemporaryFile`
    //
    // designed for designated initializer compatibility: `TemporaryFile tmpfile({ .suffix = ".obj" });`
    struct TemporaryFileParameters final {

        // a prefix that will be prepended to the dynamic portion of the temporary file name (e.g. `${prefix}XXXXXX${suffix}`)
        std::string_view prefix = {};

        // a suffix that will be appended to the dynamic portion of the temporary file name (e.g. `${prefix}XXXXXX${suffix}`)
        std::string_view suffix = {};

        // TODO: the mode that the temporary file should initially be opened in
        // std::ios_base::openmode mode = std::ios_base::in | std::ios_base::out | std::ios_base::binary | std::ios_base::app;

        // TODO: the directory in which the temporary file should be made, or `std::nullopt`, which means "use the system default"
        // std::optional<std::filesystem::path> dir = std::nullopt;
    };
}
