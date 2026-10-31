#include "Algorithms.h"
#include <vector>

namespace loom
{
namespace
{
struct AlgoDef { const char* name; const char* edges; const char* carriers; };

// "ab" — оператор a модулирует оператор b (нумерация с 1).
// Связи «снизу вверх» (например 1>6) замыкают петли и работают с задержкой в 1 сэмпл.
const AlgoDef defs[kNumAlgorithms] =
{
    { "Stack",          "65 54 43 32 21",                               "1" },
    { "Stack5+1",       "65 54 43 32",                                  "21" },
    { "Stack4+2",       "65 54 43 21",                                  "31" },
    { "Twin Stacks",    "65 54 32 21",                                  "41" },
    { "Stack4+Sines",   "65 54 43",                                     "321" },
    { "Three Pairs",    "65 43 21",                                     "531" },
    { "Stack3+Sines",   "65 54",                                        "4321" },
    { "Pair+Sines",     "65",                                           "54321" },
    { "Additive",       "",                                             "654321" },
    { "Five into One",  "61 51 41 31 21",                               "1" },
    { "Branch",         "65 54 43 31 21",                               "1" },
    { "Three Branches", "65 51 43 31 21",                               "1" },
    { "Y Stack",        "64 54 43 32 21",                               "1" },
    { "Fork 3",         "63 53 43 32 21",                               "1" },
    { "Fork 4",         "62 52 42 32 21",                               "1" },
    { "Split Top",      "65 64 54 43 32 21",                            "1" },
    { "Diamond",        "65 64 53 43 32 21",                            "1" },
    { "One to Five",    "61 62 63 64 65",                               "54321" },
    { "Spray",          "63 62 61 54",                                  "4321" },
    { "Hub",            "65 53 52 51",                                  "4321" },
    { "Twin Forks",     "64 54 31 21",                                  "41" },
    { "Merge",          "65 54 42 32 21",                               "1" },
    { "Fan Out",        "65 54 41 42 43",                               "321" },
    { "Ring 6",         "65 54 43 32 21 16",                            "1" },
    { "Twin Rings",     "32 21 13 65 54 46",                            "14" },
    { "Cross Pairs",    "56 65 34 43 12 21",                            "135" },
    { "Cross Top",      "65 56 54 43 32 21",                            "1" },
    { "Lattice",        "65 53 31 64 42 21",                            "1" },
    { "Cascade All",    "61 51 41 31 21 62 52 42 32 63 53 43 64 54 65", "1" },
    { "Stack+Bypass",   "65 54 43 32 21 61",                            "1" },
    { "Star Loop",      "61 51 41 31 21 12",                            "1" },
    { "Chaos",          "65 54 43 32 21 14 25 36",                      "14" },
};

std::vector<Algorithm> buildAll()
{
    std::vector<Algorithm> all;
    for (auto& d : defs)
    {
        Algorithm a;
        a.name = d.name;
        juce::StringArray edges;
        edges.addTokens (d.edges, " ", "");
        edges.removeEmptyStrings();
        for (auto& e : edges)
        {
            const int s = e[0] - '1', t = e[1] - '1';
            jassert (s >= 0 && s < kNumOps && t >= 0 && t < kNumOps && s != t);
            a.modulates[(size_t) s][(size_t) t] = true;
        }
        for (const char* c = d.carriers; *c != 0; ++c)
            a.carrier[(size_t) (*c - '1')] = true;
        for (bool c : a.carrier) a.numCarriers += c ? 1 : 0;
        all.push_back (a);
    }
    return all;
}
} // namespace

const Algorithm& getAlgorithm (int index)
{
    static const std::vector<Algorithm> all = buildAll();
    return all[(size_t) juce::jlimit (0, kNumAlgorithms - 1, index)];
}
} // namespace loom
