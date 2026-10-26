#pragma once

#include <juce_dsp/juce_dsp.h>

namespace rr::dsp
{
struct BiquadCoefficients
{
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;

    static BiquadCoefficients unity() noexcept { return {}; }

    static BiquadCoefficients peaking (double sampleRate, double freq, double q, double gainDb) noexcept;
    static BiquadCoefficients lowShelf (double sampleRate, double freq, double gainDb, double slope = 1.0) noexcept;
    static BiquadCoefficients highShelf (double sampleRate, double freq, double gainDb, double slope = 1.0) noexcept;
    static BiquadCoefficients highPass (double sampleRate, double freq, double q = 0.7071) noexcept;
    static BiquadCoefficients lowPass (double sampleRate, double freq, double q = 0.7071) noexcept;
    static BiquadCoefficients bandPass (double sampleRate, double freq, double q) noexcept;

    float getMagnitudeForFrequency (double freq, double sampleRate) const noexcept;
};

struct BiquadState
{
    float z1 = 0.0f, z2 = 0.0f;

    void reset() noexcept { z1 = z2 = 0.0f; }

    inline float process (float x, const BiquadCoefficients& c) noexcept
    {
        auto y = c.b0 * x + z1;
        z1 = c.b1 * x - c.a1 * y + z2;
        z2 = c.b2 * x - c.a2 * y;
        return y;
    }
};

class BiquadFilter
{
public:
    void setCoefficients (const BiquadCoefficients& newCoefficients) noexcept { coefficients = newCoefficients; }
    const BiquadCoefficients& getCoefficients() const noexcept { return coefficients; }
    void reset() noexcept { state.reset(); }

    inline float processSample (float x) noexcept { return state.process (x, coefficients); }

    float getMagnitudeForFrequency (double freq, double sampleRate) const noexcept
    {
        return coefficients.getMagnitudeForFrequency (freq, sampleRate);
    }

private:
    BiquadCoefficients coefficients;
    BiquadState state;
};

} // namespace rr::dsp
