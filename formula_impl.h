#pragma once

#include "formula.h"
#include "FormulaAST.h"

#include <memory>
#include <vector>

class Formula : public FormulaInterface {
public:
    explicit Formula(const std::string &expression);

    [[nodiscard]] Value Evaluate(const SheetInterface& sheet) const override;
    [[nodiscard]] std::string GetExpression() const override;
    [[nodiscard]] std::vector<Position> GetReferencedCells() const override;

private:
    FormulaAST ast_;
    std::vector<Position> referenced_cells_;
    std::string expression_;

    static std::vector<Position> PrepareRefs(
            const std::forward_list<Position>& raw);
};