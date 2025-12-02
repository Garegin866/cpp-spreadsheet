#pragma once

#include "cell.h"
#include "common.h"

#include <memory>
#include <unordered_map>

class Sheet : public SheetInterface {
public:
    ~Sheet() override = default;

    void SetCell(Position pos, std::string text) override;

    [[nodiscard]] const CellInterface* GetCell(Position pos) const override;
    CellInterface* GetCell(Position pos) override;

    void ClearCell(Position pos) override;

    [[nodiscard]] Size GetPrintableSize() const override;

    void PrintValues(std::ostream& output) const override;
    void PrintTexts(std::ostream& output) const override;

    Cell* GetOrCreateCell(Position pos);

private:
    struct PositionHasher {
        size_t operator()(const Position& pos) const noexcept {
            return (static_cast<size_t>(pos.row) << 16)
                   ^ static_cast<size_t>(pos.col);
        }
    };

    std::unordered_map<Position, std::unique_ptr<Cell>, PositionHasher> cells_;
};