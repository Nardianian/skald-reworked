#include "EuclideanSet.h"
#include <functional>

HyperEuclidean::HyperEuclidean(int p, int s, int d)
    : pulses(p), steps(s), depth(d)
{
    // Clamp di sicurezza
    if (steps < 1) steps = 1;
    pulses = juce::jlimit(0, steps, pulses);
}

std::vector<int> HyperEuclidean::generateSequence() {
    computeHyperEuclidean();
    return sequence;
}

// Algoritmo di Bjorklund interno (ex Euclidean base)
std::vector<int> HyperEuclidean::generateClassic(int p, int s) {
    std::vector<int> seq;
    if (s <= 0) return seq;
    if (p <= 0) { seq.resize(s, 0); return seq; }
    if (p >= s) { seq.resize(s, 1); return seq; }

    std::vector<int> counts;
    std::vector<int> remainders;
    int divisor = s - p;
    remainders.push_back(p);
    int level = 0;

    while (true) {
        counts.push_back(divisor / remainders[level]);
        remainders.push_back(divisor % remainders[level]);
        divisor = remainders[level];
        level++;
        if (remainders[level] <= 1) break;
    }
    counts.push_back(divisor);

    std::function<void(int)> build = [&](int lvl) {
        if (lvl == -1) seq.push_back(0);
        else if (lvl == -2) seq.push_back(1);
        else {
            for (int i = 0; i < counts[lvl]; ++i) build(lvl - 1);
            if (remainders[lvl] != 0) build(lvl - 2);
        }
        };

    build(level);
    if ((int)seq.size() > s) seq.resize(s);
    while ((int)seq.size() < s) seq.push_back(0);
    return seq;
}

void HyperEuclidean::computeHyperEuclidean() {
    sequence.clear();
    velocities.clear();

    // 1. Generazione base
    std::vector<int> onsets;
    std::vector<int> basePattern = generateClassic(pulses, steps);

    for (int i = 0; i < (int)basePattern.size(); ++i)
        if (basePattern[i] == 1) onsets.push_back(i);

    if (onsets.empty()) {
        sequence.resize(steps, 0);
        velocities.resize(steps, 0);
        return;
    }

    // 2. Logica Hyper (Riduzione IOI)
    for (int d = 1; d < depth; ++d) {
        int n = (int)onsets.size();
        if (n <= 1) break;

        std::vector<int> ioi;
        for (int i = 0; i < n - 1; ++i) ioi.push_back(onsets[i + 1] - onsets[i]);
        ioi.push_back(steps - onsets.back() + onsets.front());

        int k = std::max(1, n / 2);
        std::vector<int> selector = generateClassic(k, n);

        std::vector<int> nextOnsets;
        for (int i = 0; i < n; ++i) {
            if (selector[i] == 1) nextOnsets.push_back(onsets[i]);
        }
        onsets = nextOnsets;
    }

    // 3. Ricostruzione finale e Velocity
    sequence.assign(steps, 0);
    velocities.assign(steps, 0);

    if (onsets.empty()) return;

    // Calcolo IOI per accentuazione
    std::vector<int> finalIoi;
    for (size_t i = 0; i < onsets.size(); ++i) {
        int nextIdx = (i + 1) % onsets.size();
        int diff = onsets[nextIdx] - onsets[i];
        if (diff <= 0) diff += steps;
        finalIoi.push_back(diff);
    }

    int maxIOI = 0;
    for (int val : finalIoi) if (val > maxIOI) maxIOI = val;

    for (size_t i = 0; i < onsets.size(); ++i) {
        int idx = onsets[i];
        sequence[idx] = 1;
        float norm = (maxIOI > 0) ? (float)finalIoi[i] / (float)maxIOI : 1.0f;
        velocities[idx] = (int)(40 + norm * 75);
    }
}