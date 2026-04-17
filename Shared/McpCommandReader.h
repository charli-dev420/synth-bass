#pragma once
// =============================================================================
// McpCommandReader.h — File-based command reader for MCP server integration.
//
// The MCP server writes JSON command files to %APPDATA%/MusiqueHub/mcp/commands/
// The Hub polls this directory, reads commands, executes them, and writes results
// to %APPDATA%/MusiqueHub/mcp/results/
// =============================================================================

#include <JuceHeader.h>

namespace mcp
{

// ── Command types ────────────────────────────────────────────────

enum class CommandType
{
    Unknown = 0,
    DescribeSynthInventory,
    SelectSynth,
    SelectInstrument,
    LoadPreset,
    SetParameter,
    LoadMidi,
    RenderSample,
    RenderTrack,
    RenderStems,
    ExportPreset,
    AgentRunEpisode
};

inline CommandType parseCommandType(const juce::String& type)
{
    if (type == "describe_synth_inventory") return CommandType::DescribeSynthInventory;
    if (type == "select_synth")       return CommandType::SelectSynth;
    if (type == "select_instrument")  return CommandType::SelectInstrument;
    if (type == "load_preset")        return CommandType::LoadPreset;
    if (type == "set_parameter")      return CommandType::SetParameter;
    if (type == "load_midi")          return CommandType::LoadMidi;
    if (type == "render_sample")      return CommandType::RenderSample;
    if (type == "render_track")       return CommandType::RenderTrack;
    if (type == "render_stems")       return CommandType::RenderStems;
    if (type == "export_preset")      return CommandType::ExportPreset;
    if (type == "agent_run_episode")  return CommandType::AgentRunEpisode;
    return CommandType::Unknown;
}

// ── Parsed command ───────────────────────────────────────────────

struct McpCommand
{
    juce::String id;
    juce::String timestamp;
    CommandType type = CommandType::Unknown;
    juce::var payload;           // full JSON payload
    juce::File sourceFile;       // the command JSON file

    // Convenience accessors
    int getInt(const juce::String& key, int fallback = 0) const
    {
        const juce::Identifier propertyId(key);
        if (payload.hasProperty(propertyId))
            return static_cast<int>(payload[propertyId]);
        return fallback;
    }

    float getFloat(const juce::String& key, float fallback = 0.0f) const
    {
        const juce::Identifier propertyId(key);
        if (payload.hasProperty(propertyId))
            return static_cast<float>(payload[propertyId]);
        return fallback;
    }

    juce::String getString(const juce::String& key, const juce::String& fallback = {}) const
    {
        const juce::Identifier propertyId(key);
        if (payload.hasProperty(propertyId))
            return payload[propertyId].toString();
        return fallback;
    }

    double getDouble(const juce::String& key, double fallback = 0.0) const
    {
        const juce::Identifier propertyId(key);
        if (payload.hasProperty(propertyId))
            return static_cast<double>(payload[propertyId]);
        return fallback;
    }

    bool getBool(const juce::String& key, bool fallback = false) const
    {
        const juce::Identifier propertyId(key);
        if (!payload.hasProperty(propertyId))
            return fallback;

        const auto value = payload[propertyId];
        if (value.isBool())
            return static_cast<bool>(value);

        const auto normalized = value.toString().trim().toLowerCase();
        if (normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on")
            return true;
        if (normalized == "0" || normalized == "false" || normalized == "no" || normalized == "off")
            return false;
        return fallback;
    }
};

// ── Command result ───────────────────────────────────────────────

struct McpResult
{
    juce::String commandId;
    bool success = false;
    juce::String message;
    juce::StringArray outputFiles;
};

// ── McpCommandReader ─────────────────────────────────────────────

class McpCommandReader
{
public:
    McpCommandReader()
    {
        auto appData = juce::File::getSpecialLocation(
            juce::File::userApplicationDataDirectory);
        baseDir     = appData.getChildFile("MusiqueHub").getChildFile("mcp");
        commandsDir = baseDir.getChildFile("commands");
        inflightDir = baseDir.getChildFile("inflight");
        resultsDir  = baseDir.getChildFile("results");

        commandsDir.createDirectory();
        inflightDir.createDirectory();
        resultsDir.createDirectory();
    }

    /// Poll for pending commands. Returns all unprocessed command files.
    juce::Array<McpCommand> pollCommands()
    {
        juce::Array<McpCommand> commands;

        if (! commandsDir.isDirectory())
            return commands;

        auto files = commandsDir.findChildFiles(
            juce::File::findFiles, false, "*.json");

        for (const auto& file : files)
        {
            auto parsed = parseCommandFile(file);
            if (parsed.type != CommandType::Unknown && claimCommandFile(parsed))
                commands.add(std::move(parsed));
        }

        return commands;
    }

    /// Write a result and delete the original command file.
    void writeResult(const McpResult& result, const juce::File& commandFile)
    {
        juce::DynamicObject::Ptr obj = new juce::DynamicObject();
        obj->setProperty("commandId", result.commandId);
        obj->setProperty("success", result.success);
        obj->setProperty("message", result.message);

        if (! result.outputFiles.isEmpty())
        {
            juce::Array<juce::var> files;
            for (const auto& f : result.outputFiles)
                files.add(f);
            obj->setProperty("outputFiles", files);
        }

        juce::var jsonVar(obj.get());
        auto jsonStr = juce::JSON::toString(jsonVar);

        auto resultFile = resultsDir.getChildFile(result.commandId + ".json");
        resultFile.replaceWithText(jsonStr);

        // Remove the processed command file
        commandFile.deleteFile();
    }

    juce::File getCommandsDir() const { return commandsDir; }
    juce::File getResultsDir()  const { return resultsDir; }

private:
    bool claimCommandFile(McpCommand& command)
    {
        if (command.sourceFile == juce::File{} || !command.sourceFile.existsAsFile())
            return false;

        auto inflightFile = inflightDir.getChildFile(command.sourceFile.getFileName());
        if (inflightFile.existsAsFile())
            inflightFile.deleteFile();

        if (!command.sourceFile.moveFileTo(inflightFile))
            return false;

        command.sourceFile = inflightFile;
        return true;
    }

    McpCommand parseCommandFile(const juce::File& file)
    {
        McpCommand cmd;
        cmd.sourceFile = file;

        auto jsonStr = file.loadFileAsString();
        auto parsed = juce::JSON::parse(jsonStr);

        if (! parsed.isObject())
            return cmd;

        cmd.id        = parsed[juce::Identifier("id")].toString();
        cmd.timestamp = parsed[juce::Identifier("timestamp")].toString();
        cmd.type      = parseCommandType(parsed[juce::Identifier("type")].toString());
        cmd.payload   = parsed[juce::Identifier("payload")];

        return cmd;
    }

    juce::File baseDir;
    juce::File commandsDir;
    juce::File inflightDir;
    juce::File resultsDir;
};

} // namespace mcp
