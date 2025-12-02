#include "cell.h"
#include "sheet.h"

class Cell::Impl {
public:
    virtual ~Impl() = default;

    [[nodiscard]] virtual Value GetValue(const Sheet& sheet) const = 0;
    [[nodiscard]] virtual std::string GetText() const = 0;
    [[nodiscard]] virtual std::vector<Position> GetReferencedCells() const { return {}; }
};

class Cell::EmptyImpl : public Impl {
public:
    [[nodiscard]] Value GetValue(const Sheet&) const override {
        return std::string{};
    }

    [[nodiscard]] std::string GetText() const override {
        return {};
    }
};

class Cell::TextImpl : public Impl {
public:
    explicit TextImpl(std::string text)
            : text_(std::move(text)) {}

    [[nodiscard]] Value GetValue(const Sheet&) const override {
        if (!text_.empty() && text_[0] == ESCAPE_SIGN) {
            return text_.substr(1);
        }
        return text_;
    }

    [[nodiscard]] std::string GetText() const override {
        return text_;
    }
private:
    std::string text_;
};

class Cell::FormulaImpl : public Impl {
public:
    FormulaImpl(std::string expression,
                std::unique_ptr<FormulaInterface> formula)
            : expr_(std::move(expression))
            , formula_(std::move(formula)) {}

    [[nodiscard]] Value GetValue(const Sheet& sheet) const override {
        FormulaInterface::Value v = formula_->Evaluate(sheet);

        if (auto* err = std::get_if<FormulaError>(&v)) {
            return *err;
        }
        return std::get<double>(v);
    }

    [[nodiscard]] std::string GetText() const override {
        return std::string(1, FORMULA_SIGN) + formula_->GetExpression();
    }

    [[nodiscard]] std::vector<Position> GetReferencedCells() const override {
        return formula_->GetReferencedCells();
    }

private:
    std::string expr_;
    std::unique_ptr<FormulaInterface> formula_;
};

Cell::Cell(Sheet& sheet)
        : sheet_(sheet)
        , impl_(std::make_unique<EmptyImpl>()) {}

Cell::~Cell() = default;

Cell::Value Cell::GetValue() const {
    if (cache_) {
        return *cache_;
    }
    Value v = impl_->GetValue(sheet_);
    cache_ = v;
    return v;
}

std::string Cell::GetText() const {
    return impl_->GetText();
}

void Cell::Set(std::string text) {
    std::unique_ptr<Impl> new_impl;

    if (text.empty()) {
        new_impl = std::make_unique<EmptyImpl>();
    } else if (text.size() > 1 && text[0] == FORMULA_SIGN) {
        const std::string expr = text.substr(1);
        auto formula = ParseFormula(expr);
        new_impl = std::make_unique<FormulaImpl>(expr, std::move(formula));
    } else {
        new_impl = std::make_unique<TextImpl>(std::move(text));
    }

    if (HasCircularReferences(*new_impl)) {
        throw CircularDependencyException("Circular References");
    }

    impl_ = std::move(new_impl);
    cache_.reset();

    UpdateReferences();
    InvalidateCache();
}

void Cell::Clear() {
    for (Cell* ref : referenced_) {
        if (ref) {
            ref->dependents_.erase(this);
        }
    }
    referenced_.clear();

    InvalidateCache();

    impl_ = std::make_unique<EmptyImpl>();
}

std::vector<Position> Cell::GetReferencedCells() const {
    if (impl_ == nullptr) {
        return {};
    }

    return impl_->GetReferencedCells();
}

bool Cell::IsReferenced() const {
    return !referenced_.empty();
}

void Cell::UpdateReferences() {
    for (Cell* ref_cell : referenced_) {
        if (ref_cell) {
            ref_cell->dependents_.erase(this);
        }
    }

    referenced_.clear();

    const auto new_refs = impl_->GetReferencedCells();
    for (const auto& pos : new_refs) {
        Cell* ref_cell = sheet_.GetOrCreateCell(pos);
        if (!ref_cell || ref_cell == this) {
            continue;
        }
        referenced_.insert(ref_cell);
        ref_cell->dependents_.insert(this);
    }
}

bool Cell::HasCircularReferences(Cell::Impl &impl) {
    const auto new_refs = impl.GetReferencedCells();
    if (new_refs.empty()) {
        return false;
    }

    for (const auto& pos : new_refs) {
        Cell* start = sheet_.GetOrCreateCell(pos);
        if (!start) {
            continue;
        }
        if (start == this) {
            return true;
        }

        std::unordered_set<const Cell*> visited;
        std::vector<const Cell*> stack;
        stack.push_back(start);

        while (!stack.empty()) {
            const Cell* current = stack.back();
            stack.pop_back();

            if (!visited.insert(current).second) {
                continue;
            }

            if (current == this) {
                return true;
            }

            for (Cell* next : current->referenced_) {
                if (next) {
                    stack.push_back(next);
                }
            }
        }
    }

    return false;
}

void Cell::InvalidateCache() {
    std::unordered_set<Cell*> visited;
    InvalidateCacheImpl(visited);
}

void Cell::InvalidateCacheImpl(std::unordered_set<Cell*>& visited) {
    if (!visited.insert(this).second) {
        return;
    }
    cache_.reset();

    for (Cell* dep : dependents_) {
        if (dep) {
            dep->InvalidateCacheImpl(visited);
        }
    }
}
