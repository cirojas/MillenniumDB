#pragma once

namespace MQL {

class InsertNode;
class InsertEdge;
class InsertProperty;
class InsertPropertyExpr;
class InsertNodeLabel;
class SetLabel;
class DeleteProperty;
class DeleteNodeLabel;
class DeleteObject;
class CreateTextIndex;
class CreateHNSWIndex;

class UpdateActionVisitor {
public:
    virtual ~UpdateActionVisitor() = default;

    virtual void visit(InsertNode&) = 0;
    virtual void visit(InsertNodeLabel&) = 0;
    virtual void visit(SetLabel&) = 0;
    virtual void visit(InsertProperty&) = 0;
    virtual void visit(InsertPropertyExpr&) = 0;
    virtual void visit(DeleteProperty&) = 0;
    virtual void visit(DeleteNodeLabel&) = 0;
    virtual void visit(InsertEdge&) = 0;
    virtual void visit(DeleteObject&) = 0;
    virtual void visit(CreateTextIndex&) = 0;
    virtual void visit(CreateHNSWIndex&) = 0;
};
} // namespace MQL
