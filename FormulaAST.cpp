#include "FormulaAST.h"

#include "FormulaBaseListener.h"
#include "FormulaLexer.h"
#include "FormulaParser.h"

#include <cassert>
#include <cmath>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <cstdlib>

namespace ASTImpl {

    enum ExprPrecedence {
        EP_ADD,
        EP_SUB,
        EP_MUL,
        EP_DIV,
        EP_UNARY,
        EP_ATOM,
        EP_END,
    };

    enum PrecedenceRule {
        PR_NONE = 0b00,
        PR_LEFT = 0b01,
        PR_RIGHT = 0b10,
        PR_BOTH = PR_LEFT | PR_RIGHT,
    };

    constexpr PrecedenceRule PRECEDENCE_RULES[EP_END][EP_END] = {
            {PR_NONE, PR_NONE, PR_NONE, PR_NONE, PR_NONE, PR_NONE},
            {PR_RIGHT, PR_RIGHT, PR_NONE, PR_NONE, PR_NONE, PR_NONE},
            {PR_BOTH, PR_BOTH, PR_NONE, PR_NONE, PR_NONE, PR_NONE},
            {PR_BOTH, PR_BOTH, PR_RIGHT, PR_RIGHT, PR_NONE, PR_NONE},
            {PR_BOTH, PR_BOTH, PR_NONE, PR_NONE, PR_NONE, PR_NONE},
            {PR_NONE, PR_NONE, PR_NONE, PR_NONE, PR_NONE, PR_NONE},
    };

    class Expr {
    public:
        virtual ~Expr() = default;

        virtual void Print(std::ostream& out) const = 0;
        virtual void DoPrintFormula(std::ostream& out, ExprPrecedence precedence) const = 0;

        [[nodiscard]] virtual double Evaluate(const FormulaAST::CellLookup&) const = 0;

        [[nodiscard]] virtual ExprPrecedence GetPrecedence() const = 0;

        void PrintFormula(std::ostream& out, ExprPrecedence parent_precedence,
                          bool right_child = false) const {
            auto precedence = GetPrecedence();
            auto mask = right_child ? PR_RIGHT : PR_LEFT;
            bool parens_needed = PRECEDENCE_RULES[parent_precedence][precedence] & mask;
            if (parens_needed) {
                out << '(';
            }

            DoPrintFormula(out, precedence);

            if (parens_needed) {
                out << ')';
            }
        }
    };

    class NumberExpr final : public Expr {
    public:
        explicit NumberExpr(double value) : value_(value) {}
        void Print(std::ostream& out) const override { out << value_; }
        void DoPrintFormula(std::ostream& out, ExprPrecedence) const override { out << value_; }
        [[nodiscard]] ExprPrecedence GetPrecedence() const override { return EP_ATOM; }
        [[nodiscard]] double Evaluate(const FormulaAST::CellLookup&) const override { return value_; }

    private:
        double value_;
    };

    class UnaryOpExpr final : public Expr {
    public:
        enum Type : char {
            UnaryPlus = '+',
            UnaryMinus = '-',
        };

        explicit UnaryOpExpr(Type type, std::unique_ptr<Expr> operand)
                : type_(type)
                , operand_(std::move(operand)) {}

        void Print(std::ostream& out) const override {
            out << '(' << static_cast<char>(type_) << ' ';
            operand_->Print(out);
            out << ')';
        }

        void DoPrintFormula(std::ostream& out, ExprPrecedence p) const override {
            out << static_cast<char>(type_);
            operand_->PrintFormula(out, p);
        }

        [[nodiscard]] ExprPrecedence GetPrecedence() const override {
            return EP_UNARY;
        }

        [[nodiscard]] double Evaluate(const FormulaAST::CellLookup& lookup) const override {
            double value = operand_->Evaluate(lookup);
            double result = 0.0;

            switch (type_) {
                case UnaryPlus:
                    result = +value;
                    break;
                case UnaryMinus:
                    result = -value;
                    break;
                default:
                    assert(false);
            }

            if (!std::isfinite(result)) {
                throw FormulaError(FormulaError::Category::Arithmetic);
            }

            return result;
        }

    private:
        Type type_;
        std::unique_ptr<Expr> operand_;
    };

    class BinaryOpExpr final : public Expr {
    public:
        enum Type : char {
            Add      = '+',
            Subtract = '-',
            Multiply = '*',
            Divide   = '/',
        };

        explicit BinaryOpExpr(Type type, std::unique_ptr<Expr> lhs, std::unique_ptr<Expr> rhs)
                : type_(type)
                , lhs_(std::move(lhs))
                , rhs_(std::move(rhs)) {}

        void Print(std::ostream& out) const override {
            out << '(' << static_cast<char>(type_) << ' ';
            lhs_->Print(out);
            out << ' ';
            rhs_->Print(out);
            out << ')';
        }

        void DoPrintFormula(std::ostream& out, ExprPrecedence precedence) const override {
            lhs_->PrintFormula(out, precedence);
            out << static_cast<char>(type_);
            rhs_->PrintFormula(out, precedence, true);
        }

        [[nodiscard]] ExprPrecedence GetPrecedence() const override {
            switch (type_) {
                case Add:
                    return EP_ADD;
                case Subtract:
                    return EP_SUB;
                case Multiply:
                    return EP_MUL;
                case Divide:
                    return EP_DIV;
                default:
                    // have to do this because VC++ has a buggy warning
                    assert(false);
                    return static_cast<ExprPrecedence>(INT_MAX);
            }
        }

        [[nodiscard]] double Evaluate(const FormulaAST::CellLookup& lookup) const override {
            double lhs = lhs_->Evaluate(lookup);
            double rhs = rhs_->Evaluate(lookup);
            double result = 0.0;

            switch (type_) {
                case Add:
                    result = lhs + rhs;
                    break;
                case Subtract:
                    result = lhs - rhs;
                    break;
                case Multiply:
                    result = lhs * rhs;
                    break;
                case Divide:
                    if (rhs == 0.0) {
                        throw FormulaError(FormulaError::Category::Arithmetic);
                    }
                    result = lhs / rhs;
                    break;
                default:
                    assert(false);
            }

            if (!std::isfinite(result)) {
                throw FormulaError(FormulaError::Category::Arithmetic);
            }

            return result;
        }

    private:
        Type type_;
        std::unique_ptr<Expr> lhs_;
        std::unique_ptr<Expr> rhs_;
    };

    class CellExpr final : public Expr {
    public:
        explicit CellExpr(Position pos, std::string raw)
                : pos_(pos)
                , text_(std::move(raw)) {}

        void Print(std::ostream& out) const override { out << text_; }

        void DoPrintFormula(std::ostream& out, ExprPrecedence) const override {
            out << text_;
        }

        [[nodiscard]] ExprPrecedence GetPrecedence() const override {
            return EP_ATOM;
        }

        [[nodiscard]] double Evaluate(const FormulaAST::CellLookup& lookup) const override {
            if (!pos_.IsValid()) {
                throw FormulaError(FormulaError::Category::Ref);
            }

            auto value = lookup(pos_);

            if (auto* err = std::get_if<FormulaError>(&value)) {
                throw *err;
            }

            return std::get<double>(value);
        }

    private:
        Position pos_;
        std::string text_;
    };

    class ParseASTListener final : public FormulaBaseListener {
    public:
        ParseASTListener() = default;

        std::unique_ptr<Expr> MoveRoot() {
            assert(args_.size() == 1);
            auto root = std::move(args_.front());
            args_.clear();

            return root;
        }

        std::forward_list<Position> MoveCells() {
            return std::move(cells_);
        }

        void exitLiteral(FormulaParser::LiteralContext* ctx) override {
            double value = 0;
            auto valueStr = ctx->NUMBER()->getSymbol()->getText();
            std::istringstream in(valueStr);
            in >> value;
            if (!in) {
                throw ParsingError("Invalid number: " + valueStr);
            }

            args_.push_back(std::make_unique<NumberExpr>(value));
        }

        void exitCell(FormulaParser::CellContext* ctx) override {
            auto value_str = ctx->CELL()->getSymbol()->getText();
            auto value = Position::FromString(value_str);
            if (!value.IsValid()) {
                throw FormulaException("Invalid position: " + value_str);
            }

            cells_.push_front(value);
            args_.push_back(std::make_unique<CellExpr>(value, value_str));
        }

        void exitUnaryOp(FormulaParser::UnaryOpContext* ctx) override {
            assert(!args_.empty());

            auto operand = std::move(args_.back());

            UnaryOpExpr::Type type;
            if (ctx->SUB()) {
                type = UnaryOpExpr::UnaryMinus;
            } else {
                assert(ctx->ADD() != nullptr);
                type = UnaryOpExpr::UnaryPlus;
            }

            args_.back() = std::make_unique<UnaryOpExpr>(type, std::move(operand));
        }

        void exitBinaryOp(FormulaParser::BinaryOpContext* ctx) override {
            assert(args_.size() >= 2);

            auto rhs = std::move(args_.back());
            args_.pop_back();

            auto lhs = std::move(args_.back());

            BinaryOpExpr::Type type;
            if (ctx->ADD()) {
                type = BinaryOpExpr::Add;
            } else if (ctx->SUB()) {
                type = BinaryOpExpr::Subtract;
            } else if (ctx->MUL()) {
                type = BinaryOpExpr::Multiply;
            } else {
                assert(ctx->DIV() != nullptr);
                type = BinaryOpExpr::Divide;
            }

            args_.back() = std::make_unique<BinaryOpExpr>(type, std::move(lhs), std::move(rhs));
        }

    private:
        std::vector<std::unique_ptr<Expr>> args_;
        std::forward_list<Position> cells_;
    };

    class BailErrorListener : public antlr4::BaseErrorListener {
    public:
        void syntaxError(antlr4::Recognizer*,
                         antlr4::Token*,
                         size_t, size_t,
                         const std::string& msg,
                         std::exception_ptr) override {
            throw ParsingError("Error when lexing: " + msg);
        }
    };

} // namespace ASTImpl

FormulaAST ParseFormulaAST(std::istream& in) {
    using namespace antlr4;

    ANTLRInputStream input(in);

    FormulaLexer lexer(&input);
    ASTImpl::BailErrorListener error_listener;
    lexer.removeErrorListeners();
    lexer.addErrorListener(&error_listener);

    CommonTokenStream tokens(&lexer);

    FormulaParser parser(&tokens);
    auto error_handler = std::make_shared<BailErrorStrategy>();
    parser.setErrorHandler(error_handler);
    parser.removeErrorListeners();

    tree::ParseTree* tree = parser.main();
    ASTImpl::ParseASTListener listener;
    tree::ParseTreeWalker::DEFAULT.walk(&listener, tree);

    return FormulaAST(listener.MoveRoot(), listener.MoveCells());
}

FormulaAST ParseFormulaAST(const std::string& s) {
    std::istringstream in(s);
    try {
        return ParseFormulaAST(in);
    } catch (const std::exception& e) {
        throw FormulaException(e.what());
    }
}

FormulaAST::FormulaAST(std::unique_ptr<ASTImpl::Expr> root_expr,
                       std::forward_list<Position> cells)
        : root_expr_(std::move(root_expr))
        , cells_(std::move(cells))
{}

FormulaAST::~FormulaAST() = default;

double FormulaAST::Execute(const CellLookup& lookup) const {
    return root_expr_->Evaluate(lookup);
}

void FormulaAST::Print(std::ostream& out) const {
    root_expr_->Print(out);
}

void FormulaAST::PrintFormula(std::ostream& out) const {
    root_expr_->PrintFormula(out, ASTImpl::EP_ATOM);
}

const std::forward_list<Position>& FormulaAST::GetRawReferencedCells() const {
    return cells_;
}

void FormulaAST::PrintCells(std::ostream &out) const {
    bool first = true;
    for (const auto& pos : cells_) {
        if (!first) {
            out << ' ';
        }
        first = false;
        out << pos.ToString();
    }
}