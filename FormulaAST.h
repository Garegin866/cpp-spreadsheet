#pragma once

#include "formula.h"
#include "FormulaLexer.h"
#include "common.h"

#include <forward_list>
#include <functional>
#include <memory>
#include <ostream>
#include <vector>

namespace ASTImpl {
class Expr;
}

class ParsingError : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

class FormulaAST {
public:
    using CellLookup = std::function<FormulaInterface::Value(const Position&)>;

    explicit FormulaAST(std::unique_ptr<ASTImpl::Expr> root_expr,
                        std::forward_list<Position> cells);
    FormulaAST(FormulaAST&&) = default;
    FormulaAST& operator=(FormulaAST&&) = default;
    ~FormulaAST();

    [[nodiscard]] double Execute(const CellLookup& lookup) const;
    void PrintCells(std::ostream& out) const;
    void Print(std::ostream& out) const;
    void PrintFormula(std::ostream& out) const;

    [[nodiscard]] const std::forward_list<Position>& GetRawReferencedCells() const;

    std::forward_list<Position>& GetCells() {
        return cells_;
    }

    [[nodiscard]] const std::forward_list<Position>& GetCells() const {
        return cells_;
    }

private:
    std::unique_ptr<ASTImpl::Expr> root_expr_;
    std::forward_list<Position> cells_;
};

FormulaAST ParseFormulaAST(std::istream& in);
FormulaAST ParseFormulaAST(const std::string& in_str);