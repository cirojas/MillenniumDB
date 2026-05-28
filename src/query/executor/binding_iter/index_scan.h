#pragma once

#include "query/executor/binding_iter.h"
#include "query/executor/binding_iter/scan_ranges/scan_range.h"
#include "storage/index/bplus_tree/bplus_tree.h"

#include <array>
#include <memory>

template<std::size_t N>
class IndexScan : public BindingIter {
public:
    IndexScan(BPlusTree<N>& bpt, std::array<std::unique_ptr<ScanRange>, N>&& ranges) :
        ranges(std::move(ranges)),
        bpt(bpt)
    { }

    void print(std::ostream& os, int indent, bool stats) const override;

    void _begin(Binding& parent_binding) override;
    bool _next() override;
    void _reset() override;
    void assign_nulls() override;

    // statistics
    uint_fast32_t bpt_searches = 0;
    std::array<std::unique_ptr<ScanRange>, N> ranges;

private:
    BPlusTree<N>& bpt;
    BptIter<N> it;

    Binding* parent_binding;
};
