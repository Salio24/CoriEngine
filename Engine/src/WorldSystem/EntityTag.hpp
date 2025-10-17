#pragma once

/**
 * @brief Declares an empty components that is meant to be used as a tag.
 * @param tag Name of the tag, final components name will be tag+Tag.
 */
#define CORI_DECLARE_TAG(tag) struct tag##Tag { private: static constexpr bool bober{ false }; }