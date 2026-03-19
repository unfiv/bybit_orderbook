#include <array>
#include <limits>
#include <cstring>

enum Side {
    Buy = 0,
    Sell = 1
};

struct PriceLevel {
    double px;
    double amt;

    PriceLevel(): px{std::numeric_limits<double>::quiet_NaN()}, amt{0}
    {
    }

    void clear() noexcept
    {
        px = std::numeric_limits<double>::quiet_NaN();
        amt = 0;
    }

    bool empty() const noexcept
    {
        return std::isnan(px);
    }
};

// Represents level 2 orderbook (orderbook with aggregated price levels)
// You can learn more about ie here: https://academy.binance.com/en/articles/what-is-an-order-book-and-how-does-it-work
template<size_t DEPTH>
class Book {
public:
    constexpr void clear()
    {
        for (int d = 0; d < depth(); ++d) {
            bidLevelsArr[d].clear();
            askLevelsArr[d].clear();
        }
    }

    constexpr size_t depth() const
    {
        return DEPTH;
    }

    PriceLevel& level(Side side, size_t l)
    {
        return side == Buy ? bidLevelsArr[l] : askLevelsArr[l];
    }

    void deleteLevel(Side side, size_t lvlIdx)
    {
        shuffleLevels(side, lvlIdx + 1, lvlIdx, depth() - lvlIdx - 1);
        auto& last_level = side == Buy ? bidLevelsArr[depth() - 1] : askLevelsArr[depth() - 1];
        last_level.clear();
    }

    void insertLevel(Side side, size_t lvlIdx, double px, double amt)
    {
        shuffleLevels(side, lvlIdx, lvlIdx + 1, depth() - lvlIdx - 1);
        auto& lvl = side == Buy ? bidLevelsArr[lvlIdx] : askLevelsArr[lvlIdx];
        lvl.px = px;
        lvl.amt = amt;
    }

    auto& getBids() { return bidLevelsArr; }
    auto& getAsks() { return askLevelsArr; }

private:
    void shuffleLevels(Side side, size_t fromIdx, size_t toIdx, size_t count) {
        if (count == 0) [[unlikely]] {
            return;
        }

        char* levels = (char*)(side == Side::Buy ? bidLevelsArr : askLevelsArr).data();
        std::memmove(levels + toIdx * sizeof(PriceLevel), levels + fromIdx * sizeof(PriceLevel), count * sizeof(PriceLevel));
    }

    std::array<PriceLevel, DEPTH> bidLevelsArr;
    std::array<PriceLevel, DEPTH> askLevelsArr;
};
