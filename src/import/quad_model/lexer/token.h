#pragma once

namespace Import { namespace QuadModel {
// IMPORTANT: must assign consecutive numbers for the automata to work properly
enum Token {
    // END_OF_FILE must be 0
    END_OF_FILE = 0,
    COLON = 1,
    L_ARROW = 2,
    R_ARROW = 3,
    K_TRUE = 4,
    K_FALSE = 5,
    STRING = 6,
    TYPED_STRING = 7,
    IDENTIFIER = 8,
    ANON = 9,
    INTEGER = 10,
    FLOAT = 11,
    WHITESPACE = 12,
    ENDLINE = 13,
    L_BRACKET = 14,
    R_BRACKET = 15,
    COMMA = 16,
    UNRECOGNIZED = 17,
    HEX_ID = 18,
    UUID_ID = 19,
    TOTAL_TOKENS = 20
};

}} // namespace Import::QuadModel
