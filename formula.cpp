#include "formula.h"
#include "FormulaAST.h"
#include "common.h"

#include <sstream>

using namespace std::literals;

std::ostream& operator<<(std::ostream& output, const FormulaError& fe) {
    return output << "#ARITHM!";
}

namespace {
    class Formula : public FormulaInterface {
    public:
        explicit Formula(std::string expression)
                : ast_(ParseFormulaAST(expression)) {
        }

        Value Evaluate() const override {
            try {
                double result = ast_.Execute();
                return result;
            } catch (const FormulaError& fe) {
                return fe;
            }
        }

        std::string GetExpression() const override {
            std::ostringstream out;
            ast_.PrintFormula(out);
            return out.str();
        }

    private:
        FormulaAST ast_;
    };
}

std::unique_ptr<FormulaInterface> ParseFormula(std::string expression) {
    return std::make_unique<Formula>(std::move(expression));
}
