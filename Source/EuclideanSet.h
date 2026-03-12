#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <algorithm>

// Per Skald usiamo direttamente la logica HyperEuclidean
class HyperEuclidean {
public:
    // Costruttore con parametri di default
    HyperEuclidean(int pulses = 4, int steps = 16, int depth = 1);
    ~HyperEuclidean() {}

    // Funzione principale chiamata dal Processor
    std::vector<int> generateSequence();

    // Vettore delle velocity calcolate (IOI)
    std::vector<int> velocities;

private:
    int pulses;
    int steps;
    int depth;
    std::vector<int> sequence;

    void computeHyperEuclidean();
    std::vector<int> generateClassic(int p, int s); // Algoritmo interno di supporto
};