#pragma once

#include <set>

#include "query/executor/binding.h"

#include "query/update/mql/update_action/update_action_visitor.h"
#include "query/update/mql/update_context.h"
#include "system/string_manager.h"
#include "system/tensor_manager.h"
#include "system/tmp_manager.h"

namespace MQL {

class UpdateAction {
public:
    virtual ~UpdateAction() = default;

    virtual void process(Binding& binding, UpdateContext&) = 0;

    static constexpr uint64_t CLEAR_TMP_MASK = ~(ObjectId::MOD_MASK | ObjectId::MASK_EXTERNAL_ID);

    virtual void print(std::ostream& os, int indent) const = 0;

    // to check used vars are declared
    virtual std::set<VarId> get_input_vars() const = 0;

    virtual void accept_visitor(UpdateActionVisitor&) = 0;

protected:
    ObjectId transform_if_tmp(ObjectId oid)
    {
        if (oid.is_tmp()) {
            const uint64_t tmp_id = oid.id & ObjectId::MASK_EXTERNAL_ID;
            const auto& tmp_str = tmp_manager.get_str(tmp_id);

            uint64_t new_external_id;
            if (oid.generic_type() == ObjectGenType::Tensor) {
                new_external_id = tensor_manager.get_or_create_id(tmp_str.data(), tmp_str.size());
            } else {
                new_external_id = string_manager.get_or_create(tmp_str.data(), tmp_str.size());
            }

            oid.id = (oid.id & CLEAR_TMP_MASK) | ObjectId::MOD_EXT | new_external_id;
        }

        assert(!oid.is_tmp());

        return oid;
    }
};

class CreateTextIndex : public UpdateAction {
public:
    const std::string index_name;
    const std::string property;
    const TextSearch::NORMALIZE_TYPE normalize_type;
    const TextSearch::TOKENIZE_TYPE tokenize_type;

    CreateTextIndex(
        std::string&& index_name,
        std::string&& property,
        TextSearch::NORMALIZE_TYPE normalize_type,
        TextSearch::TOKENIZE_TYPE tokenize_type
    ) :
        index_name(std::move(index_name)),
        property(std::move(property)),
        normalize_type(normalize_type),
        tokenize_type(tokenize_type)
    { }

    void process(Binding&, UpdateContext& ctx) override
    {
        ctx.create_text_index(*this);
    }

    void print(std::ostream& os, int indent = 0) const override
    {
        os << std::string(indent, ' ');
        os << "OpCreateTextIndex(index_name: " << index_name << ", property: " << property
           << ", normalize_type: " << normalize_type << ", tokenize_type: " << tokenize_type << ")\n";
    }

    std::set<VarId> get_input_vars() const override
    {
        std::set<VarId> res;
        return res;
    }

    void accept_visitor(UpdateActionVisitor& visitor) override {
        visitor.visit(*this);
    }
};

class CreateHNSWIndex : public UpdateAction {
public:
    const std::string index_name;
    const std::string property;
    const uint64_t dimension;
    const uint64_t max_edges;
    const uint64_t max_candidates;
    const HNSW::MetricType metric_type;

    CreateHNSWIndex(
        std::string&& index_name,
        std::string&& property,
        uint64_t dimension,
        uint64_t num_edges,
        uint64_t num_candidates,
        HNSW::MetricType metric_type
    ) :
        index_name(std::move(index_name)),
        property(std::move(property)),
        dimension(dimension),
        max_edges(num_edges),
        max_candidates(num_candidates),
        metric_type(metric_type)
    { }

    void process(Binding&, UpdateContext& ctx) override
    {
        ctx.create_hnsw_index(*this);
    }

    void print(std::ostream& os, int indent = 0) const override
    {
        os << std::string(indent, ' ');
        os << "OpCreateHNSWIndex(index_name: " << index_name << ", property: " << property
           << ", dimension: " << dimension << ", num_edges: " << max_edges
           << ", num_candidates: " << max_candidates << ", metric_type: " << metric_type << ")\n";
    }

    std::set<VarId> get_input_vars() const override
    {
        std::set<VarId> res;
        return res;
    }

    void accept_visitor(UpdateActionVisitor& visitor) override {
        visitor.visit(*this);
    }
};

} // namespace MQL
