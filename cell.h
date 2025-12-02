#pragma once

#include "common.h"
#include "formula.h"

#include <unordered_set>
#include <vector>
#include <optional>

class Sheet;

class Cell : public CellInterface {
public:
    using Value = CellInterface::Value;

    explicit Cell(Sheet& sheet);
    ~Cell() override;

    void Set(std::string text);
    void Clear();

    [[nodiscard]] Value GetValue() const override;
    [[nodiscard]] std::string GetText() const override;
    [[nodiscard]] std::vector<Position> GetReferencedCells() const override;

    [[nodiscard]] bool IsReferenced() const;

private:
    class Impl;
    class EmptyImpl;
    class TextImpl;
    class FormulaImpl;

    [[nodiscard]] bool HasCircularReferences(Impl& impl);
    void UpdateReferences();
    void InvalidateCache();

private:
    Sheet& sheet_;
    std::unique_ptr<Impl> impl_;
    mutable std::optional<Value> cache_;

    std::unordered_set<Cell*> referenced_;
    std::unordered_set<Cell*> dependents_;

private:
    void InvalidateCacheImpl(std::unordered_set<Cell*>& visited);
};
