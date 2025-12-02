#include "formula.h"
#include "FormulaAST.h"
#include "common.h"

#include <sstream>
#include <cstdlib>

using namespace std;

namespace {

    class Formula : public FormulaInterface {
    public:
        explicit Formula(std::string expression)
                : ast_(ParseFormulaAST(expression))
        {
            std::ostringstream oss;
            ast_.PrintFormula(oss);
            expression_ = oss.str();
        }

        [[nodiscard]] Value Evaluate(const SheetInterface& sheet) const override {
            auto lookup = [&](const Position& pos) -> Value {
                const CellInterface* cell = sheet.GetCell(pos);

                if (!pos.IsValid())
                    return FormulaError(FormulaError::Category::Ref);

                if (!cell)
                    return 0.0;

                auto cv = cell->GetValue();

                if (std::holds_alternative<double>(cv))
                    return std::get<double>(cv);

                if (std::holds_alternative<FormulaError>(cv))
                    return std::get<FormulaError>(cv);

                const string& text = std::get<std::string>(cv);

                if (text.empty())
                    return 0.0;

                char* end = nullptr;
                double parsed = std::strtod(text.c_str(), &end);

                if (end && *end == '\0')
                    return parsed;

                return FormulaError(FormulaError::Category::Value);
            };

            try {
                double res = ast_.Execute(lookup);
                return res;
            } catch (const FormulaError& fe) {
                return fe;
            }
        }

        [[nodiscard]] std::string GetExpression() const override {
            return expression_;
        }

        [[nodiscard]] std::vector<Position> GetReferencedCells() const override {
            std::vector<Position> result;
            for (const auto& p : ast_.GetRawReferencedCells()) {
                if (p.IsValid())
                    result.push_back(p);
            }
            sort(result.begin(), result.end());
            result.erase(unique(result.begin(), result.end()), result.end());
            return result;
        }

    private:
        FormulaAST ast_;
        std::string expression_;
    };

} // namespace

std::unique_ptr<FormulaInterface> ParseFormula(std::string expression) {
    return std::make_unique<Formula>(std::move(expression));
}

FormulaError::FormulaError(Category category)
        : category_(category) {
}

FormulaError::Category FormulaError::GetCategory() const {
    return category_;
}

bool FormulaError::operator==(FormulaError rhs) const {
    return category_ == rhs.category_;
}

std::string_view FormulaError::ToString() const {
    using Category = FormulaError::Category;
    switch (category_) {
        case Category::Ref:
            return "#REF!";
        case Category::Value:
            return "#VALUE!";
        case Category::Arithmetic:
            return "#ARITHM!";
    }
    return "#VALUE!";
}

std::ostream& operator<<(std::ostream& output, FormulaError fe) {
    return output << fe.ToString();
}