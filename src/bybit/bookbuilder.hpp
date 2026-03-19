#pragma once

#include "book.hpp"

#include <cmath>
#include <span>

namespace bybit {

/* Implements orderbook building from snapshot and delta messages. From Bybit documentation:
 * To apply delta updates:
 * If you receive an amount that is 0, delete the entry
 * If you receive an amount that does not exist, insert it
 * If the entry exists, you simply update the value
 */
template<size_t DEPTH>
class Bookbuilder {
public:
    Bookbuilder(Book<DEPTH>& _book) : book(_book) {};

    void clear() {
        book.clear();
    }

    void applyDelta(Side side, double px, double amt, bool remove) noexcept {
        auto& arr = (side == Side::Buy) ? book.getBids() : book.getAsks();
        auto range_begin = arr.begin();
        auto range_end = arr.end();
        
        // Lower bound on array sorted by side:
        // For bids: sorted descending (highest px first) -> compare: lvl.px > val
        // For asks: sorted ascending -> compare: lvl.px < val
        auto it = std::lower_bound(range_begin, range_end, px, [side](const PriceLevel& lvl, double val) {
            if (lvl.empty()) return false;
            if (side == Side::Buy) {
                return lvl.px > val; // treat sequence as "less" if its px is greater than val
            } else {
                return lvl.px < val;
            }
        });

        size_t idx = static_cast<size_t>(std::distance(range_begin, it));

        if (idx < DEPTH && !it->empty() && it->px == px) {
            if (remove) {
                book.deleteLevel(side, idx);
            } else {
                it->amt = amt;
            }
        } 
        else if (!remove && idx < DEPTH) {
            book.insertLevel(side, idx, px, amt);
        }
    }

private:
    Book<DEPTH>& book;
};

} // namespace bybit
