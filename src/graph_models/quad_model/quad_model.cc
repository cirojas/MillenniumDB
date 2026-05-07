#include "quad_model.h"

#include <type_traits>

#include "query/query_context.h"
#include "query/executor/query_executor/mql/return_executor.h"
#include "storage/index/bplus_tree/bplus_tree.h"

using namespace std;

// memory for the object
static typename std::aligned_storage<sizeof(QuadModel), alignof(QuadModel)>::type quad_model_buf;
// global object
QuadModel& quad_model = reinterpret_cast<QuadModel&>(quad_model_buf);

std::unique_ptr<ModelDestroyer> QuadModel::init()
{
    new (&quad_model) QuadModel();
    return std::make_unique<ModelDestroyer>([]() { quad_model.~QuadModel(); });
}

std::ostream& debug_print(std::ostream& os, ObjectId oid)
{
    MQL::ReturnExecutor<MQL::ReturnType::CSV>::print(os, oid);
    return os;
}

QuadModel::QuadModel() :
    catalog("catalog.dat")
{
    QueryContext::_debug_print = debug_print;

    nodes = make_unique<BPlusTree<1>>("nodes");

    // Create BPT
    label_node = make_unique<BPlusTree<2>>("label_node");
    node_label = make_unique<BPlusTree<2>>("node_label");

    node_key_value = make_unique<BPlusTree<3>>("node_key_value");
    key_value_node = make_unique<BPlusTree<3>>("key_value_node");

    edge_key_value = make_unique<BPlusTree<3>>("edge_key_value");
    key_value_edge = make_unique<BPlusTree<3>>("key_value_edge");

    from_to_label_edge = make_unique<BPlusTree<4>>("from_to_label_edge");
    to_label_from_edge = make_unique<BPlusTree<4>>("to_label_from_edge");
    label_from_to_edge = make_unique<BPlusTree<4>>("label_from_to_edge");
    label_to_from_edge = make_unique<BPlusTree<4>>("label_to_from_edge");
    edge_from_to_label = make_unique<BPlusTree<4>>("edge_from_to_label");

    equal_from_to = make_unique<BPlusTree<3>>("equal_from_to");
    equal_from_to_inv = make_unique<BPlusTree<3>>("equal_from_to_inverted");
}
