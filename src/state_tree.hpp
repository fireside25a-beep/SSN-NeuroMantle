#pragma once
#include "neuro_mantle.h"
#include <array>
#include <cstddef>
#include <string>

namespace nm_state {
/* Domain-separated SHA-256 tree over canonical, fixed-partition state leaves. */
class Tree {
public:
    explicit Tree(const nm_network &network);
    void neuron(std::size_t id);
    void edge(std::size_t id);
    void pending(std::size_t slot);
    void tick();
    std::string root();
private:
    static constexpr std::size_t leaf_count=256;
    const nm_network &network;
    std::array<std::string,leaf_count*2> nodes{};
    std::array<bool,leaf_count*2> dirty{};
    void mark(std::size_t leaf);
    std::string leaf(std::size_t index) const;
};
}
