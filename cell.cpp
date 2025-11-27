#include "cell.h"

#include <string>
#include <utility>

Cell::Cell() = default;

Cell::~Cell() = default;

void Cell::Set(std::string text) {
    if (text.size() > 1 && text[0] == FORMULA_SIGN) {
        std::string expr = text.substr(1);
        auto new_formula = ParseFormula(expr);
        std::string normalized_text = std::string(1, FORMULA_SIGN) + new_formula->GetExpression();
        formula_ = std::move(new_formula);
        text_ = std::move(normalized_text);
    } else {
        formula_.reset();
        text_ = std::move(text);
    }
}

void Cell::Clear() {
    text_.clear();
    formula_.reset();
}

Cell::Value Cell::GetValue() const {
    if (formula_) {
        auto v = formula_->Evaluate();
        if (std::holds_alternative<double>(v)) {
            return std::get<double>(v);
        } else {
            return std::get<FormulaError>(v);
        }
    } else {
        if (!text_.empty() && text_[0] == ESCAPE_SIGN) {
            return text_.substr(1);
        }
        return text_;
    }
}

std::string Cell::GetText() const {
    return text_;
}
