#pragma once

#include "graph_models/common/conversions.h" // IWYU pragma: export
#include "graph_models/inliner.h"
#include "system/string_manager.h"
#include "system/tmp_manager.h"
#include "third_party/dragonbox/dragonbox_to_chars.h"

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>

namespace MQL { namespace Conversions {
using namespace Common::Conversions;

// The order, int < flt < inv is important
constexpr uint8_t OPTYPE_INTEGER = 0x01;
constexpr uint8_t OPTYPE_FLOAT = 0x02;
constexpr uint8_t OPTYPE_INVALID = 0x03;

inline uint64_t unpack_anon(ObjectId oid)
{
    return oid.get_value();
}

inline uint64_t unpack_edge(ObjectId oid)
{
    return oid.get_value();
}

ObjectId get_key_id(const std::string& str);
ObjectId get_node_label_id(const std::string& str);
ObjectId get_edge_label_id(const std::string& str);

std::string get_key(ObjectId);
std::string get_node_label(ObjectId);
std::string get_edge_label(ObjectId);

// Compress a UUID string (36 chars, lowercase, with dashes) into 16 bytes.
// out must have space for at least 16 bytes.
inline void pack_uuid_bytes(const char* uuid, char* out)
{
    int out_pos = 0;
    int hex_idx = 0;
    char pair[3] = { 0, 0, '\0' };
    for (int i = 0; i < 36; i++) {
        if (uuid[i] == '-') {
            continue;
        }
        pair[hex_idx % 2] = uuid[i];
        if (hex_idx % 2 == 1) {
            out[out_pos++] = static_cast<char>(static_cast<unsigned char>(strtol(pair, nullptr, 16)));
        }
        hex_idx++;
    }
}

// Decompress 16 UUID bytes into a 36-char lowercase UUID string (no null terminator written).
// out must have space for at least 36 chars.
inline void unpack_uuid_bytes(const char* bytes, char* out)
{
    static constexpr int group_sizes[] = { 4, 2, 2, 2, 6 };
    int byte_pos = 0;
    int out_pos = 0;
    for (int g = 0; g < 5; g++) {
        if (g > 0) {
            out[out_pos++] = '-';
        }
        for (int j = 0; j < group_sizes[g]; j++) {
            snprintf(out + out_pos, 3, "%02x", static_cast<unsigned char>(bytes[byte_pos++]));
            out_pos += 2;
        }
    }
}

// Pack a hex named node from a "0x..." string (case-insensitive).
// Supports up to 510 hex digits (255 bytes). Values up to 7 bytes are inlined.
// Leading zero bytes are stripped so "0x0032" and "0x32" produce the same ObjectId.
inline ObjectId pack_named_node_hex(const char* str, size_t len)
{
    const char* hex = str + 2; // skip "0x" or "0X"
    const size_t hex_len = len - 2;

    if (hex_len == 0 || hex_len > 510) {
        return ObjectId::get_null();
    }

    // Compress hex chars to binary bytes (up to 255 bytes)
    char bytes[256];
    size_t num_bytes = 0;

    size_t i = 0;
    if (hex_len % 2 == 1) {
        char c = static_cast<char>(tolower(static_cast<unsigned char>(hex[0])));
        bytes[num_bytes++] = static_cast<char>((c >= 'a') ? (c - 'a' + 10) : (c - '0'));
        i = 1;
    }
    for (; i < hex_len; i += 2) {
        char hi = static_cast<char>(tolower(static_cast<unsigned char>(hex[i])));
        char lo = static_cast<char>(tolower(static_cast<unsigned char>(hex[i + 1])));
        uint8_t hi_val = (hi >= 'a') ? (hi - 'a' + 10) : (hi - '0');
        uint8_t lo_val = (lo >= 'a') ? (lo - 'a' + 10) : (lo - '0');
        bytes[num_bytes++] = static_cast<char>((hi_val << 4) | lo_val);
    }

    // Strip leading zero bytes; keep at least one byte (for "0x0" → [0x00])
    size_t start = 0;
    while (start < num_bytes - 1 && bytes[start] == 0) {
        start++;
    }
    const char* data = bytes + start;
    num_bytes -= start;

    if (num_bytes <= 7) {
        uint64_t value = 0;
        for (size_t j = 0; j < num_bytes; j++) {
            value = (value << 8) | static_cast<unsigned char>(data[j]);
        }
        return ObjectId(ObjectId::MASK_NAMED_NODE_HEX_INL | value);
    }

    const auto str_id = string_manager.get_bytes_id(data, num_bytes);
    if (str_id != ObjectId::MASK_NOT_FOUND) {
        return ObjectId(ObjectId::MASK_NAMED_NODE_HEX_EXT | str_id);
    }
    return ObjectId(ObjectId::MASK_NAMED_NODE_HEX_TMP | tmp_manager.get_bytes_id(data, num_bytes));
}

// Unpack a hex named node back to its "0x..." string representation (lowercase).
inline std::string unpack_named_node_hex(ObjectId oid)
{
    switch (oid.type()) {
    case ObjectType::NamedNodeHexInl: {
        char buf[20]; // "0x" + up to 14 hex chars + null
        uint64_t value = oid.get_value();
        int len = snprintf(buf, sizeof(buf), "0x%llx", static_cast<unsigned long long>(value));
        return std::string(buf, len);
    }
    case ObjectType::NamedNodeHexExt:
    case ObjectType::NamedNodeHexTmp: {
        char bytes[256]; // up to 255 bytes
        uint64_t external_id = oid.id & ObjectId::MASK_EXTERNAL_ID;
        size_t num_bytes;
        if (oid.type() == ObjectType::NamedNodeHexExt) {
            num_bytes = string_manager.print_to_buffer(bytes, external_id);
        } else {
            num_bytes = tmp_manager.print_to_buffer(bytes, external_id);
        }
        // "0x" + first byte without leading zero + remaining bytes with leading zeros
        std::string result = "0x";
        char buf[3];
        snprintf(buf, sizeof(buf), "%x", static_cast<unsigned char>(bytes[0]));
        result += buf;
        for (size_t i = 1; i < num_bytes; i++) {
            snprintf(buf, sizeof(buf), "%02x", static_cast<unsigned char>(bytes[i]));
            result += buf;
        }
        return result;
    }
    default:
        throw LogicException("Called unpack_named_node_hex with incorrect ObjectId type");
    }
}

// Pack a UUID named node from a 36-char UUID string (case-insensitive).
inline ObjectId pack_named_node_uuid(const char* str, size_t /*len*/)
{
    // Normalize to lowercase and compress to 16 bytes
    char normalized[36];
    for (int i = 0; i < 36; i++) {
        normalized[i] = static_cast<char>(tolower(static_cast<unsigned char>(str[i])));
    }
    char bytes[16];
    pack_uuid_bytes(normalized, bytes);
    const auto str_id = string_manager.get_bytes_id(bytes, 16);
    if (str_id != ObjectId::MASK_NOT_FOUND) {
        return ObjectId(ObjectId::MASK_NAMED_NODE_UUID_EXT | str_id);
    }
    return ObjectId(ObjectId::MASK_NAMED_NODE_UUID_TMP | tmp_manager.get_bytes_id(bytes, 16));
}

// Unpack a UUID named node back to its lowercase UUID string representation.
inline std::string unpack_named_node_uuid(ObjectId oid)
{
    char bytes[16];
    uint64_t external_id = oid.id & ObjectId::MASK_EXTERNAL_ID;
    switch (oid.type()) {
    case ObjectType::NamedNodeUuidExt:
        string_manager.print_to_buffer(bytes, external_id);
        break;
    case ObjectType::NamedNodeUuidTmp:
        tmp_manager.print_to_buffer(bytes, external_id);
        break;
    default:
        throw LogicException("Called unpack_named_node_uuid with incorrect ObjectId type");
    }
    char out[36];
    unpack_uuid_bytes(bytes, out);
    return std::string(out, 36);
}

inline std::string unpack_named_node(ObjectId oid)
{
    switch (oid.type()) {
    case ObjectType::NamedNodeInl: {
        return Inliner::get_string_inlined<ObjectId::NAMED_NODE_INLINE_BYTES>(oid.get_value());
    }
    case ObjectType::NamedNodeExt: {
        std::stringstream ss;
        const uint64_t external_id = oid.id & ObjectId::MASK_EXTERNAL_ID;
        string_manager.print(ss, external_id);
        return ss.str();
    }
    case ObjectType::NamedNodeTmp: {
        std::stringstream ss;
        const uint64_t external_id = oid.id & ObjectId::MASK_EXTERNAL_ID;
        tmp_manager.print_str(ss, external_id);
        return ss.str();
    }
    case ObjectType::NamedNodeHexInl:
    case ObjectType::NamedNodeHexExt:
    case ObjectType::NamedNodeHexTmp:
        return unpack_named_node_hex(oid);
    case ObjectType::NamedNodeUuidExt:
    case ObjectType::NamedNodeUuidTmp:
        return unpack_named_node_uuid(oid);
    default: {
        throw LogicException(
            "Called unpack_named_node with incorrect ObjectId type, this should never happen"
        );
    }
    }
}

inline ObjectId pack_named_node(const std::string& str)
{
    uint64_t oid;
    if (str.size() <= ObjectId::NAMED_NODE_INLINE_BYTES) {
        oid = Inliner::inline_string(str.c_str()) | ObjectId::MASK_NAMED_NODE_INL;
    } else {
        const auto str_id = string_manager.get_str_id(str);
        if (str_id != ObjectId::MASK_NOT_FOUND) {
            oid = ObjectId::MASK_NAMED_NODE_EXT | str_id;
        } else {
            oid = ObjectId::MASK_NAMED_NODE_TMP | tmp_manager.get_str_id(str);
        }
    }
    return ObjectId(oid);
}

inline ObjectId pack_edge(uint64_t edge_id)
{
    return ObjectId(ObjectId::MASK_DIRECTED_EDGE | edge_id);
}

inline ObjectId pack_anon_tmp(uint64_t anon_id)
{
    return ObjectId(ObjectId::MASK_ANON_TMP | anon_id);
}

inline DateTime unpack_datetime(ObjectId oid)
{
    return DateTime(oid);
}

inline float to_float(ObjectId oid)
{
    switch (oid.subtype()) {
    case ObjectSubType::Int:
        return unpack_int(oid);
    case ObjectSubType::Float:
        return unpack_float(oid);
    case ObjectSubType::Double:
        return unpack_double(oid);
    default:
        throw LogicException("Called to_float with incorrect ObjectId type, this should never happen");
    }
}

// Returns a string with the lexical representation of the value
inline std::string to_lexical_str(ObjectId oid)
{
    switch (oid.type()) {
    case ObjectType::AnonInl:
        return "_a" + std::to_string(unpack_anon(oid));
    case ObjectType::AnonTmp:
        return "_t" + std::to_string(unpack_anon(oid));
    case ObjectType::NamedNodeInl:
    case ObjectType::NamedNodeExt:
    case ObjectType::NamedNodeTmp:
    case ObjectType::NamedNodeHexInl:
    case ObjectType::NamedNodeHexExt:
    case ObjectType::NamedNodeHexTmp:
    case ObjectType::NamedNodeUuidExt:
    case ObjectType::NamedNodeUuidTmp:
        return unpack_named_node(oid);
    case ObjectType::StringInl:
    case ObjectType::StringExt:
    case ObjectType::StringTmp:
        return unpack_string(oid);
    case ObjectType::NegativeInt56:
    case ObjectType::PositiveInt56: {
        const int64_t i = unpack_int(oid);
        return std::to_string(i);
    }
    case ObjectType::Float: {
        const float f = unpack_float(oid);

        char float_buffer[1 + jkj::dragonbox::max_output_string_length<jkj::dragonbox::ieee754_binary32>];
        jkj::dragonbox::to_chars(f, float_buffer);

        return std::string(float_buffer);
    }
    case ObjectType::Date: {
        const DateTime datetime = unpack_date(oid);
        return "date(\"" + datetime.get_value_string() + "\")";
    }
    case ObjectType::Datetime: {
        const DateTime datetime = unpack_date(oid);
        return "dateTime(\"" + datetime.get_value_string() + "\")";
    }
    case ObjectType::Datetimestamp: {
        const DateTime datetime = unpack_date(oid);
        return "dateTimeStamp(\"" + datetime.get_value_string() + "\")";
    }
    case ObjectType::Time: {
        const DateTime datetime = unpack_date(oid);
        return "time(\"" + datetime.get_value_string() + "\")";
    }
    case ObjectType::Bool:
        return unpack_bool(oid) ? "true" : "false";
    case ObjectType::DirectedEdge:
        return "_e" + std::to_string(unpack_edge(oid));

    case ObjectType::TensorFloatExt:
    case ObjectType::TensorFloatTmp: {
        const auto tensor = unpack_tensor<float>(oid);
        return tensor.to_string();
    }
    case ObjectType::TensorDoubleExt:
    case ObjectType::TensorDoubleTmp: {
        const auto tensor = unpack_tensor<double>(oid);
        return tensor.to_string();
    }
    case ObjectType::ListExt:
    case ObjectType::ListTmp: {
        auto list = unpack_list(oid);
        std::stringstream ss;
        ss << "[";
        for (auto it = list.begin(); it != list.end(); ++it) {
            if (it != list.begin()) {
                ss << ",";
            }
            to_lexical_str(*it);
        }
        ss << "]";
        return ss.str();
    }
    case ObjectType::DictionaryExt:
    case ObjectType::DictionaryTmp: {
        std::unique_ptr<Dictionary> dict = unpack_dictionary(oid);
        std::stringstream ss;
        dict->to_string(ss);
        return ss.str();
    }
    case ObjectType::PropertyKey:
        return Conversions::get_key(oid);
    case ObjectType::EdgeLabel:
        return Conversions::get_edge_label(oid);
    case ObjectType::NodeLabel:
        return Conversions::get_node_label(oid);
    case ObjectType::Path:
    case ObjectType::Null:
    case ObjectType::DoubleExt:
    case ObjectType::DoubleTmp:
    case ObjectType::DecimalInl:
    case ObjectType::DecimalExt:
    case ObjectType::DecimalTmp:
        // TODO:
        break;

    case ObjectType::StringXsdInl:
    case ObjectType::StringXsdExt:
    case ObjectType::StringXsdTmp:
    case ObjectType::StringLangInl:
    case ObjectType::StringLangExt:
    case ObjectType::StringLangTmp:
    case ObjectType::StringDatatypeInl:
    case ObjectType::StringDatatypeExt:
    case ObjectType::StringDatatypeTmp:
    case ObjectType::IriInl:
    case ObjectType::IriExt:
    case ObjectType::IriTmp:
    case ObjectType::IriUuidLowerTmp:
    case ObjectType::IriUuidLowerExt:
    case ObjectType::IriUuidUpperTmp:
    case ObjectType::IriUuidUpperExt:
    case ObjectType::IriHexLowerTmp:
    case ObjectType::IriHexLowerExt:
    case ObjectType::IriHexUpperTmp:
    case ObjectType::IriHexUpperExt:
    case ObjectType::UndirectedEdge:
    case ObjectType::Direction:
    case ObjectType::NotFound:
        assert(false);
        break;
    }
    return "";
}

inline ObjectId to_boolean(ObjectId oid)
{
    uint64_t value = oid.get_value();

    switch (oid.subtype()) {
    case ObjectSubType::Bool:
        return oid;
    // Note: Extern strings will never be empty
    // Note: This assumes 0 is never represented as 0.0, 0.00, etc
    // Note: Extern decimals will never be zero
    case ObjectSubType::Decimal:
    case ObjectSubType::String:
    case ObjectSubType::Int:
        return ObjectId(ObjectId::MASK_BOOL | static_cast<uint64_t>(value != 0));
    case ObjectSubType::Float: {
        auto f = unpack_float(oid);
        return ObjectId(ObjectId::MASK_BOOL | static_cast<uint64_t>(f != 0 && !std::isnan(f)));
    }
    case ObjectSubType::Double: {
        auto d = unpack_double(oid);
        return ObjectId(ObjectId::MASK_BOOL | static_cast<uint64_t>(d != 0 && !std::isnan(d)));
    }

    // Note: This assumes empty tensors will never be extern/tmp
    // case ObjectSubType::TensorFloat:
    // case ObjectSubType::TensorDouble:
    //     return ObjectId(ObjectId::MASK_BOOL | static_cast<uint64_t>(value != 0));
    //     return ObjectId::get_true();
    // Can not be converted to boolean
    default:
        return ObjectId::get_null();
    }
}

// works for named nodes and strings
inline size_t print_string(ObjectId oid, char* out)
{
    const auto type = oid.type();
    const auto unmasked_id = oid.id & ObjectId::VALUE_MASK;
    switch (type) {
    case ObjectType::NamedNodeInl: {
        return Inliner::print_string_inlined<7>(out, unmasked_id);
    }
    case ObjectType::NamedNodeExt: {
        return string_manager.print_to_buffer(out, unmasked_id);
    }
    case ObjectType::NamedNodeTmp: {
        return tmp_manager.print_to_buffer(out, unmasked_id);
    }
    case ObjectType::StringInl: {
        return Inliner::print_string_inlined<7>(out, unmasked_id);
    }
    case ObjectType::StringExt: {
        return string_manager.print_to_buffer(out, unmasked_id);
    }
    case ObjectType::StringTmp: {
        return tmp_manager.print_to_buffer(out, unmasked_id);
    }
    default:
        throw std::logic_error("Unmanaged mask in MQL::Conversions::print_string: " + to_string(type));
    }
}

}} // namespace MQL::Conversions
