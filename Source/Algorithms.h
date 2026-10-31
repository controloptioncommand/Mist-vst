#pragma once
#include "Params.h"
#include <array>
#include <cstdint>

namespace ferrite
{
struct Algorithm
{
    const char* name = "";
    // modulates[src][dst] — оператор src модулирует фазу dst
    std::array<std::array<bool, kNumOps>, kNumOps> modulates {};
    std::array<bool, kNumOps> carrier {};
    int numCarriers = 0;
};

const Algorithm& getAlgorithm (int index);   // 0..kNumAlgorithms-1
} // namespace ferrite
