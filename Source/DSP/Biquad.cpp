#include "DSP/Biquad.h"

#include <cmath>
#include <complex>

namespace rr::dsp
{
namespace
{
    constexpr double kPi = 3.14159265358979323846;

    double clampFrequency (double sampleRate, double freq) noexcept
    {
        return juce::jlimit (10.0, sampleRate * 0.49, freq);
    }

    BiquadCoefficients normalise (double b0, double b1, double b2, double a0, double a1, double a2) noexcept
    {
        if (std::abs (a0) < 1.0e-12)
            return BiquadCoefficients::unity();

        BiquadCoefficients c;
        c.b0 = (float) (b0 / a0);
        c.b1 = (float) (b1 / a0);
        c.b2 = (float) (b2 / a0);
        c.a1 = (float) (a1 / a0);
        c.a2 = (float) (a2 / a0);
        return c;
    }
}

BiquadCoefficients BiquadCoefficients::peaking (double sampleRate, double freq, double q, double gainDb) noexcept
{
    freq = clampFrequency (sampleRate, freq);
    q = juce::jlimit (0.05, 20.0, q);

    const double a = std::pow (10.0, gainDb / 40.0);
    const double w0 = 2.0 * kPi * freq / sampleRate;
    const double alpha = std::sin (w0) / (2.0 * q);
    const double cosw0 = std::cos (w0);

    return normalise (1.0 + alpha * a,
                      1.0 - alpha * a,
                      1.0 - alpha / a,
                      1.0 + alpha / a,
                      -2.0 * cosw0,
                      1.0 - alpha / a);
}

BiquadCoefficients BiquadCoefficients::lowShelf (double sampleRate, double freq, double gainDb, double slope) noexcept
{
    freq = clampFrequency (sampleRate, freq);
    slope = juce::jlimit (0.1, 2.0, slope);

    const double a = std::pow (10.0, gainDb / 40.0);
    const double w0 = 2.0 * kPi * freq / sampleRate;
    const double cosw0 = std::cos (w0);
    const double sinw0 = std::sin (w0);
    const double alpha = sinw0 * 0.5 * std::sqrt ((a + 1.0 / a) * (1.0 / slope - 1.0) + 2.0);
    const double twoSqrtAAlpha = 2.0 * std::sqrt (a) * alpha;

    return normalise (a * ((a + 1.0) - (a - 1.0) * cosw0 + twoSqrtAAlpha),
                      2.0 * a * ((a - 1.0) - (a + 1.0) * cosw0),
                      a * ((a + 1.0) - (a - 1.0) * cosw0 - twoSqrtAAlpha),
                      (a + 1.0) + (a - 1.0) * cosw0 + twoSqrtAAlpha,
                      -2.0 * ((a - 1.0) + (a + 1.0) * cosw0),
                      (a + 1.0) + (a - 1.0) * cosw0 - twoSqrtAAlpha);
}

BiquadCoefficients BiquadCoefficients::highShelf (double sampleRate, double freq, double gainDb, double slope) noexcept
{
    freq = clampFrequency (sampleRate, freq);
    slope = juce::jlimit (0.1, 2.0, slope);

    const double a = std::pow (10.0, gainDb / 40.0);
    const double w0 = 2.0 * kPi * freq / sampleRate;
    const double cosw0 = std::cos (w0);
    const double sinw0 = std::sin (w0);
    const double alpha = sinw0 * 0.5 * std::sqrt ((a + 1.0 / a) * (1.0 / slope - 1.0) + 2.0);
    const double twoSqrtAAlpha = 2.0 * std::sqrt (a) * alpha;

    return normalise (a * ((a + 1.0) + (a - 1.0) * cosw0 + twoSqrtAAlpha),
                      -2.0 * a * ((a - 1.0) + (a + 1.0) * cosw0),
                      a * ((a + 1.0) + (a - 1.0) * cosw0 - twoSqrtAAlpha),
                      (a + 1.0) - (a - 1.0) * cosw0 + twoSqrtAAlpha,
                      2.0 * ((a - 1.0) - (a + 1.0) * cosw0),
                      (a + 1.0) - (a - 1.0) * cosw0 - twoSqrtAAlpha);
}

BiquadCoefficients BiquadCoefficients::highPass (double sampleRate, double freq, double q) noexcept
{
    freq = clampFrequency (sampleRate, freq);
    q = juce::jlimit (0.05, 20.0, q);

    const double w0 = 2.0 * kPi * freq / sampleRate;
    const double cosw0 = std::cos (w0);
    const double alpha = std::sin (w0) / (2.0 * q);

    return normalise ((1.0 + cosw0) * 0.5,
                      -(1.0 + cosw0),
                      (1.0 + cosw0) * 0.5,
                      1.0 + alpha,
                      -2.0 * cosw0,
                      1.0 - alpha);
}

BiquadCoefficients BiquadCoefficients::lowPass (double sampleRate, double freq, double q) noexcept
{
    freq = clampFrequency (sampleRate, freq);
    q = juce::jlimit (0.05, 20.0, q);

    const double w0 = 2.0 * kPi * freq / sampleRate;
    const double cosw0 = std::cos (w0);
    const double alpha = std::sin (w0) / (2.0 * q);

    return normalise ((1.0 - cosw0) * 0.5,
                      1.0 - cosw0,
                      (1.0 - cosw0) * 0.5,
                      1.0 + alpha,
                      -2.0 * cosw0,
                      1.0 - alpha);
}

BiquadCoefficients BiquadCoefficients::bandPass (double sampleRate, double freq, double q) noexcept
{
    freq = clampFrequency (sampleRate, freq);
    q = juce::jlimit (0.05, 20.0, q);

    const double w0 = 2.0 * kPi * freq / sampleRate;
    const double cosw0 = std::cos (w0);
    const double alpha = std::sin (w0) / (2.0 * q);

    return normalise (alpha, 0.0, -alpha, 1.0 + alpha, -2.0 * cosw0, 1.0 - alpha);
}

float BiquadCoefficients::getMagnitudeForFrequency (double freq, double sampleRate) const noexcept
{
    const auto w = 2.0 * kPi * freq / sampleRate;
    const std::complex<double> z1 = std::polar (1.0, -w);
    const std::complex<double> z2 = std::polar (1.0, -2.0 * w);
    const auto numerator = (double) b0 + (double) b1 * z1 + (double) b2 * z2;
    const auto denominator = 1.0 + (double) a1 * z1 + (double) a2 * z2;

    if (std::abs (denominator) < 1.0e-12)
        return 0.0f;

    return (float) std::abs (numerator / denominator);
}

} // namespace rr::dsp
