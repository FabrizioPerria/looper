#pragma once
#include "audio/EngineCommandBus.h"
#include <JuceHeader.h>
#include <functional>
#include <map>
#include <vector>

enum class AutomationMode
{
    LoopBased, // Applied on wrap (speed, pitch, etc.)
    TimeBased  // Applied continuously (metronome, volume fades)
};

struct AutomationCurve
{
    std::vector<juce::Point<float>> breakpoints; // x = loop index, y = parameter value
    EngineMessageBus::CommandType commandType;
    int trackIndex;
    bool enabled = true;
    AutomationMode mode = AutomationMode::LoopBased;
    double startTime = 0.0;

    float getValueAtLoopIndex (int loopIndex) const
    {
        if (breakpoints.empty()) return 0.0f;
        if (loopIndex <= 0 || breakpoints.size() == 1) return breakpoints[0].getY();

        int clampedIndex = juce::jmin (loopIndex, (int) breakpoints.size() - 1);
        return breakpoints[(size_t) clampedIndex].getY();
    }

    float getValueAtTime (double elapsedSeconds) const
    {
        if (breakpoints.empty()) return 0.0f;

        // Find position in curve based on elapsed time
        // breakpoints[i].x = time in seconds
        // breakpoints[i].y = parameter value

        for (size_t i = 0; i < breakpoints.size() - 1; ++i)
        {
            if (elapsedSeconds >= breakpoints[i].x && elapsedSeconds < breakpoints[i + 1].x)
            {
                // Linear interpolation between points
                float t = (elapsedSeconds - breakpoints[i].x) / (breakpoints[i + 1].x - breakpoints[i].x);
                return juce::jmap (t, breakpoints[i].y, breakpoints[i + 1].y);
            }
        }

        return breakpoints.back().y; // Past end, use final value
    }
};

struct ParameterCoupling
{
    juce::String sourceParamId;
    juce::String targetParamId;
    std::function<float (float)> transform;
    bool enabled = true;
};

class AutomationEngine
{
public:
    AutomationEngine (EngineMessageBus* messageBus) : engineMessageBus (messageBus) {}

    void prepareToPlay (double sampleRate)
    {
        this->sampleRate = sampleRate;
        elapsedSamples = 0;
    }

    void processBlock (int numSamples) // Called every audio block
    {
        elapsedSamples += numSamples;
        double elapsedSeconds = elapsedSamples / sampleRate;

        // Apply time-based automation
        for (const auto& [paramId, curve] : curves)
        {
            if (! curve.enabled || curve.mode != AutomationMode::TimeBased) continue;

            float value = curve.getValueAtTime (elapsedSeconds - curve.startTime);
            dispatchCommand (curve.commandType, curve.trackIndex, value);
        }
    }

    void registerCurve (const juce::String& paramId, const AutomationCurve& curve) { curves[paramId] = curve; }

    void removeCurve (const juce::String& paramId) { curves.erase (paramId); }

    void enableCurve (const juce::String& paramId, bool enabled)
    {
        if (curves.count (paramId)) curves[paramId].enabled = enabled;
    }

    void registerCoupling (const ParameterCoupling& coupling) { couplings.push_back (coupling); }

    void removeCoupling (const juce::String& sourceId, const juce::String& targetId)
    {
        couplings.erase (std::remove_if (couplings.begin(),
                                         couplings.end(),
                                         [&] (const ParameterCoupling& c)
                                         { return c.sourceParamId == sourceId && c.targetParamId == targetId; }),
                         couplings.end());
    }

    void enableCoupling (const juce::String& sourceId, const juce::String& targetId, bool enabled)
    {
        for (auto& coupling : couplings)
        {
            if (coupling.sourceParamId == sourceId && coupling.targetParamId == targetId)
            {
                coupling.enabled = enabled;
                break;
            }
        }
    }

    void applyAtLoopIndex (int trackIndex, int loopIndex)
    {
        std::map<juce::String, float> evaluatedParams;

        // First pass: evaluate curves
        for (const auto& [paramId, curve] : curves)
        {
            if (! curve.enabled || curve.trackIndex != trackIndex) continue;

            evaluatedParams[paramId] = curve.getValueAtLoopIndex (loopIndex);
        }

        // Second pass: apply couplings
        for (const auto& coupling : couplings)
        {
            if (! coupling.enabled) continue;

            if (evaluatedParams.count (coupling.sourceParamId))
            {
                float sourceValue = evaluatedParams[coupling.sourceParamId];
                float targetValue = coupling.transform (sourceValue);

                // Only apply if target doesn't have explicit automation
                if (! curves.count (coupling.targetParamId) || ! curves[coupling.targetParamId].enabled)
                {
                    evaluatedParams[coupling.targetParamId] = targetValue;
                }
            }
        }

        // Third pass: dispatch commands
        for (const auto& [paramId, curve] : curves)
        {
            if (curve.trackIndex != trackIndex) continue;

            if (evaluatedParams.count (paramId))
            {
                dispatchCommand (curve.commandType, trackIndex, evaluatedParams[paramId]);
            }
        }

        // Dispatch coupled parameters that don't have curves
        for (const auto& coupling : couplings)
        {
            if (! coupling.enabled) continue;
            if (curves.count (coupling.targetParamId)) continue; // Has curve, already dispatched

            if (evaluatedParams.count (coupling.targetParamId))
            {
                // Need to find command type - store in coupling or registry
                // For now, skipping this case - coupled params should have registered curves
            }
        }
    }

    void startTimeBasedAutomation (const juce::String& paramId)
    {
        if (curves.count (paramId))
        {
            curves[paramId].startTime = elapsedSamples / sampleRate;
        }
    }

    void stopTimeBasedAutomation (const juce::String& paramId)
    {
        if (curves.count (paramId))
        {
            curves[paramId].enabled = false;
        }
    }

    void clear()
    {
        curves.clear();
        couplings.clear();
    }

    const std::map<juce::String, AutomationCurve>& getCurves() const { return curves; }
    const std::vector<ParameterCoupling>& getCouplings() const { return couplings; }

private:
    EngineMessageBus* engineMessageBus;
    std::map<juce::String, AutomationCurve> curves;
    std::vector<ParameterCoupling> couplings;
    double sampleRate;
    uint64_t elapsedSamples = 0;

    void dispatchCommand (EngineMessageBus::CommandType commandType, int trackIndex, float value)
    {
        if (! engineMessageBus) return;

        EngineMessageBus::Command cmd;
        cmd.type = commandType;
        cmd.trackIndex = trackIndex;
        cmd.payload = value;

        engineMessageBus->pushCommand (cmd);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutomationEngine)
};
