#include "sheet.h"

#include "cell.h"
#include "common.h"

#include <algorithm>
#include <iostream>
#include <optional>
#include <sstream>
#include <utility>

using namespace std::literals;

namespace {

    void CheckPositionValid(Position pos) {
        if (!pos.IsValid()) {
            throw InvalidPositionException("Invalid position");
        }
    }

} // namespace

void Sheet::SetCell(Position pos, std::string text) {
    CheckPositionValid(pos);

    if (text.empty()) {
        ClearCell(pos);
        return;
    }

    auto it = cells_.find(pos);
    if (it == cells_.end()) {
        auto new_cell = std::make_unique<Cell>();
        new_cell->Set(std::move(text));
        cells_.emplace(pos, std::move(new_cell));
    } else {
        it->second->Set(std::move(text));
    }
}

const CellInterface* Sheet::GetCell(Position pos) const {
    CheckPositionValid(pos);

    auto it = cells_.find(pos);
    if (it == cells_.end()) {
        return nullptr;
    }
    return it->second.get();
}

CellInterface* Sheet::GetCell(Position pos) {
    CheckPositionValid(pos);

    auto it = cells_.find(pos);
    if (it == cells_.end()) {
        return nullptr;
    }
    return it->second.get();
}

void Sheet::ClearCell(Position pos) {
    CheckPositionValid(pos);
    cells_.erase(pos);
}

Size Sheet::GetPrintableSize() const {
    if (cells_.empty()) {
        return {0, 0};
    }

    int max_row = -1;
    int max_col = -1;

    for (const auto& [pos, cell_ptr] : cells_) {
        if (!cell_ptr) {
            continue;
        }
        const std::string text = cell_ptr->GetText();
        if (text.empty()) {
            continue;
        }
        if (pos.row > max_row) {
            max_row = pos.row;
        }
        if (pos.col > max_col) {
            max_col = pos.col;
        }
    }

    if (max_row == -1 || max_col == -1) {
        return {0, 0};
    }

    return {max_row + 1, max_col + 1};
}

void Sheet::PrintValues(std::ostream& output) const {
    Size size = GetPrintableSize();
    for (int r = 0; r < size.rows; ++r) {
        for (int c = 0; c < size.cols; ++c) {
            if (c > 0) {
                output << '\t';
            }

            Position pos{r, c};
            auto it = cells_.find(pos);
            if (it == cells_.end() || !it->second) {
                continue;
            }

            const auto& cell = it->second;
            const std::string text = cell->GetText();
            if (text.empty()) {
                continue;
            }

            auto value = cell->GetValue();
            std::visit([&output](const auto& v) { output << v; }, value);
        }
        output << '\n';
    }
}

void Sheet::PrintTexts(std::ostream& output) const {
    Size size = GetPrintableSize();
    for (int r = 0; r < size.rows; ++r) {
        for (int c = 0; c < size.cols; ++c) {
            if (c > 0) {
                output << '\t';
            }

            Position pos{r, c};
            auto it = cells_.find(pos);
            if (it == cells_.end() || !it->second) {
                continue;
            }

            const std::string text = it->second->GetText();
            if (text.empty()) {
                continue;
            }

            output << text;
        }
        output << '\n';
    }
}

std::unique_ptr<SheetInterface> CreateSheet() {
    return std::make_unique<Sheet>();
}
