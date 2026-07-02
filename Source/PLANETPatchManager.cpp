/*
  ==============================================================================
    PLANETPatchManager.cpp
    Implementation of patch management system
  ==============================================================================
*/

#include "PLANETPatchManager.h"

//==============================================================================
PLANETPatchManager::PLANETPatchManager() {
}

//==============================================================================
// LOADING PATCHES
//==============================================================================

bool PLANETPatchManager::loadPatchFromFile(const juce::File& file, PLANETPatch& outPatch) {
    if (!file.existsAsFile() || file.getFileExtension() != ".md")
        return false;

    juce::String content = file.loadFileAsString();
    if (content.isEmpty())
        return false;

    outPatch.clear();

    // Extract metadata
    outPatch.patchName = extractTitle(content);
    outPatch.description = extractDescription(content);
    outPatch.tags = extractTags(content);

    // Determine category from parent folder
    juce::File parentDir = file.getParentDirectory();
    outPatch.category = parentDir.getFileName();

    // Parse all parameters
    parseParameters(content, outPatch);

    return true;
}

void PLANETPatchManager::scanPatchLibrary(const juce::File& rootDirectory) {
    patchLibrary.clear();

    if (!rootDirectory.isDirectory())
        return;

    // Recursively find all .md files
    juce::Array<juce::File> patchFiles;
    rootDirectory.findChildFiles(patchFiles, juce::File::findFiles, true, "*.md");

    for (const auto& file : patchFiles) {
        PLANETPatch patch;
        if (loadPatchFromFile(file, patch)) {
            patchLibrary.push_back(patch);
        }
    }
}

std::vector<PLANETPatch> PLANETPatchManager::getPatchesByCategory(const juce::String& category) const {
    std::vector<PLANETPatch> filtered;
    for (const auto& patch : patchLibrary) {
        if (patch.category.equalsIgnoreCase(category)) {
            filtered.push_back(patch);
        }
    }
    return filtered;
}

const PLANETPatch* PLANETPatchManager::getPatchByName(const juce::String& name) const {
    for (const auto& patch : patchLibrary) {
        if (patch.patchName.equalsIgnoreCase(name)) {
            return &patch;
        }
    }
    return nullptr;
}

//==============================================================================
// SAVING PATCHES
//==============================================================================

bool PLANETPatchManager::savePatchToFile(const PLANETPatch& patch, const juce::File& file) {
    if (patch.patchName.isEmpty())
        return false;

    juce::String markdown;

    // Generate header with title, description, and tags
    markdown << generateMarkdownHeader(patch);
    markdown << "\n---\n\n";

    // Generate parameter sections in organized groups
    markdown << generateParameterSection("Amplitude Envelope",
        { "ampEnvAttackTime", "ampEnvDecayTime", "ampEnvSustainLevel", "ampEnvReleaseTime", "exponentialControl" },
        patch);

    markdown << generateParameterSection("Velocity Response",
        { "velToAmplitude", "velToAttackTime", "vintageAmount" },
        patch);

    markdown << generateParameterSection("Colour",
        { "brilliance", "carrierMorph", "brillianceModWheel", "carrierMorphModWheel" },
        patch);

    markdown << generateParameterSection("Vibrato",
        { "vibratoRate", "vibratoDepth", "vibratoFadeIn" },
        patch);

    markdown << generateParameterSection("Pitch Envelope",
        { "pitchEnvDistance", "pitchEnvTime" },
        patch);

    markdown << generateParameterSection("Effects",
        { "detuneAmount", "detuneMix", "warmth", "punch", "punchFrequency" },
        patch);

    markdown << "\n---\n\n";

    // Add parameter reference section
    markdown << "## Drawbar Parameters Reference\n";
    markdown << "# k = -2.0 to 2.0\n";
    markdown << "# EnvelopeAmount = -5.0 to 20.0\n";
    markdown << "# LFOAmount = -5.0 to 5.0\n";
    markdown << "# input_f = 0.5 to 30.0 (0.5 steps)\n";
    markdown << "# AttackTime = 0.001 to 10.0\n";
    markdown << "# DecayTime = 0.001 to 10.0\n";
    markdown << "# SustainLevel = 0.0 to 2.0\n";
    markdown << "# ReleaseTime = 0.001 to 10.0\n";
    markdown << "# LFOShape = 1, 2, or 3 (Sine, Triangle, Square)\n";
    markdown << "# LFORate = 0.05 to 1000.0\n";
    markdown << "# LFOSync = 0 (Free) or 1 (Tempo Sync)\n";
    markdown << "# LFOSyncDiv = 0-12 (4/1, 2/1, 1/1, 1/2., 1/2, 1/2T, 1/4., 1/4, 1/4T, 1/8., 1/8, 1/8T, 1/16)\n\n";
    markdown << "# VelToHarmonic = -100 to 100\n";
    markdown << "# ToPM = 0 (off) or 1 (on) - route drawbar to phase-distortion path\n";
    markdown << "# ToOut = 0 (off) or 1 (on) - route drawbar direct to output (additive partial)\n";
    markdown << "# TrigSingle = 0 (Multi/retrigger) or 1 (Single/phrase-start) - single-trigger envelope (Hammond perc)\n";

    // Generate all 10 drawbar sections
    for (int i = 1; i <= 10; ++i) {
        juce::String drawbarNum = juce::String(i);
        markdown << "## Drawbar " << drawbarNum << "\n";

        juce::StringArray drawbarParams = {
            "k" + drawbarNum,
            "k" + drawbarNum + "EnvelopeAmount",
            "k" + drawbarNum + "LFOAmount",
            "k" + drawbarNum + "VelToHarmonic",
            "input_f" + drawbarNum,
            "k" + drawbarNum + "AttackTime",
            "k" + drawbarNum + "DecayTime",
            "k" + drawbarNum + "SustainLevel",
            "k" + drawbarNum + "ReleaseTime",
            "k" + drawbarNum + "LFOShape",
            "k" + drawbarNum + "LFORate",
            "k" + drawbarNum + "LFOSync",
            "k" + drawbarNum + "LFOSyncDiv",
            "k" + drawbarNum + "ToPM",
            "k" + drawbarNum + "ToOut",
            "k" + drawbarNum + "TrigSingle"
        };

        for (const auto& paramID : drawbarParams) {
            if (patch.parameters.count(paramID) > 0) {
                float value = patch.parameters.at(paramID);
                auto ranges = getParameterRanges();
                auto range = ranges.count(paramID) > 0 ? ranges[paramID] : ParameterRange();

                markdown << paramID << " = " << formatParameterValue(value, paramID);
                markdown << "  (" << juce::String(range.min, 3) << " to " << juce::String(range.max, 1) << ")\n";
            }
        }

        markdown << "\n";
    }

    // Write to file
    return file.replaceWithText(markdown);
}

PLANETPatch PLANETPatchManager::createPatchFromProcessor(
    juce::AudioProcessorValueTreeState& apvts,
    const juce::String& name,
    const juce::String& description,
    const juce::String& tags,
    const juce::String& category) {

    PLANETPatch patch;
    patch.patchName = name;
    patch.description = description;
    patch.tags = tags;
    patch.category = category;

    // Get all parameters from APVTS and store their current values
    for (auto* param : apvts.processor.getParameters()) {
        if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(param)) {
            juce::String paramID = parameter->getParameterID();
            float normalizedValue = parameter->getValue();  // 0.0 to 1.0
            // Use NormalisableRange to convert normalized to actual value
            float actualValue = parameter->getNormalisableRange().convertFrom0to1(normalizedValue);
            patch.parameters[paramID] = actualValue;
        }
    }

    return patch;
}

void PLANETPatchManager::applyPatchToProcessor(const PLANETPatch& patch,
                                                juce::AudioProcessorValueTreeState& apvts) {
    // A patch describes the COMPLETE instrument state. Parameters the file doesn't
    // mention (typically ones added to ISHTAR after the patch was saved, e.g. LIFE)
    // must return to their defaults - not silently keep whatever the previous setup
    // left behind, which made old "static" patches inherit the prior patch's Life.
    for (auto* param : apvts.processor.getParameters())
        if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(param))
            parameter->setValueNotifyingHost(parameter->getDefaultValue());

    // Apply all parameter values from the patch to the processor
    for (const auto& [paramID, actualValue] : patch.parameters) {
        // Get parameter and convert actual value to normalized
        if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(paramID))) {
            // normalizeRange converts actual value to 0-1 range
            float normalizedValue = parameter->getNormalisableRange().convertTo0to1(actualValue);
            parameter->setValueNotifyingHost(normalizedValue);
        }
    }
}

//==============================================================================
// UTILITY FUNCTIONS
//==============================================================================

juce::StringArray PLANETPatchManager::getCategories() const {
    juce::StringArray categories;
    for (const auto& patch : patchLibrary) {
        if (!categories.contains(patch.category)) {
            categories.add(patch.category);
        }
    }
    return categories;
}

juce::File PLANETPatchManager::getDefaultPatchDirectory() {
    juce::File documentsDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    return documentsDir.getChildFile("PLANET2026").getChildFile("Patches");
}

void PLANETPatchManager::createDefaultDirectoryStructure(const juce::File& rootDirectory) {
    if (!rootDirectory.exists()) {
        rootDirectory.createDirectory();
    }

    // Create default category folders
    juce::StringArray defaultCategories = { "Pads", "Plucks", "Leads", "Bass", "Keys", "FX", "User" };

    for (const auto& category : defaultCategories) {
        juce::File categoryDir = rootDirectory.getChildFile(category);
        if (!categoryDir.exists()) {
            categoryDir.createDirectory();
        }
    }
}

//==============================================================================
// MARKDOWN PARSING HELPERS
//==============================================================================

juce::String PLANETPatchManager::extractTitle(const juce::String& markdownContent) {
    // Extract text after first "# "
    int titleStart = markdownContent.indexOf("# ");
    if (titleStart < 0)
        return "Untitled Patch";

    int titleEnd = markdownContent.indexOfChar(titleStart, '\n');
    if (titleEnd < 0)
        titleEnd = markdownContent.length();

    return markdownContent.substring(titleStart + 2, titleEnd).trim();
}

juce::String PLANETPatchManager::extractDescription(const juce::String& markdownContent) {
    // Extract text between title and first "Tags:" or "---"
    int titleEnd = markdownContent.indexOf("# ");
    if (titleEnd < 0)
        return "";

    titleEnd = markdownContent.indexOfChar(titleEnd, '\n');
    if (titleEnd < 0)
        return "";

    int descEnd = markdownContent.substring(titleEnd).indexOf("Tags:");
    if (descEnd >= 0)
        descEnd += titleEnd;
    else
        descEnd = markdownContent.substring(titleEnd).indexOf("---");

    if (descEnd >= 0)
        descEnd += titleEnd;

    if (descEnd < 0)
        return "";

    return markdownContent.substring(titleEnd, descEnd).trim();
}

juce::String PLANETPatchManager::extractTags(const juce::String& markdownContent) {
    int tagsStart = markdownContent.indexOf("Tags:");
    if (tagsStart < 0)
        return "";

    int tagsEnd = markdownContent.indexOfChar(tagsStart, '\n');
    if (tagsEnd < 0)
        tagsEnd = markdownContent.length();

    return markdownContent.substring(tagsStart + 5, tagsEnd).trim();
}

void PLANETPatchManager::parseParameters(const juce::String& markdownContent, PLANETPatch& patch) {
    // Parse parameter lines in format: "parameterName = value  (min to max)"
    juce::StringArray lines = juce::StringArray::fromLines(markdownContent);

    for (const auto& line : lines) {
        // Look for lines with " = " (parameter assignments)
        int equalPos = line.indexOf(" = ");
        if (equalPos < 0)
            continue;

        // Skip comment lines
        if (line.trimStart().startsWith("#"))
            continue;

        juce::String paramID = line.substring(0, equalPos).trim();
        juce::String valueStr = line.substring(equalPos + 3).trim();

        // Extract numeric value (before any parentheses)
        int parenPos = valueStr.indexOf("(");
        if (parenPos > 0)
            valueStr = valueStr.substring(0, parenPos).trim();

        float value = valueStr.getFloatValue();
        patch.parameters[paramID] = value;
    }
}

//==============================================================================
// MARKDOWN GENERATION HELPERS
//==============================================================================

juce::String PLANETPatchManager::generateMarkdownHeader(const PLANETPatch& patch) {
    juce::String header;
    header << "# " << patch.patchName << "\n\n";
    header << patch.description << "\n\n";
    if (patch.tags.isNotEmpty()) {
        header << "Tags: " << patch.tags << "\n";
    }
    return header;
}

juce::String PLANETPatchManager::generateParameterSection(
    const juce::String& sectionName,
    const juce::StringArray& parameterIDs,
    const PLANETPatch& patch) {

    juce::String section;
    section << "## " << sectionName << "\n";

    auto ranges = getParameterRanges();

    for (const auto& paramID : parameterIDs) {
        if (patch.parameters.count(paramID) > 0) {
            float value = patch.parameters.at(paramID);
            auto range = ranges.count(paramID) > 0 ? ranges[paramID] : ParameterRange();

            section << paramID << " = " << formatParameterValue(value, paramID);
            section << "  (" << juce::String(range.min, 3) << " to " << juce::String(range.max, 1) << ")\n";
        }
    }

    section << "\n";
    return section;
}

std::map<juce::String, PLANETPatchManager::ParameterRange> PLANETPatchManager::getParameterRanges() {
    std::map<juce::String, ParameterRange> ranges;

    // Amplitude Envelope
    ranges["ampEnvAttackTime"] = ParameterRange(0.001f, 10.0f);
    ranges["ampEnvDecayTime"] = ParameterRange(0.001f, 10.0f);
    ranges["ampEnvSustainLevel"] = ParameterRange(0.0f, 1.0f);
    ranges["ampEnvReleaseTime"] = ParameterRange(0.001f, 10.0f);
    ranges["exponentialControl"] = ParameterRange(0.0f, 1.0f);

    // Velocity Response
    ranges["velToAmplitude"] = ParameterRange(0.0f, 200.0f);
    
    ranges["velToAttackTime"] = ParameterRange(0.0f, 100.0f);
    ranges["vintageAmount"] = ParameterRange(0.0f, 100.0f);

    // Colour (Brilliance + Density) + their mod-wheel modes (0 Off / 1 Normal / 2 Inverse)
    ranges["brilliance"] = ParameterRange(0.0f, 1.0f);
    ranges["carrierMorph"] = ParameterRange(0.0f, 1.0f);
    ranges["brillianceModWheel"] = ParameterRange(0.0f, 2.0f);
    ranges["carrierMorphModWheel"] = ParameterRange(0.0f, 2.0f);

    // Vibrato
    ranges["vibratoRate"] = ParameterRange(0.5f, 12.0f);
    ranges["vibratoDepth"] = ParameterRange(0.0f, 2.0f);
    ranges["vibratoFadeIn"] = ParameterRange(0.0f, 10.0f);

    // Pitch Envelope
    ranges["pitchEnvDistance"] = ParameterRange(-12.0f, 12.0f);
    ranges["pitchEnvTime"] = ParameterRange(0.01f, 5.0f);

    // Effects
    ranges["detuneAmount"] = ParameterRange(0.0f, 1.0f);
    ranges["detuneMix"] = ParameterRange(0.0f, 1.0f);
    ranges["warmth"] = ParameterRange(0.0f, 1.0f);
    ranges["punch"] = ParameterRange(0.0f, 1.0f);
    ranges["punchFrequency"] = ParameterRange(500.0f, 5000.0f);

    // Drawbar parameters (k1-k10, with all their sub-parameters)
    for (int i = 1; i <= 10; ++i) {
        juce::String prefix = "k" + juce::String(i);
        ranges[prefix] = ParameterRange(-2.0f, 2.0f);
        ranges[prefix + "AttackTime"] = ParameterRange(0.001f, 10.0f);
        ranges[prefix + "DecayTime"] = ParameterRange(0.001f, 10.0f);
        ranges[prefix + "SustainLevel"] = ParameterRange(0.0f, 2.0f);
        ranges[prefix + "ReleaseTime"] = ParameterRange(0.001f, 10.0f);
        ranges[prefix + "EnvelopeAmount"] = ParameterRange(-5.0f, 20.0f);
        ranges[prefix + "LFOShape"] = ParameterRange(1.0f, 4.0f);
        ranges[prefix + "LFORate"] = ParameterRange(0.05f, 1000.0f);
        ranges[prefix + "LFOAmount"] = ParameterRange(-5.0f, 5.0f);
        ranges["input_f" + juce::String(i)] = ParameterRange(0.5f, 30.0f);
        ranges[prefix + "VelToHarmonic"] = ParameterRange(-100.0f, 100.0f);
        ranges[prefix + "LFOSync"] = ParameterRange(0.0f, 1.0f);
        ranges[prefix + "LFOSyncDiv"] = ParameterRange(0.0f, 12.0f);
        ranges[prefix + "ToPM"] = ParameterRange(0.0f, 1.0f);
        ranges[prefix + "ToOut"] = ParameterRange(0.0f, 1.0f);
        ranges[prefix + "TrigSingle"] = ParameterRange(0.0f, 1.0f);
    }

    return ranges;
}

juce::String PLANETPatchManager::formatParameterValue(float value, const juce::String& parameterID) {
    // Integer parameters
    if (parameterID.contains("LFOShape") || parameterID.contains("LFOSync") || parameterID.contains("LFOSyncDiv")
        || parameterID.contains("ToPM") || parameterID.contains("ToOut") || parameterID.contains("TrigSingle")
        || parameterID.contains("ModWheel")
        || parameterID == "punchFrequency") {
        return juce::String((int)value);
    }

    // Two decimal places for most parameters
    return juce::String(value, 2);
}
