#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <algorithm>

class HyperEuclidean {
public:
    HyperEuclidean(int pulses = 4, int steps = 16, int depth = 1);
    ~HyperEuclidean() {}

    std::vector<int> generateSequence();

    std::vector<int> velocities;

private:
    int pulses;
    int steps;
    int depth;
    std::vector<int> sequence;

    void computeHyperEuclidean();
    std::vector<int> generateClassic(int p, int s); 

};
