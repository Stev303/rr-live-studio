#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <vector>

namespace rr::music
{
inline constexpr int numKeys = 12;
inline constexpr int numScales = 4;

inline const char* const keyNames[numKeys] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
inline const char* const scaleNames[numScales] = { "Major", "Natural Minor", "Harmonic Minor", "Chromatic" };
inline const char* const intervalNames[4] = { "3rd", "5th", "Octave", "Custom" };

inline int scaleDegreeCount (int scale) noexcept
{
    switch (scale)
    {
        case 0:  return 7; // major
        case 1:  return 7; // natural minor
        case 2:  return 7; // harmonic minor
        default: return 12;
    }
}

inline int scaleSemitone (int scale, int degreeIndex) noexcept
{
    static const int major[7]          = { 0, 2, 4, 5, 7, 9, 11 };
    static const int naturalMinor[7]   = { 0, 2, 3, 5, 7, 8, 10 };
    static const int harmonicMinor[7]  = { 0, 2, 3, 5, 7, 8, 11 };

    const auto count = scaleDegreeCount (scale);
    const auto wrapped = ((degreeIndex % count) + count) % count;

    switch (scale)
    {
        case 0: return major[wrapped];
        case 1: return naturalMinor[wrapped];
        case 2: return harmonicMinor[wrapped];
        default: return wrapped; // chromatic
    }
}

inline bool isMidiInScale (int midiNote, int key, int scale) noexcept
{
    if (scale == 3)
        return true;

    const auto pitchClass = ((midiNote - key) % 12 + 12) % 12;

    for (int i = 0; i < scaleDegreeCount (scale); ++i)
        if (scaleSemitone (scale, i) == pitchClass)
            return true;

    return false;
}

inline int nearestScaleNote (int midiNote, int key, int scale) noexcept
{
    if (scale == 3)
        return midiNote;

    for (int distance = 0; distance <= 6; ++distance)
    {
        if (isMidiInScale (midiNote - distance, key, scale))
            return midiNote - distance;

        if (distance > 0 && isMidiInScale (midiNote + distance, key, scale))
            return midiNote + distance;
    }

    return midiNote;
}

inline int absoluteScaleIndex (int midiNote, int key, int scale) noexcept
{
    int index = 0;

    for (int note = midiNote - 1; note >= midiNote - 48; --note)
        if (isMidiInScale (note, key, scale))
            ++index;

    return index;
}

inline int scaleDegreeForMidi (const int midiNote, const int key, const int scale) noexcept
{
    return absoluteScaleIndex (midiNote, key, scale);
}

inline int transposeNoteByScaleSteps (int midiNote, int key, int scale, int steps) noexcept
{
    if (scale == 3)
        return midiNote + steps;

    int note = nearestScaleNote (midiNote, key, scale);
    const auto direction = steps >= 0 ? 1 : -1;
    auto remaining = std::abs (steps);

    while (remaining > 0)
    {
        do
        {
            note += direction;
        }
        while (! isMidiInScale (note, key, scale));

        --remaining;
    }

    return note;
}

inline juce::String noteNameForMidi (int midiNote)
{
    int pitchClass = midiNote % 12;

    if (pitchClass < 0)
        pitchClass += 12;

    const auto octave = (midiNote / 12) - 1;
    return juce::String (keyNames[pitchClass]) + juce::String (octave);
}

inline juce::String keyAndScaleName (int key, int scale)
{
    return juce::String (keyNames[((key % 12) + 12) % 12]) + " " + scaleNames[juce::jlimit (0, 3, scale)];
}

inline int intervalToScaleSteps (int intervalIndex) noexcept
{
    switch (intervalIndex)
    {
        case 0:  return 2; // third
        case 1:  return 4; // fifth
        case 2:  return 7; // octave
        default: return 0;
    }
}

} // namespace rr::music
