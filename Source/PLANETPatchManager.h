/*
  ==============================================================================
    PLANETPatchManager.h
    Manages patch loading/saving in human-readable markdown format
  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>
#include <map>
#include <vector>

//==============================================================================
// PATCH DATA STRUCTURE
//==============================================================================

struct PLANETPatch {
    // Metadata
    juce::String patchName;
    juce::String description;
    juce::String tags;
    juce::String category;  // e.g., "Pads", "Plucks", "Leads"

    // Parameter values stored as map: parameterID -> value
    std::map<juce::String, float> parameters;

    PLANETPatch() = default;

    void clear() {
        patchName = "";
        description = "";
        tags = "";
        category = "";
        parameters.clear();
    }
};

//==============================================================================
// PATCH MANAGER CLASS
//==============================================================================

class PLANETPatchManager {
public:
    PLANETPatchManager();
    ~PLANETPatchManager() = default;

    // ======================== LOADING PATCHES ========================

    // Load a single patch from a markdown file
    bool loadPatchFromFile(const juce::File& file, PLANETPatch& outPatch);

    // Load all patches from a directory and its subdirectories
    void scanPatchLibrary(const juce::File& rootDirectory);

    // Get list of all available patches
    const std::vector<PLANETPatch>& getAllPatches() const { return patchLibrary; }

    // Get patches filtered by category
    std::vector<PLANETPatch> getPatchesByCategory(const juce::String& category) const;

    // Get patch by name
    const PLANETPatch* getPatchByName(const juce::String& name) const;

    // ======================== SAVING PATCHES ========================

    // Save a patch to a markdown file
    bool savePatchToFile(const PLANETPatch& patch, const juce::File& file);

    // Create a patch from current processor state
    PLANETPatch createPatchFromProcessor(juce::AudioProcessorValueTreeState& apvts,
                                          const juce::String& name,
                                          const juce::String& description,
                                          const juce::String& tags,
                                          const juce::String& category);

    // Apply a patch to the processor
    void applyPatchToProcessor(const PLANETPatch& patch,
                                juce::AudioProcessorValueTreeState& apvts);

    // ======================== UTILITY ========================

    // Get all unique categories from loaded patches
    juce::StringArray getCategories() const;

    // Get default patch library path (Documents/PLANET2026/Patches)
    static juce::File getDefaultPatchDirectory();

    // Create default directory structure (Pads, Plucks, Leads, etc.)
    static void createDefaultDirectoryStructure(const juce::File& rootDirectory);

private:
    std::vector<PLANETPatch> patchLibrary;

    // Helper functions for markdown parsing
    juce::String extractTitle(const juce::String& markdownContent);
    juce::String extractDescription(const juce::String& markdownContent);
    juce::String extractTags(const juce::String& markdownContent);
    void parseParameters(const juce::String& markdownContent, PLANETPatch& patch);

    // Helper functions for markdown generation
    juce::String generateMarkdownHeader(const PLANETPatch& patch);
    juce::String generateParameterSection(const juce::String& sectionName,
                                           const juce::StringArray& parameterIDs,
                                           const PLANETPatch& patch);

    // Parameter organization helpers
    struct ParameterRange {
        float min;
        float max;
        juce::String unit;

        ParameterRange(float minVal = 0.0f, float maxVal = 1.0f, const juce::String& unitStr = "")
            : min(minVal), max(maxVal), unit(unitStr) {}
    };

    std::map<juce::String, ParameterRange> getParameterRanges();
    juce::String formatParameterValue(float value, const juce::String& parameterID);
};
