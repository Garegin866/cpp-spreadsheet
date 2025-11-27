#pragma once

#include "common.h"
#include "formula.h"

#include <memory>
#include <string>

class Cell : public CellInterface {
public:
    using Value = CellInterface::Value;

    Cell();
    ~Cell() override;

    void Set(std::string text) override;
    void Clear();

    [[nodiscard]] Value GetValue() const override;
    [[nodiscard]] std::string GetText() const override;

private:
    std::string text_;
    std::unique_ptr<FormulaInterface> formula_;
};
