function(target_embed_assets target_name)
    cmake_parse_arguments(ARG "" "OUT_DIR;NAMESPACE;BASE_DIR" "" ${ARGN})

    if(NOT ARG_OUT_DIR)
        set(ARG_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/embedded_assets")
    endif()
    if(NOT ARG_NAMESPACE)
        set(ARG_NAMESPACE "assets")
    endif()
    if(NOT ARG_BASE_DIR)
        set(ARG_BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()

    file(MAKE_DIRECTORY "${ARG_OUT_DIR}")

    set(HEADER_FILE "${ARG_OUT_DIR}/embedded_assets.h")
    set(SOURCE_FILE "${ARG_OUT_DIR}/embedded_assets.cpp")

    set(HEADER_CONTENT
"#pragma once

#include <cstddef>
#include <string_view>

namespace ${ARG_NAMESPACE} {

struct EmbeddedAsset {
    std::string_view relative_path;
    const unsigned char* data;
    std::size_t size;
};

struct AssetList {
    const EmbeddedAsset* items;
    std::size_t count;

    const EmbeddedAsset* begin() const noexcept { return items; }
    const EmbeddedAsset* end() const noexcept { return items + count; }
};

extern const EmbeddedAsset kAssets[];
extern const std::size_t kAssetCount;

AssetList all_assets();
const EmbeddedAsset* find_asset(std::string_view relative_path);

} // namespace ${ARG_NAMESPACE}
")

    set(SOURCE_CONTENT
"#include \"embedded_assets.h\"

namespace ${ARG_NAMESPACE} {

")

    set(TABLE_ENTRIES "")
    set(IDX 0)

    foreach(ASSET_PATH IN LISTS ARG_UNPARSED_ARGUMENTS)
        if(NOT EXISTS "${ASSET_PATH}")
            message(FATAL_ERROR "Embedded asset not found: ${ASSET_PATH}")
        endif()

        file(RELATIVE_PATH REL_PATH "${ARG_BASE_DIR}" "${ASSET_PATH}")
        file(TO_CMAKE_PATH "${REL_PATH}" REL_PATH_NORM)

        string(REGEX REPLACE "[^A-Za-z0-9_]" "_" SANITIZED "${REL_PATH_NORM}")
        set(VAR_NAME "asset_${IDX}_${SANITIZED}")
        math(EXPR IDX "${IDX} + 1")

        file(READ "${ASSET_PATH}" HEX_DATA HEX)
        string(LENGTH "${HEX_DATA}" HEX_LEN)
        math(EXPR BYTE_LEN "${HEX_LEN} / 2")

        if(BYTE_LEN GREATER 0)
            string(REGEX REPLACE "([0-9a-fA-F][0-9a-fA-F])" "0x\\1, " ARRAY_BODY "${HEX_DATA}")
            string(REGEX REPLACE ", $" "" ARRAY_BODY "${ARRAY_BODY}")
        else()
            set(ARRAY_BODY "")
        endif()

        string(APPEND SOURCE_CONTENT
"static const unsigned char ${VAR_NAME}_data[] = {
    ${ARRAY_BODY}
};
static constexpr std::size_t ${VAR_NAME}_size = ${BYTE_LEN};

")

        string(APPEND TABLE_ENTRIES
"    { \"${REL_PATH_NORM}\", ${VAR_NAME}_data, ${VAR_NAME}_size },\n")
    endforeach()

    string(APPEND SOURCE_CONTENT
"const EmbeddedAsset kAssets[] = {
${TABLE_ENTRIES}};

const std::size_t kAssetCount = sizeof(kAssets) / sizeof(kAssets[0]);

AssetList all_assets() {
    return {kAssets, kAssetCount};
}

const EmbeddedAsset* find_asset(std::string_view relative_path) {
    for (std::size_t i = 0; i < kAssetCount; ++i) {
        if (kAssets[i].relative_path == relative_path) {
            return &kAssets[i];
        }
    }
    return nullptr;
}

} // namespace ${ARG_NAMESPACE}
")

    file(WRITE "${HEADER_FILE}" "${HEADER_CONTENT}")
    file(WRITE "${SOURCE_FILE}" "${SOURCE_CONTENT}")

    target_sources("${target_name}" PRIVATE "${SOURCE_FILE}")
    target_include_directories("${target_name}" PRIVATE "${ARG_OUT_DIR}")
endfunction()