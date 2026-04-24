#include "FactoryPresets.h"

#include <algorithm>
#include <cctype>

namespace mbs
{
namespace
{
void scaleBankLevels(std::vector<InstrumentPreset>& bank, const float scale)
{
  for (auto& preset : bank)
    preset.settings.level = std::clamp(preset.settings.level * scale, 0.0f, 1.0f);
}

void addTag(std::vector<std::string>& tags, const std::string& tag)
{
    if (std::find(tags.begin(), tags.end(), tag) == tags.end())
        tags.push_back(tag);
}

std::string toLowerAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [] (unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

PresetMetadata buildPresetMetadata(const int bassIndex, const InstrumentPreset& preset)
{
    static constexpr std::array<const char*, kNumBasses> kBassTags = {{
        "double-bass", "fingered-bass", "slap-bass",
        "sub-808", "boom-808", "distorted-808",
        "moog-bass", "reese-bass", "acid-bass"
    }};

    PresetMetadata metadata;
    const auto family = getFamily(bassIndex);
    const auto lowerName = toLowerAscii(preset.name);

    switch (family)
    {
        case Family::Acoustic:
            metadata.familyLabel = "acoustic";
            metadata.mixRole = "organic-foundation";
            metadata.nominalPeakDb = -12.0f;
            break;
        case Family::Eight08:
            metadata.familyLabel = "808";
            metadata.mixRole = "sub-foundation";
            metadata.nominalPeakDb = -9.5f;
            break;
        case Family::Synth:
        default:
            metadata.familyLabel = "synth";
            metadata.mixRole = "character-bass";
            metadata.nominalPeakDb = -10.5f;
            break;
    }

        const auto hasAnyToken = [&lowerName] (std::initializer_list<const char*> tokens)
        {
          for (const auto* token : tokens)
          {
            if (lowerName.find(token) != std::string::npos)
              return true;
          }
          return false;
        };

        if (hasAnyToken({ "slap", "punch", "attack", "staccato" }))
        {
          metadata.mixRole = "transient-bass";
          metadata.nominalPeakDb = -8.0f;
        }
        else if (hasAnyToken({ "ambient", "pad", "drone", "atmosph" }))
        {
          metadata.mixRole = "texture-bass";
          metadata.nominalPeakDb = -13.5f;
        }
        else if (family == Family::Synth && hasAnyToken({ "lead" }))
        {
          metadata.mixRole = "lead-bass";
          metadata.nominalPeakDb = -9.0f;
        }

    addTag(metadata.tags, metadata.familyLabel);
    addTag(metadata.tags, kBassTags[static_cast<std::size_t>(bassIndex)]);
    addTag(metadata.tags, metadata.mixRole);

    if (lowerName.find("glide") != std::string::npos)
        addTag(metadata.tags, "glide");
    if (lowerName.find("distort") != std::string::npos || lowerName.find("drive") != std::string::npos
        || lowerName.find("crunch") != std::string::npos || lowerName.find("grit") != std::string::npos)
        addTag(metadata.tags, "driven");
    if (lowerName.find("warm") != std::string::npos || lowerName.find("round") != std::string::npos
        || lowerName.find("velours") != std::string::npos)
        addTag(metadata.tags, "warm");
    if (lowerName.find("dark") != std::string::npos || lowerName.find("sombre") != std::string::npos)
        addTag(metadata.tags, "dark");
    if (lowerName.find("bright") != std::string::npos || lowerName.find("brillante") != std::string::npos)
        addTag(metadata.tags, "bright");
    if (lowerName.find("acid") != std::string::npos)
        addTag(metadata.tags, "acid");
    if (lowerName.find("reese") != std::string::npos)
        addTag(metadata.tags, "reese");
    if (lowerName.find("moog") != std::string::npos)
        addTag(metadata.tags, "moog");

    return metadata;
}
}

const std::array<std::vector<InstrumentPreset>, kNumBasses>& getFactoryPresetBanks()
{
    // -------------------------------------------------------------------------
    // BassSettings field order:
    //   level, tuneSemitones, brightness, attackSeconds, decaySeconds,
    //   sustainLevel, releaseSeconds, body, drive, pitchEnv,
    //   subLevel, character, cutoffHz, pan, resonance, glideTime, filterEnv
    // -------------------------------------------------------------------------

    static const auto banks = []()
    {
        std::array<std::vector<InstrumentPreset>, kNumBasses> presetBanks = {{

        // =====================================================================
        // [0] Contrebasse — warm, woody, bowed/plucked double bass (25 presets)
        // =====================================================================
        {
            // --- Neutres / fondamentaux ---
            { "Pizzicato Jazz",
              { 0.34f, 0.0f, 0.45f, 0.005f, 2.5f, 0.30f, 0.35f, 0.70f,
                0.00f, 0.0f, 0.40f, 0.50f, 2800.0f, 0.0f, 0.20f, 0.0f } },
            { "Walking Bass",
              { 0.34f, 0.0f, 0.48f, 0.004f, 2.0f, 0.35f, 0.30f, 0.60f,
                0.00f, 0.0f, 0.45f, 0.55f, 3500.0f, 0.0f, 0.22f, 0.0f } },
            { "Arco Lyrique",
              { 0.34f, 0.0f, 0.38f, 0.020f, 4.0f, 0.55f, 0.60f, 0.75f,
                0.00f, 0.0f, 0.35f, 0.45f, 2200.0f, 0.0f, 0.18f, 0.0f } },
            { "Bossa Nova",
              { 0.34f, 0.0f, 0.52f, 0.004f, 1.8f, 0.28f, 0.25f, 0.55f,
                0.00f, 0.0f, 0.35f, 0.45f, 4000.0f, 0.0f, 0.22f, 0.0f } },
            { "Cin\xC3\xA9""matique",
              { 0.34f, 0.0f, 0.35f, 0.010f, 5.0f, 0.40f, 0.80f, 0.80f,
                0.05f, 0.0f, 0.45f, 0.60f, 2000.0f, 0.0f, 0.18f, 0.0f } },
            // --- Registres et styles ---
            { "Orchestrale Douce",
              { 0.34f, 0.0f, 0.32f, 0.025f, 5.5f, 0.50f, 0.70f, 0.82f,
                0.00f, 0.0f, 0.38f, 0.42f, 1800.0f, 0.0f, 0.15f, 0.0f } },
            { "Tango Profond",
              { 0.34f, 0.0f, 0.50f, 0.003f, 2.2f, 0.32f, 0.28f, 0.65f,
                0.02f, 0.0f, 0.48f, 0.58f, 3200.0f, 0.0f, 0.24f, 0.0f } },
            { "Bluegrass Percussif",
              { 0.34f, 0.0f, 0.58f, 0.002f, 1.5f, 0.22f, 0.20f, 0.55f,
                0.04f, 0.0f, 0.42f, 0.62f, 4200.0f, 0.0f, 0.28f, 0.0f } },
            { "Jazz Club Feutr\xC3\xA9",
              { 0.34f, 0.0f, 0.40f, 0.006f, 3.0f, 0.35f, 0.40f, 0.72f,
                0.00f, 0.0f, 0.42f, 0.48f, 2400.0f, 0.0f, 0.20f, 0.0f } },
            { "Solo Expressif",
              { 0.34f, 0.0f, 0.55f, 0.008f, 3.5f, 0.42f, 0.50f, 0.68f,
                0.03f, 0.0f, 0.40f, 0.55f, 3800.0f, 0.0f, 0.25f, 0.0f } },
            { "Contrebasse Sombre",
              { 0.34f, 0.0f, 0.28f, 0.012f, 4.5f, 0.48f, 0.55f, 0.85f,
                0.00f, 0.0f, 0.50f, 0.40f, 1600.0f, 0.0f, 0.15f, 0.0f } },
            { "Pizz Court",
              { 0.34f, 0.0f, 0.55f, 0.002f, 1.2f, 0.15f, 0.15f, 0.50f,
                0.02f, 0.0f, 0.38f, 0.52f, 4500.0f, 0.0f, 0.26f, 0.0f } },
            { "Arco Dramatique",
              { 0.34f, 0.0f, 0.42f, 0.030f, 6.0f, 0.60f, 0.90f, 0.78f,
                0.06f, 0.0f, 0.38f, 0.50f, 2600.0f, 0.0f, 0.20f, 0.0f } },
            // --- Travaill\xC3\xA9s ---
            { "Bois Ancien",
              { 0.34f, 0.0f, 0.30f, 0.015f, 5.0f, 0.52f, 0.65f, 0.90f,
                0.00f, 0.0f, 0.48f, 0.38f, 1500.0f, 0.0f, 0.12f, 0.0f } },
            { "Groove Latin",
              { 0.34f, 0.0f, 0.52f, 0.003f, 1.8f, 0.28f, 0.22f, 0.58f,
                0.03f, 0.0f, 0.44f, 0.55f, 3800.0f, 0.0f, 0.25f, 0.0f } },
            { "Swing Vintage",
              { 0.34f, 0.0f, 0.44f, 0.005f, 2.8f, 0.34f, 0.35f, 0.68f,
                0.01f, 0.0f, 0.40f, 0.50f, 3000.0f, 0.0f, 0.22f, 0.0f } },
            { "Folk Rustique",
              { 0.34f, 0.0f, 0.50f, 0.004f, 2.0f, 0.30f, 0.28f, 0.62f,
                0.05f, 0.0f, 0.42f, 0.58f, 3600.0f, 0.0f, 0.24f, 0.0f } },
            { "Chambre Intime",
              { 0.34f, 0.0f, 0.35f, 0.018f, 4.2f, 0.45f, 0.55f, 0.78f,
                0.00f, 0.0f, 0.36f, 0.44f, 2000.0f, 0.0f, 0.16f, 0.0f } },
            { "R\xC3\xA9gga\xC3\xA9 Roots",
              { 0.34f, 0.0f, 0.42f, 0.006f, 3.2f, 0.38f, 0.35f, 0.70f,
                0.02f, 0.0f, 0.50f, 0.48f, 2500.0f, 0.0f, 0.20f, 0.0f } },
            { "Film Noir",
              { 0.34f, 0.0f, 0.30f, 0.020f, 6.0f, 0.50f, 0.85f, 0.85f,
                0.04f, 0.0f, 0.45f, 0.55f, 1800.0f, 0.0f, 0.18f, 0.0f } },
            { "Contrebasse Brillante",
              { 0.34f, 0.0f, 0.62f, 0.003f, 2.0f, 0.30f, 0.28f, 0.55f,
                0.03f, 0.0f, 0.38f, 0.60f, 5000.0f, 0.0f, 0.28f, 0.0f } },
            { "Sustain Lyrique",
              { 0.34f, 0.0f, 0.40f, 0.022f, 7.0f, 0.58f, 0.75f, 0.75f,
                0.00f, 0.0f, 0.40f, 0.48f, 2200.0f, 0.0f, 0.18f, 0.0f } },
            { "Staccato Vif",
              { 0.34f, 0.0f, 0.56f, 0.001f, 0.8f, 0.10f, 0.12f, 0.48f,
                0.02f, 0.0f, 0.36f, 0.55f, 4800.0f, 0.0f, 0.28f, 0.0f } },
            { "Warm Pad Bass",
              { 0.34f, 0.0f, 0.32f, 0.035f, 8.0f, 0.55f, 1.00f, 0.88f,
                0.00f, 0.0f, 0.48f, 0.42f, 1400.0f, 0.0f, 0.14f, 0.0f } },
            { "Percussivement Bois",
              { 0.34f, 0.0f, 0.60f, 0.001f, 1.0f, 0.12f, 0.10f, 0.45f,
                0.06f, 0.0f, 0.40f, 0.65f, 5500.0f, 0.0f, 0.32f, 0.0f } },
        },

        // =====================================================================
        // [1] Basse Fingered — electric fingerstyle, round and warm (25 presets)
        // =====================================================================
        {
            // --- Neutres / fondamentaux ---
            { "Groove Motown",
              { 0.42f, 0.0f, 0.48f, 0.004f, 2.5f, 0.35f, 0.25f, 0.50f,
                0.05f, 0.0f, 0.55f, 0.45f, 3000.0f, 0.0f, 0.22f, 0.0f } },
            { "Rock Solide",
              { 0.42f, 0.0f, 0.58f, 0.002f, 1.8f, 0.30f, 0.20f, 0.40f,
                0.18f, 0.0f, 0.45f, 0.55f, 4200.0f, 0.0f, 0.28f, 0.0f } },
            { "Jazz Fusion",
              { 0.42f, 0.0f, 0.65f, 0.003f, 2.0f, 0.28f, 0.20f, 0.35f,
                0.08f, 0.0f, 0.55f, 0.65f, 5000.0f, 0.0f, 0.25f, 0.0f } },
            { "Pop Ronde",
              { 0.42f, 0.0f, 0.50f, 0.005f, 2.8f, 0.35f, 0.28f, 0.40f,
                0.00f, 0.0f, 0.50f, 0.45f, 3800.0f, 0.0f, 0.20f, 0.0f } },
            { "Funk Doigts",
              { 0.42f, 0.0f, 0.62f, 0.002f, 1.5f, 0.25f, 0.18f, 0.42f,
                0.12f, 0.0f, 0.48f, 0.65f, 5500.0f, 0.0f, 0.30f, 0.0f } },
            // --- Registres et styles ---
            { "R&B Velours",
              { 0.42f, 0.0f, 0.42f, 0.006f, 3.0f, 0.40f, 0.32f, 0.48f,
                0.03f, 0.0f, 0.58f, 0.42f, 2800.0f, 0.0f, 0.20f, 0.0f } },
            { "Indie Doux",
              { 0.42f, 0.0f, 0.45f, 0.005f, 2.5f, 0.38f, 0.30f, 0.44f,
                0.02f, 0.0f, 0.52f, 0.48f, 3200.0f, 0.0f, 0.22f, 0.0f } },
            { "Blues Profond",
              { 0.42f, 0.0f, 0.52f, 0.004f, 2.8f, 0.35f, 0.28f, 0.52f,
                0.08f, 0.0f, 0.55f, 0.50f, 3500.0f, 0.0f, 0.25f, 0.0f } },
            { "Gospel Punch",
              { 0.42f, 0.0f, 0.58f, 0.002f, 1.8f, 0.28f, 0.22f, 0.38f,
                0.14f, 0.0f, 0.50f, 0.60f, 4500.0f, 0.0f, 0.28f, 0.0f } },
            { "Country Road",
              { 0.42f, 0.0f, 0.55f, 0.003f, 2.0f, 0.32f, 0.25f, 0.42f,
                0.06f, 0.0f, 0.48f, 0.52f, 4000.0f, 0.0f, 0.24f, 0.0f } },
            { "Reggae Laid Back",
              { 0.42f, 0.0f, 0.40f, 0.008f, 3.5f, 0.42f, 0.35f, 0.55f,
                0.02f, 0.0f, 0.55f, 0.45f, 2500.0f, 0.0f, 0.18f, 0.0f } },
            { "Neo Soul",
              { 0.42f, 0.0f, 0.46f, 0.005f, 2.8f, 0.38f, 0.30f, 0.50f,
                0.04f, 0.0f, 0.56f, 0.48f, 3000.0f, 0.0f, 0.22f, 0.0f } },
            { "Alt Rock Grit",
              { 0.42f, 0.0f, 0.62f, 0.001f, 1.5f, 0.22f, 0.15f, 0.35f,
                0.22f, 0.0f, 0.42f, 0.62f, 5200.0f, 0.0f, 0.32f, 0.0f } },
            // --- Travaill\xC3\xA9s ---
            { "Fingered Round Warm",
              { 0.42f, 0.0f, 0.38f, 0.008f, 3.5f, 0.45f, 0.38f, 0.55f,
                0.00f, 0.0f, 0.58f, 0.40f, 2200.0f, 0.0f, 0.18f, 0.0f } },
            { "Disco Groove",
              { 0.42f, 0.0f, 0.60f, 0.002f, 1.5f, 0.25f, 0.18f, 0.38f,
                0.10f, 0.0f, 0.48f, 0.62f, 5000.0f, 0.0f, 0.30f, 0.0f } },
            { "Ballad Intime",
              { 0.42f, 0.0f, 0.40f, 0.010f, 4.0f, 0.48f, 0.45f, 0.52f,
                0.00f, 0.0f, 0.55f, 0.42f, 2500.0f, 0.0f, 0.18f, 0.0f } },
            { "Punk Drive",
              { 0.42f, 0.0f, 0.68f, 0.001f, 1.2f, 0.18f, 0.12f, 0.30f,
                0.28f, 0.0f, 0.40f, 0.68f, 6000.0f, 0.0f, 0.35f, 0.0f } },
            { "Latin Groove",
              { 0.42f, 0.0f, 0.55f, 0.003f, 2.0f, 0.30f, 0.22f, 0.42f,
                0.06f, 0.0f, 0.50f, 0.55f, 4200.0f, 0.0f, 0.25f, 0.0f } },
            { "Fingered Sub Heavy",
              { 0.42f, 0.0f, 0.35f, 0.006f, 3.2f, 0.40f, 0.35f, 0.50f,
                0.05f, 0.0f, 0.65f, 0.40f, 2000.0f, 0.0f, 0.18f, 0.0f } },
            { "Prog Technique",
              { 0.42f, 0.0f, 0.64f, 0.002f, 1.8f, 0.28f, 0.18f, 0.36f,
                0.10f, 0.0f, 0.45f, 0.62f, 5500.0f, 0.0f, 0.30f, 0.0f } },
            { "Metal Agressif",
              { 0.42f, 0.0f, 0.72f, 0.001f, 1.2f, 0.15f, 0.10f, 0.28f,
                0.32f, 0.0f, 0.38f, 0.72f, 6500.0f, 0.0f, 0.38f, 0.0f } },
            { "Mute Doux",
              { 0.42f, 0.0f, 0.32f, 0.003f, 1.0f, 0.12f, 0.10f, 0.60f,
                0.00f, 0.0f, 0.52f, 0.35f, 1800.0f, 0.0f, 0.15f, 0.0f } },
            { "Fingered Sustain Long",
              { 0.42f, 0.0f, 0.48f, 0.006f, 5.0f, 0.52f, 0.55f, 0.45f,
                0.02f, 0.0f, 0.55f, 0.48f, 3200.0f, 0.0f, 0.22f, 0.0f } },
            { "Comp\xC3\xA9tition Slick",
              { 0.42f, 0.0f, 0.66f, 0.002f, 1.5f, 0.25f, 0.15f, 0.35f,
                0.15f, 0.0f, 0.45f, 0.60f, 5800.0f, 0.0f, 0.32f, 0.0f } },
            { "Dub Profond",
              { 0.42f, 0.0f, 0.35f, 0.008f, 4.0f, 0.45f, 0.40f, 0.58f,
                0.04f, 0.0f, 0.62f, 0.42f, 2000.0f, 0.0f, 0.18f, 0.0f } },
        },

        // =====================================================================
        // [2] Basse Slap — percussive slap technique, bright attack (25 presets)
        // =====================================================================
        {
            // --- Neutres / fondamentaux ---
            { "Slap Classique",
              { 0.40f, 0.0f, 0.68f, 0.001f, 1.5f, 0.18f, 0.15f, 0.35f,
                0.12f, 0.0f, 0.48f, 0.62f, 5000.0f, 0.0f, 0.32f, 0.0f } },
            { "Funk Attack",
              { 0.40f, 0.0f, 0.75f, 0.001f, 1.2f, 0.12f, 0.12f, 0.30f,
                0.22f, 0.0f, 0.42f, 0.72f, 6500.0f, 0.0f, 0.38f, 0.0f } },
            { "Slap Groovy",
              { 0.40f, 0.0f, 0.65f, 0.002f, 1.8f, 0.20f, 0.18f, 0.38f,
                0.10f, 0.0f, 0.52f, 0.58f, 4500.0f, 0.0f, 0.30f, 0.0f } },
            { "Double Thumb",
              { 0.40f, 0.0f, 0.60f, 0.001f, 1.6f, 0.22f, 0.18f, 0.40f,
                0.08f, 0.0f, 0.55f, 0.55f, 4200.0f, 0.0f, 0.35f, 0.0f } },
            { "Slap Profond",
              { 0.40f, 0.0f, 0.58f, 0.002f, 2.2f, 0.25f, 0.22f, 0.45f,
                0.15f, 0.0f, 0.60f, 0.55f, 3500.0f, 0.0f, 0.28f, 0.0f } },
            // --- Registres et styles ---
            { "Pop Slap",
              { 0.40f, 0.0f, 0.62f, 0.001f, 1.8f, 0.22f, 0.18f, 0.36f,
                0.10f, 0.0f, 0.50f, 0.58f, 4800.0f, 0.0f, 0.30f, 0.0f } },
            { "Slap Marcus",
              { 0.40f, 0.0f, 0.78f, 0.001f, 1.0f, 0.10f, 0.10f, 0.28f,
                0.25f, 0.0f, 0.40f, 0.75f, 7000.0f, 0.0f, 0.42f, 0.0f } },
            { "Thumb Heavy",
              { 0.40f, 0.0f, 0.55f, 0.001f, 2.0f, 0.28f, 0.22f, 0.48f,
                0.18f, 0.0f, 0.58f, 0.52f, 3800.0f, 0.0f, 0.28f, 0.0f } },
            { "Slap Bright Pop",
              { 0.40f, 0.0f, 0.72f, 0.001f, 1.4f, 0.15f, 0.12f, 0.32f,
                0.14f, 0.0f, 0.44f, 0.65f, 6000.0f, 0.0f, 0.35f, 0.0f } },
            { "Slap R&B",
              { 0.40f, 0.0f, 0.60f, 0.002f, 2.0f, 0.25f, 0.20f, 0.42f,
                0.08f, 0.0f, 0.55f, 0.55f, 4500.0f, 0.0f, 0.28f, 0.0f } },
            { "Wooten Fury",
              { 0.40f, 0.0f, 0.82f, 0.001f, 0.8f, 0.08f, 0.08f, 0.25f,
                0.30f, 0.0f, 0.38f, 0.80f, 8000.0f, 0.0f, 0.45f, 0.0f } },
            { "Slap Lo-Fi",
              { 0.40f, 0.0f, 0.50f, 0.002f, 2.5f, 0.30f, 0.25f, 0.50f,
                0.20f, 0.0f, 0.55f, 0.50f, 3000.0f, 0.0f, 0.25f, 0.0f } },
            { "Slap Ghost Notes",
              { 0.40f, 0.0f, 0.64f, 0.001f, 0.8f, 0.08f, 0.06f, 0.32f,
                0.10f, 0.0f, 0.42f, 0.60f, 5500.0f, 0.0f, 0.32f, 0.0f } },
            // --- Travaill\xC3\xA9s ---
            { "Festival Punchy",
              { 0.40f, 0.0f, 0.76f, 0.001f, 1.0f, 0.10f, 0.08f, 0.28f,
                0.28f, 0.0f, 0.40f, 0.75f, 7500.0f, 0.0f, 0.40f, 0.0f } },
            { "Slap Vintage 70s",
              { 0.40f, 0.0f, 0.58f, 0.002f, 2.0f, 0.25f, 0.22f, 0.45f,
                0.12f, 0.0f, 0.52f, 0.55f, 4000.0f, 0.0f, 0.28f, 0.0f } },
            { "Slap Comp\xC3\xA9tition",
              { 0.40f, 0.0f, 0.74f, 0.001f, 1.2f, 0.12f, 0.10f, 0.30f,
                0.20f, 0.0f, 0.42f, 0.70f, 6500.0f, 0.0f, 0.38f, 0.0f } },
            { "Slap Jazz",
              { 0.40f, 0.0f, 0.62f, 0.002f, 2.0f, 0.22f, 0.20f, 0.40f,
                0.05f, 0.0f, 0.50f, 0.58f, 5000.0f, 0.0f, 0.30f, 0.0f } },
            { "Slap \xC3\x89""lectrique",
              { 0.40f, 0.0f, 0.70f, 0.001f, 1.5f, 0.18f, 0.15f, 0.35f,
                0.16f, 0.0f, 0.45f, 0.65f, 5500.0f, 0.0f, 0.35f, 0.0f } },
            { "Slap Sub Lourd",
              { 0.40f, 0.0f, 0.52f, 0.002f, 2.5f, 0.30f, 0.28f, 0.50f,
                0.18f, 0.0f, 0.65f, 0.50f, 3200.0f, 0.0f, 0.25f, 0.0f } },
            { "Slap Staccato",
              { 0.40f, 0.0f, 0.70f, 0.001f, 0.6f, 0.05f, 0.05f, 0.25f,
                0.15f, 0.0f, 0.40f, 0.68f, 7000.0f, 0.0f, 0.38f, 0.0f } },
            { "Slap Chorus",
              { 0.40f, 0.0f, 0.66f, 0.002f, 1.8f, 0.20f, 0.18f, 0.38f,
                0.12f, 0.0f, 0.48f, 0.60f, 5200.0f, 0.0f, 0.32f, 0.0f } },
            { "Slap Drive Sale",
              { 0.40f, 0.0f, 0.72f, 0.001f, 1.2f, 0.12f, 0.10f, 0.30f,
                0.35f, 0.0f, 0.42f, 0.72f, 6000.0f, 0.0f, 0.38f, 0.0f } },
            { "Slap Sustain Chaud",
              { 0.40f, 0.0f, 0.56f, 0.003f, 3.0f, 0.35f, 0.30f, 0.48f,
                0.08f, 0.0f, 0.55f, 0.52f, 3800.0f, 0.0f, 0.26f, 0.0f } },
            { "Slap Harmonique",
              { 0.40f, 0.0f, 0.80f, 0.001f, 2.0f, 0.15f, 0.18f, 0.28f,
                0.05f, 0.0f, 0.35f, 0.72f, 8000.0f, 0.0f, 0.42f, 0.0f } },
        },

        // =====================================================================
        // [3] Sub 808 — pure sub sine, pitch drop, very clean (25 presets)
        // FIX Phase 3.1: Calibration — reduced levels for sub-foundation presets
        // Sub-bass presets have long sustain, so peak should be lower (~-12dB)
        // Transient presets (Trap Court, 808 Punch) can be higher (~-8dB)
        // =====================================================================
        {
            // --- Neutres / fondamentaux ---
            { "Sub Pur",
              { 0.75f, 0.0f, 0.18f, 0.002f, 5.0f, 0.45f, 0.35f, 0.0f,
                0.00f, 0.70f, 0.59f, 0.45f, 700.0f, 0.0f, 0.15f, 0.0f } },
            { "Trap Minimal",
              { 0.77f, 0.0f, 0.22f, 0.002f, 4.0f, 0.40f, 0.28f, 0.0f,
                0.00f, 0.78f, 0.57f, 0.50f, 650.0f, 0.0f, 0.15f, 0.0f } },
            { "Sub Long",
              { 0.74f, 0.0f, 0.18f, 0.003f, 8.0f, 0.55f, 0.45f, 0.0f,
                0.00f, 0.60f, 0.62f, 0.40f, 600.0f, 0.0f, 0.12f, 0.0f } },
            { "Drill Drop",
              { 0.77f, 0.0f, 0.20f, 0.001f, 3.0f, 0.30f, 0.20f, 0.0f,
                0.00f, 0.78f, 0.56f, 0.55f, 750.0f, 0.0f, 0.18f, 0.0f } },
            { "Club Sub",
              { 0.75f, 0.0f, 0.22f, 0.002f, 4.5f, 0.42f, 0.32f, 0.0f,
                0.00f, 0.78f, 0.57f, 0.50f, 800.0f, 0.0f, 0.15f, 0.0f } },
            // --- Registres et styles ---
            { "808 Lourd",
              { 0.76f, 0.0f, 0.16f, 0.002f, 6.0f, 0.50f, 0.40f, 0.0f,
                0.00f, 0.80f, 0.78f, 0.48f, 650.0f, 0.0f, 0.14f, 0.0f } },
            { "Trap Court",
              { 0.79f, 0.0f, 0.24f, 0.001f, 2.5f, 0.25f, 0.18f, 0.0f,
                0.00f, 0.95f, 0.65f, 0.52f, 720.0f, 0.0f, 0.16f, 0.0f } },
            { "Sub Gliss\xC3\xA9",
              { 0.75f, 0.0f, 0.20f, 0.002f, 5.0f, 0.45f, 0.35f, 0.0f,
                0.00f, 0.70f, 0.72f, 0.45f, 680.0f, 0.0f, 0.14f, 0.08f } },
            { "UK Garage Sub",
              { 0.76f, 0.0f, 0.22f, 0.003f, 3.5f, 0.35f, 0.28f, 0.0f,
                0.00f, 0.82f, 0.68f, 0.50f, 750.0f, 0.0f, 0.16f, 0.0f } },
            { "Lo-Fi 808",
              { 0.75f, 0.0f, 0.20f, 0.004f, 4.5f, 0.42f, 0.35f, 0.0f,
                0.03f, 0.72f, 0.70f, 0.48f, 620.0f, 0.0f, 0.14f, 0.0f } },
            { "Phonk Sub",
              { 0.78f, 0.0f, 0.25f, 0.001f, 3.5f, 0.32f, 0.22f, 0.0f,
                0.05f, 0.92f, 0.65f, 0.55f, 780.0f, 0.0f, 0.18f, 0.0f } },
            { "Ambient Sub",
              { 0.74f, 0.0f, 0.15f, 0.008f, 9.0f, 0.58f, 0.60f, 0.0f,
                0.00f, 0.55f, 0.78f, 0.38f, 550.0f, 0.0f, 0.10f, 0.0f } },
            { "Sub Staccato",
              { 0.79f, 0.0f, 0.22f, 0.001f, 1.5f, 0.15f, 0.10f, 0.0f,
                0.00f, 0.85f, 0.62f, 0.50f, 700.0f, 0.0f, 0.15f, 0.0f } },
            // --- Travaill\xC3\xA9s ---
            { "Reggaeton Sub",
              { 0.76f, 0.0f, 0.20f, 0.002f, 3.8f, 0.38f, 0.28f, 0.0f,
                0.00f, 0.88f, 0.72f, 0.50f, 700.0f, 0.0f, 0.16f, 0.0f } },
            { "Sub Vibrant",
              { 0.75f, 0.0f, 0.18f, 0.003f, 5.5f, 0.48f, 0.38f, 0.0f,
                0.00f, 0.78f, 0.75f, 0.45f, 660.0f, 0.0f, 0.14f, 0.0f } },
            { "Ultra Low",
              { 0.74f, 0.0f, 0.12f, 0.003f, 7.0f, 0.52f, 0.42f, 0.0f,
                0.00f, 0.60f, 0.82f, 0.35f, 500.0f, 0.0f, 0.10f, 0.0f } },
            { "Bounce 808",
              { 0.78f, 0.0f, 0.24f, 0.001f, 2.8f, 0.28f, 0.18f, 0.0f,
                0.02f, 0.92f, 0.66f, 0.52f, 740.0f, 0.0f, 0.16f, 0.0f } },
            { "Jersey Club",
              { 0.76f, 0.0f, 0.22f, 0.002f, 3.0f, 0.32f, 0.22f, 0.0f,
                0.00f, 0.88f, 0.68f, 0.50f, 720.0f, 0.0f, 0.15f, 0.0f } },
            { "Sub Mono Glide",
              { 0.75f, 0.0f, 0.18f, 0.002f, 5.0f, 0.45f, 0.35f, 0.0f,
                0.00f, 0.75f, 0.72f, 0.45f, 680.0f, 0.0f, 0.14f, 0.10f } },
            { "Sub Distortion L\xC3\xA9g\xC3\xA8re",
              { 0.76f, 0.0f, 0.25f, 0.002f, 4.0f, 0.38f, 0.28f, 0.0f,
                0.08f, 0.82f, 0.68f, 0.52f, 800.0f, 0.0f, 0.18f, 0.0f } },
            { "Grime Sub",
              { 0.78f, 0.0f, 0.22f, 0.002f, 3.5f, 0.35f, 0.25f, 0.0f,
                0.04f, 0.90f, 0.70f, 0.52f, 720.0f, 0.0f, 0.16f, 0.0f } },
            { "Sub Pad Atmosph\xC3\xA8re",
              { 0.74f, 0.0f, 0.14f, 0.010f, 10.0f, 0.60f, 0.70f, 0.0f,
                0.00f, 0.50f, 0.80f, 0.35f, 500.0f, 0.0f, 0.10f, 0.0f } },
            { "808 Punch",
              { 0.79f, 0.0f, 0.26f, 0.001f, 2.0f, 0.20f, 0.15f, 0.0f,
                0.02f, 0.98f, 0.64f, 0.55f, 780.0f, 0.0f, 0.18f, 0.0f } },
            { "Sub R\xC3\xA9verb\xC3\xA9r\xC3\xA9",
              { 0.75f, 0.0f, 0.18f, 0.005f, 6.0f, 0.50f, 0.50f, 0.0f,
                0.00f, 0.68f, 0.75f, 0.42f, 620.0f, 0.0f, 0.12f, 0.0f } },
        },

        // =====================================================================
        // [4] Boom 808 — richer 808, longer pitch drop, built-in saturation (25 presets)
        // FIX Phase 3.1: Calibration — reduced levels for character-bass presets
        // Boom 808 presets have medium sustain, peak around -9dB
        // =====================================================================
        {
            // --- Neutres / fondamentaux ---
            { "Boom Standard",
              { 0.77f, 0.0f, 0.28f, 0.003f, 6.0f, 0.42f, 0.35f, 0.0f,
                0.11f, 0.62f, 0.62f, 0.55f, 1200.0f, 0.0f, 0.22f, 0.0f } },
            { "Trap Boom",
              { 0.79f, 0.0f, 0.35f, 0.002f, 5.0f, 0.40f, 0.28f, 0.0f,
                0.22f, 0.70f, 0.59f, 0.60f, 1100.0f, 0.0f, 0.25f, 0.0f } },
            { "Heavy Hip-Hop",
              { 0.79f, 0.0f, 0.25f, 0.004f, 7.0f, 0.50f, 0.50f, 0.0f,
                0.14f, 0.55f, 0.66f, 0.50f,  900.0f, 0.0f, 0.20f, 0.0f } },
            { "808 Satur\xC3\xA9",
              { 0.77f, 0.0f, 0.40f, 0.002f, 4.5f, 0.35f, 0.25f, 0.0f,
                0.32f, 0.75f, 0.56f, 0.65f, 1400.0f, 0.0f, 0.28f, 0.0f } },
            { "808 M\xC3\xA9""lodique",
              { 0.75f, 0.0f, 0.22f, 0.003f, 8.0f, 0.55f, 0.60f, 0.0f,
                0.07f, 0.50f, 0.66f, 0.45f,  800.0f, 0.0f, 0.18f, 0.0f } },
            // --- Registres et styles ---
            { "Boom Deep",
              { 0.77f, 0.0f, 0.22f, 0.004f, 7.5f, 0.52f, 0.45f, 0.0f,
                0.12f, 0.55f, 0.82f, 0.48f,  850.0f, 0.0f, 0.18f, 0.0f } },
            { "Boom Clap",
              { 0.78f, 0.0f, 0.32f, 0.001f, 3.5f, 0.28f, 0.20f, 0.0f,
                0.35f, 0.80f, 0.68f, 0.62f, 1300.0f, 0.0f, 0.28f, 0.0f } },
            { "Boom Smooth",
              { 0.76f, 0.0f, 0.25f, 0.005f, 6.5f, 0.48f, 0.42f, 0.0f,
                0.10f, 0.58f, 0.78f, 0.50f,  950.0f, 0.0f, 0.20f, 0.0f } },
            { "Boom Punch",
              { 0.79f, 0.0f, 0.35f, 0.001f, 4.0f, 0.32f, 0.22f, 0.0f,
                0.32f, 0.75f, 0.70f, 0.62f, 1200.0f, 0.0f, 0.26f, 0.0f } },
            { "Boom R&B",
              { 0.76f, 0.0f, 0.28f, 0.004f, 5.5f, 0.45f, 0.38f, 0.0f,
                0.15f, 0.60f, 0.76f, 0.52f, 1000.0f, 0.0f, 0.22f, 0.0f } },
            { "Boom Reggaeton",
              { 0.78f, 0.0f, 0.30f, 0.002f, 4.5f, 0.38f, 0.28f, 0.0f,
                0.25f, 0.72f, 0.72f, 0.58f, 1100.0f, 0.0f, 0.24f, 0.0f } },
            { "Boom Lo-Fi",
              { 0.75f, 0.0f, 0.25f, 0.005f, 5.0f, 0.42f, 0.35f, 0.0f,
                0.18f, 0.58f, 0.75f, 0.50f,  900.0f, 0.0f, 0.20f, 0.0f } },
            { "Boom Phonk",
              { 0.79f, 0.0f, 0.38f, 0.001f, 3.5f, 0.30f, 0.22f, 0.0f,
                0.40f, 0.85f, 0.65f, 0.65f, 1400.0f, 0.0f, 0.30f, 0.0f } },
            // --- Travaill\xC3\xA9s ---
            { "Boom Layered",
              { 0.77f, 0.0f, 0.30f, 0.003f, 6.0f, 0.45f, 0.38f, 0.0f,
                0.22f, 0.65f, 0.78f, 0.55f, 1100.0f, 0.0f, 0.24f, 0.0f } },
            { "Boom Dark",
              { 0.77f, 0.0f, 0.20f, 0.004f, 7.0f, 0.52f, 0.48f, 0.0f,
                0.18f, 0.52f, 0.82f, 0.45f,  800.0f, 0.0f, 0.18f, 0.0f } },
            { "Boom BassHead",
              { 0.79f, 0.0f, 0.42f, 0.001f, 5.0f, 0.38f, 0.30f, 0.0f,
                0.50f, 0.78f, 0.72f, 0.68f, 1500.0f, 0.0f, 0.32f, 0.0f } },
            { "Boom Sustain",
              { 0.76f, 0.0f, 0.24f, 0.004f, 9.0f, 0.58f, 0.55f, 0.0f,
                0.10f, 0.50f, 0.82f, 0.45f,  800.0f, 0.0f, 0.18f, 0.0f } },
            { "Boom ATL",
              { 0.78f, 0.0f, 0.32f, 0.002f, 5.0f, 0.40f, 0.30f, 0.0f,
                0.28f, 0.72f, 0.74f, 0.58f, 1150.0f, 0.0f, 0.25f, 0.0f } },
            { "Boom Glide",
              { 0.77f, 0.0f, 0.28f, 0.003f, 6.0f, 0.42f, 0.35f, 0.0f,
                0.15f, 0.62f, 0.75f, 0.55f, 1000.0f, 0.0f, 0.22f, 0.08f } },
            { "Boom Speaker Blow",
              { 0.79f, 0.0f, 0.45f, 0.001f, 4.0f, 0.30f, 0.22f, 0.0f,
                0.55f, 0.82f, 0.65f, 0.72f, 1600.0f, 0.0f, 0.35f, 0.0f } },
            { "Boom Festival",
              { 0.78f, 0.0f, 0.35f, 0.002f, 5.5f, 0.42f, 0.32f, 0.0f,
                0.30f, 0.70f, 0.75f, 0.60f, 1200.0f, 0.0f, 0.26f, 0.0f } },
            { "Boom Film Score",
              { 0.75f, 0.0f, 0.20f, 0.005f, 8.0f, 0.55f, 0.55f, 0.0f,
                0.08f, 0.48f, 0.82f, 0.42f,  750.0f, 0.0f, 0.16f, 0.0f } },
            { "Boom Vinyl Crunch",
              { 0.76f, 0.0f, 0.30f, 0.004f, 5.0f, 0.40f, 0.32f, 0.0f,
                0.22f, 0.60f, 0.74f, 0.55f, 1000.0f, 0.0f, 0.22f, 0.0f } },
            { "Boom Mono Funk",
              { 0.78f, 0.0f, 0.34f, 0.002f, 4.5f, 0.35f, 0.25f, 0.0f,
                0.28f, 0.68f, 0.70f, 0.58f, 1100.0f, 0.0f, 0.24f, 0.05f } },
        },

        // =====================================================================
        // [5] Distorted 808 — saturated, aggressive, wide pitch sweep (25 presets)
        // FIX Phase 3.1: Calibration — reduced levels for character-bass presets
        // Distorted presets already have drive/clipping, so reduced levels needed
        // to avoid clipping on export. Range: 0.76-0.86
        // =====================================================================
        {
            // --- Neutres / fondamentaux ---
            { "Rage 808",
              { 0.76f, 0.0f, 0.40f, 0.001f, 3.0f, 0.22f, 0.18f, 0.0f,
                0.66f, 0.78f, 0.45f, 0.72f, 1800.0f, 0.0f, 0.35f, 0.0f } },
            { "Trap Gritty",
              { 0.74f, 0.0f, 0.38f, 0.001f, 3.5f, 0.28f, 0.22f, 0.0f,
                0.45f, 0.78f, 0.47f, 0.65f, 1600.0f, 0.0f, 0.30f, 0.0f } },
            { "Grunge Sub",
              { 0.74f, 0.0f, 0.30f, 0.002f, 2.5f, 0.25f, 0.20f, 0.0f,
                0.53f, 0.78f, 0.51f, 0.60f, 1200.0f, 0.0f, 0.28f, 0.0f } },
            { "808 Clipp\xC3\xA9",
              { 0.76f, 0.0f, 0.45f, 0.001f, 2.8f, 0.20f, 0.18f, 0.0f,
                0.74f, 0.78f, 0.43f, 0.78f, 2000.0f, 0.0f, 0.38f, 0.0f } },
            { "Acide Satur\xC3\xA9",
              { 0.76f, 0.0f, 0.50f, 0.001f, 2.0f, 0.20f, 0.15f, 0.0f,
                0.57f, 0.78f, 0.39f, 0.75f, 2200.0f, 0.0f, 0.40f, 0.0f } },
            // --- Registres et styles ---
            { "Distort Lourd",
              { 0.76f, 0.0f, 0.42f, 0.001f, 3.5f, 0.25f, 0.20f, 0.0f,
                0.85f, 0.90f, 0.55f, 0.75f, 1900.0f, 0.0f, 0.36f, 0.0f } },
            { "Trash 808",
              { 0.74f, 0.0f, 0.48f, 0.001f, 2.5f, 0.18f, 0.15f, 0.0f,
                0.92f, 0.95f, 0.50f, 0.82f, 2200.0f, 0.0f, 0.42f, 0.0f } },
            { "808 Buzz Saw",
              { 0.74f, 0.0f, 0.52f, 0.001f, 2.0f, 0.15f, 0.12f, 0.0f,
                0.88f, 0.92f, 0.48f, 0.80f, 2400.0f, 0.0f, 0.40f, 0.0f } },
            { "Distort Warm",
              { 0.74f, 0.0f, 0.35f, 0.002f, 4.0f, 0.32f, 0.28f, 0.0f,
                0.55f, 0.80f, 0.62f, 0.60f, 1400.0f, 0.0f, 0.28f, 0.0f } },
            { "808 Crunch",
              { 0.75f, 0.0f, 0.44f, 0.001f, 3.0f, 0.22f, 0.18f, 0.0f,
                0.75f, 0.88f, 0.56f, 0.70f, 1800.0f, 0.0f, 0.35f, 0.0f } },
            { "Industrial 808",
              { 0.76f, 0.0f, 0.50f, 0.001f, 2.2f, 0.18f, 0.15f, 0.0f,
                0.95f, 0.95f, 0.48f, 0.85f, 2500.0f, 0.0f, 0.45f, 0.0f } },
            { "808 Scream",
              { 0.78f, 0.0f, 0.55f, 0.001f, 1.8f, 0.12f, 0.10f, 0.0f,
                0.98f, 0.98f, 0.45f, 0.90f, 2800.0f, 0.0f, 0.48f, 0.0f } },
            { "Distort Short",
              { 0.76f, 0.0f, 0.42f, 0.001f, 1.5f, 0.10f, 0.08f, 0.0f,
                0.82f, 0.90f, 0.52f, 0.72f, 2000.0f, 0.0f, 0.38f, 0.0f } },
            // --- Travaill\xC3\xA9s ---
            { "Phonk Rage",
              { 0.78f, 0.0f, 0.46f, 0.001f, 2.8f, 0.20f, 0.16f, 0.0f,
                0.88f, 0.94f, 0.52f, 0.78f, 2000.0f, 0.0f, 0.38f, 0.0f } },
            { "Distort M\xC3\xA9""lodie",
              { 0.75f, 0.0f, 0.35f, 0.002f, 5.0f, 0.40f, 0.35f, 0.0f,
                0.50f, 0.70f, 0.65f, 0.58f, 1200.0f, 0.0f, 0.26f, 0.0f } },
            { "808 Fuzz",
              { 0.74f, 0.0f, 0.48f, 0.001f, 2.5f, 0.18f, 0.15f, 0.0f,
                0.90f, 0.92f, 0.50f, 0.80f, 2200.0f, 0.0f, 0.42f, 0.0f } },
            { "Distort Glide",
              { 0.75f, 0.0f, 0.40f, 0.001f, 3.5f, 0.25f, 0.20f, 0.0f,
                0.72f, 0.88f, 0.58f, 0.68f, 1700.0f, 0.0f, 0.32f, 0.06f } },
            { "808 RMS Max",
              { 0.78f, 0.0f, 0.50f, 0.001f, 2.0f, 0.15f, 0.12f, 0.0f,
                0.95f, 0.95f, 0.48f, 0.85f, 2500.0f, 0.0f, 0.45f, 0.0f } },
            { "Distort Ambient",
              { 0.74f, 0.0f, 0.28f, 0.005f, 6.0f, 0.45f, 0.50f, 0.0f,
                0.42f, 0.65f, 0.68f, 0.55f, 1000.0f, 0.0f, 0.22f, 0.0f } },
            { "808 Broken Speaker",
              { 0.76f, 0.0f, 0.52f, 0.001f, 2.2f, 0.15f, 0.12f, 0.0f,
                0.96f, 0.94f, 0.48f, 0.88f, 2600.0f, 0.0f, 0.45f, 0.0f } },
            { "Distort Bounce",
              { 0.76f, 0.0f, 0.42f, 0.001f, 3.0f, 0.22f, 0.18f, 0.0f,
                0.78f, 0.90f, 0.55f, 0.72f, 1800.0f, 0.0f, 0.36f, 0.0f } },
            { "808 Tape Sat",
              { 0.74f, 0.0f, 0.35f, 0.002f, 4.0f, 0.35f, 0.30f, 0.0f,
                0.48f, 0.78f, 0.62f, 0.58f, 1300.0f, 0.0f, 0.26f, 0.0f } },
        },

        // =====================================================================
        // [6] Moog Bass — classic saw, warm saturation (25 presets)
        // FIX Phase 3.1: Calibration — reduced levels for synth character-bass
        // Saw/square waves are rich harmonics, saturation adds more energy
        // Reduced levels to avoid clipping on export. Range: 0.74-0.86
        // =====================================================================
        {
            // --- Neutres / fondamentaux ---
            { "Warm Classic",
              { 0.82f, 0.0f, 0.52f, 0.003f, 2.5f, 0.40f, 0.28f, 0.0f,
                0.18f, 0.0f, 0.38f, 0.50f, 1800.0f, 0.0f, 0.40f, 0.04f } },
            { "Moog Funk",
              { 0.84f, 0.0f, 0.65f, 0.002f, 1.5f, 0.30f, 0.20f, 0.0f,
                0.28f, 0.0f, 0.32f, 0.60f, 2800.0f, 0.0f, 0.45f, 0.03f } },
            { "Lead Bass",
              { 0.82f, 0.0f, 0.72f, 0.002f, 2.0f, 0.35f, 0.22f, 0.0f,
                0.25f, 0.0f, 0.30f, 0.65f, 3500.0f, 0.0f, 0.50f, 0.05f } },
            { "R\xC3\xA9""tro 70s",
              { 0.78f, 0.0f, 0.48f, 0.004f, 3.0f, 0.48f, 0.35f, 0.0f,
                0.15f, 0.0f, 0.42f, 0.45f, 1600.0f, 0.0f, 0.35f, 0.04f } },
            { "Moog Deep",
              { 0.80f, 0.0f, 0.40f, 0.005f, 3.5f, 0.50f, 0.35f, 0.0f,
                0.12f, 0.0f, 0.45f, 0.42f, 1200.0f, 0.0f, 0.38f, 0.05f } },
            // --- Registres et styles ---
            { "Moog Disco",
              { 0.84f, 0.0f, 0.68f, 0.002f, 1.5f, 0.28f, 0.18f, 0.0f,
                0.30f, 0.0f, 0.30f, 0.62f, 3200.0f, 0.0f, 0.48f, 0.03f } },
            { "Moog Sub Drone",
              { 0.78f, 0.0f, 0.32f, 0.008f, 5.0f, 0.55f, 0.50f, 0.0f,
                0.10f, 0.0f, 0.55f, 0.38f, 1000.0f, 0.0f, 0.30f, 0.0f } },
            { "Moog Grind",
              { 0.86f, 0.0f, 0.75f, 0.001f, 1.5f, 0.22f, 0.15f, 0.0f,
                0.38f, 0.0f, 0.28f, 0.72f, 4000.0f, 0.0f, 0.55f, 0.02f } },
            { "Moog Pad",
              { 0.76f, 0.0f, 0.38f, 0.015f, 5.0f, 0.55f, 0.60f, 0.0f,
                0.08f, 0.0f, 0.48f, 0.40f, 1200.0f, 0.0f, 0.32f, 0.0f } },
            { "Moog Stab",
              { 0.84f, 0.0f, 0.62f, 0.001f, 0.8f, 0.10f, 0.08f, 0.0f,
                0.22f, 0.0f, 0.30f, 0.58f, 3000.0f, 0.0f, 0.42f, 0.0f } },
            { "Moog Wah",
              { 0.82f, 0.0f, 0.58f, 0.003f, 2.0f, 0.35f, 0.25f, 0.0f,
                0.20f, 0.0f, 0.35f, 0.55f, 2400.0f, 0.0f, 0.62f, 0.04f } },
            { "Moog Solo Scream",
              { 0.84f, 0.0f, 0.78f, 0.001f, 1.8f, 0.28f, 0.20f, 0.0f,
                0.35f, 0.0f, 0.25f, 0.72f, 4500.0f, 0.0f, 0.58f, 0.03f } },
            { "Moog 80s Pop",
              { 0.80f, 0.0f, 0.55f, 0.003f, 2.2f, 0.38f, 0.28f, 0.0f,
                0.18f, 0.0f, 0.38f, 0.52f, 2200.0f, 0.0f, 0.42f, 0.04f } },
            // --- Travaill\xC3\xA9s ---
            { "Moog Prog",
              { 0.82f, 0.0f, 0.62f, 0.002f, 2.5f, 0.35f, 0.25f, 0.0f,
                0.22f, 0.0f, 0.35f, 0.58f, 2800.0f, 0.0f, 0.48f, 0.05f } },
            { "Moog Dark Ambient",
              { 0.76f, 0.0f, 0.30f, 0.012f, 6.0f, 0.55f, 0.60f, 0.0f,
                0.06f, 0.0f, 0.50f, 0.35f,  900.0f, 0.0f, 0.28f, 0.0f } },
            { "Moog Octave Split",
              { 0.82f, -12.0f, 0.55f, 0.003f, 2.0f, 0.35f, 0.25f, 0.0f,
                0.20f, 0.0f, 0.40f, 0.55f, 2200.0f, 0.0f, 0.42f, 0.0f } },
            { "Moog Reggae",
              { 0.80f, 0.0f, 0.42f, 0.006f, 3.5f, 0.45f, 0.38f, 0.0f,
                0.12f, 0.0f, 0.50f, 0.45f, 1500.0f, 0.0f, 0.35f, 0.04f } },
            { "Moog Acid Trip",
              { 0.84f, 0.0f, 0.70f, 0.001f, 1.2f, 0.20f, 0.12f, 0.0f,
                0.32f, 0.0f, 0.28f, 0.68f, 3800.0f, 0.0f, 0.60f, 0.03f } },
            { "Moog Filter Sweep",
              { 0.80f, 0.0f, 0.50f, 0.004f, 3.0f, 0.42f, 0.32f, 0.0f,
                0.15f, 0.0f, 0.42f, 0.48f, 1800.0f, 0.0f, 0.55f, 0.04f } },
            { "Moog Percussif",
              { 0.86f, 0.0f, 0.60f, 0.001f, 1.0f, 0.08f, 0.06f, 0.0f,
                0.25f, 0.0f, 0.30f, 0.60f, 3500.0f, 0.0f, 0.45f, 0.0f } },
            { "Moog Glide Legato",
              { 0.82f, 0.0f, 0.52f, 0.003f, 2.5f, 0.40f, 0.28f, 0.0f,
                0.18f, 0.0f, 0.38f, 0.50f, 2000.0f, 0.0f, 0.42f, 0.12f } },
            { "Moog EQ Boost",
              { 0.82f, 0.0f, 0.68f, 0.002f, 2.0f, 0.32f, 0.22f, 0.0f,
                0.25f, 0.0f, 0.32f, 0.62f, 3200.0f, 0.0f, 0.50f, 0.03f } },
        },

        // =====================================================================
        // [7] Reese Bass — 3 detuned saws, dark and pulsating (25 presets)
        // FIX Phase 3.1: Calibration — reduced levels for synth character-bass
        // 3 detuned saws = 3x harmonic content, further reduced levels
        // Range: 0.74-0.86
        // =====================================================================
        {
            // --- Neutres / fondamentaux ---
            { "Reese DnB",
              { 0.78f, 0.0f, 0.42f, 0.005f, 4.0f, 0.38f, 0.50f, 0.0f,
                0.12f, 0.0f, 0.52f, 0.62f, 1200.0f, 0.0f, 0.28f, 0.0f } },
            { "Reese Sombre",
              { 0.80f, 0.0f, 0.32f, 0.006f, 5.0f, 0.48f, 0.60f, 0.0f,
                0.08f, 0.0f, 0.55f, 0.55f,  900.0f, 0.0f, 0.25f, 0.0f } },
            { "Neurofunk",
              { 0.82f, 0.0f, 0.48f, 0.004f, 3.5f, 0.35f, 0.45f, 0.0f,
                0.22f, 0.0f, 0.48f, 0.72f, 1600.0f, 0.0f, 0.35f, 0.0f } },
            { "Reese Large",
              { 0.78f, 0.0f, 0.38f, 0.006f, 4.5f, 0.45f, 0.55f, 0.0f,
                0.10f, 0.0f, 0.58f, 0.65f, 1000.0f, 0.0f, 0.28f, 0.0f } },
            { "Reese Ambient",
              { 0.76f, 0.0f, 0.32f, 0.010f, 6.0f, 0.52f, 0.70f, 0.0f,
                0.08f, 0.0f, 0.60f, 0.55f,  800.0f, 0.0f, 0.22f, 0.0f } },
            // --- Registres et styles ---
            { "Reese Jungle",
              { 0.80f, 0.0f, 0.45f, 0.004f, 3.5f, 0.35f, 0.42f, 0.0f,
                0.15f, 0.0f, 0.50f, 0.65f, 1300.0f, 0.0f, 0.30f, 0.0f } },
            { "Reese Dubstep",
              { 0.82f, 0.0f, 0.52f, 0.003f, 3.0f, 0.30f, 0.38f, 0.0f,
                0.25f, 0.0f, 0.45f, 0.70f, 1500.0f, 0.0f, 0.35f, 0.0f } },
            { "Reese Liquid",
              { 0.76f, 0.0f, 0.35f, 0.008f, 5.5f, 0.50f, 0.62f, 0.0f,
                0.06f, 0.0f, 0.58f, 0.55f,  850.0f, 0.0f, 0.22f, 0.0f } },
            { "Reese Minimal",
              { 0.78f, 0.0f, 0.38f, 0.006f, 4.0f, 0.42f, 0.50f, 0.0f,
                0.10f, 0.0f, 0.55f, 0.58f, 1000.0f, 0.0f, 0.25f, 0.0f } },
            { "Reese Menace",
              { 0.84f, 0.0f, 0.50f, 0.003f, 3.0f, 0.30f, 0.35f, 0.0f,
                0.28f, 0.0f, 0.45f, 0.72f, 1600.0f, 0.0f, 0.38f, 0.0f } },
            { "Reese Sub Deep",
              { 0.80f, 0.0f, 0.28f, 0.008f, 5.5f, 0.52f, 0.60f, 0.0f,
                0.05f, 0.0f, 0.62f, 0.50f,  750.0f, 0.0f, 0.20f, 0.0f } },
            { "Reese Techno",
              { 0.80f, 0.0f, 0.44f, 0.004f, 3.5f, 0.35f, 0.42f, 0.0f,
                0.15f, 0.0f, 0.48f, 0.62f, 1300.0f, 0.0f, 0.30f, 0.0f } },
            { "Reese Filthy",
              { 0.84f, 0.0f, 0.55f, 0.003f, 2.5f, 0.28f, 0.32f, 0.0f,
                0.32f, 0.0f, 0.42f, 0.75f, 1800.0f, 0.0f, 0.40f, 0.0f } },
            // --- Travaill\xC3\xA9s ---
            { "Reese Slow Evolve",
              { 0.76f, 0.0f, 0.30f, 0.012f, 7.0f, 0.55f, 0.70f, 0.0f,
                0.06f, 0.0f, 0.60f, 0.52f,  800.0f, 0.0f, 0.22f, 0.0f } },
            { "Reese Aggro",
              { 0.86f, 0.0f, 0.55f, 0.002f, 2.5f, 0.25f, 0.30f, 0.0f,
                0.30f, 0.0f, 0.42f, 0.75f, 1800.0f, 0.0f, 0.40f, 0.0f } },
            { "Reese Pad Lush",
              { 0.74f, 0.0f, 0.28f, 0.015f, 8.0f, 0.58f, 0.80f, 0.0f,
                0.04f, 0.0f, 0.62f, 0.48f,  700.0f, 0.0f, 0.18f, 0.0f } },
            { "Reese Roller",
              { 0.80f, 0.0f, 0.42f, 0.005f, 4.0f, 0.38f, 0.48f, 0.0f,
                0.14f, 0.0f, 0.52f, 0.62f, 1200.0f, 0.0f, 0.28f, 0.0f } },
            { "Reese Dark Matter",
              { 0.82f, 0.0f, 0.48f, 0.004f, 3.5f, 0.32f, 0.40f, 0.0f,
                0.20f, 0.0f, 0.48f, 0.68f, 1400.0f, 0.0f, 0.32f, 0.0f } },
            { "Reese Cinematic",
              { 0.76f, 0.0f, 0.32f, 0.010f, 6.5f, 0.52f, 0.65f, 0.0f,
                0.08f, 0.0f, 0.58f, 0.55f,  850.0f, 0.0f, 0.22f, 0.0f } },
            { "Reese Glide Smooth",
              { 0.78f, 0.0f, 0.40f, 0.006f, 4.5f, 0.42f, 0.52f, 0.0f,
                0.12f, 0.0f, 0.55f, 0.60f, 1100.0f, 0.0f, 0.28f, 0.08f } },
            { "Reese Laser",
              { 0.82f, 0.0f, 0.58f, 0.002f, 2.5f, 0.25f, 0.30f, 0.0f,
                0.25f, 0.0f, 0.40f, 0.72f, 2000.0f, 0.0f, 0.38f, 0.0f } },
            { "Reese Sub Growl",
              { 0.80f, 0.0f, 0.35f, 0.005f, 4.0f, 0.40f, 0.48f, 0.0f,
                0.18f, 0.0f, 0.58f, 0.62f, 1000.0f, 0.0f, 0.28f, 0.0f } },
        },

        // =====================================================================
        // [8] Acid Bass — square wave, filter-resonance focused (25 presets)
        // FIX Phase 3.1: Calibration — reduced levels for synth character-bass
        // Square waves have strong odd harmonics, high resonance adds more
        // Reduced levels to avoid clipping on export. Range: 0.74-0.86
        // =====================================================================
        {
            // --- Neutres / fondamentaux ---
            { "Acid 303",
              { 0.82f, 0.0f, 0.62f, 0.001f, 1.5f, 0.22f, 0.12f, 0.0f,
                0.18f, 0.0f, 0.28f, 0.60f, 2500.0f, 0.0f, 0.55f, 0.03f } },
            { "Acid Rapide",
              { 0.84f, 0.0f, 0.70f, 0.001f, 0.8f, 0.15f, 0.08f, 0.0f,
                0.22f, 0.0f, 0.25f, 0.70f, 3500.0f, 0.0f, 0.62f, 0.02f } },
            { "Acid Hardcore",
              { 0.86f, 0.0f, 0.78f, 0.001f, 1.2f, 0.15f, 0.10f, 0.0f,
                0.38f, 0.0f, 0.22f, 0.80f, 4500.0f, 0.0f, 0.68f, 0.02f } },
            { "Acid M\xC3\xA9""lodique",
              { 0.80f, 0.0f, 0.55f, 0.002f, 2.5f, 0.35f, 0.20f, 0.0f,
                0.12f, 0.0f, 0.35f, 0.52f, 2000.0f, 0.0f, 0.50f, 0.04f } },
            { "Techno Acid",
              { 0.84f, 0.0f, 0.65f, 0.001f, 1.0f, 0.18f, 0.10f, 0.0f,
                0.25f, 0.0f, 0.25f, 0.65f, 3200.0f, 0.0f, 0.58f, 0.02f } },
            // --- Registres et styles ---
            { "Acid Deep",
              { 0.80f, 0.0f, 0.50f, 0.002f, 2.0f, 0.30f, 0.18f, 0.0f,
                0.15f, 0.0f, 0.32f, 0.55f, 2200.0f, 0.0f, 0.52f, 0.03f } },
            { "Acid Squelch",
              { 0.86f, 0.0f, 0.72f, 0.001f, 1.0f, 0.15f, 0.08f, 0.0f,
                0.25f, 0.0f, 0.24f, 0.72f, 3800.0f, 0.0f, 0.70f, 0.02f } },
            { "Acid Groove",
              { 0.82f, 0.0f, 0.60f, 0.001f, 1.5f, 0.22f, 0.15f, 0.0f,
                0.20f, 0.0f, 0.28f, 0.62f, 2800.0f, 0.0f, 0.55f, 0.03f } },
            { "Acid House",
              { 0.82f, 0.0f, 0.58f, 0.002f, 1.8f, 0.25f, 0.15f, 0.0f,
                0.18f, 0.0f, 0.30f, 0.58f, 2600.0f, 0.0f, 0.52f, 0.03f } },
            { "Acid Dark",
              { 0.80f, 0.0f, 0.48f, 0.002f, 2.2f, 0.28f, 0.20f, 0.0f,
                0.15f, 0.0f, 0.35f, 0.55f, 1800.0f, 0.0f, 0.50f, 0.04f } },
            { "Acid Rave",
              { 0.86f, 0.0f, 0.75f, 0.001f, 0.8f, 0.12f, 0.06f, 0.0f,
                0.30f, 0.0f, 0.22f, 0.75f, 4200.0f, 0.0f, 0.65f, 0.02f } },
            { "Acid Warm",
              { 0.80f, 0.0f, 0.45f, 0.003f, 2.5f, 0.38f, 0.25f, 0.0f,
                0.10f, 0.0f, 0.35f, 0.48f, 1800.0f, 0.0f, 0.45f, 0.04f } },
            { "Acid Staccato",
              { 0.84f, 0.0f, 0.68f, 0.001f, 0.5f, 0.08f, 0.05f, 0.0f,
                0.22f, 0.0f, 0.25f, 0.65f, 3500.0f, 0.0f, 0.60f, 0.0f } },
            // --- Travaill\xC3\xA9s ---
            { "Acid Screech",
              { 0.86f, 0.0f, 0.82f, 0.001f, 1.0f, 0.10f, 0.06f, 0.0f,
                0.35f, 0.0f, 0.20f, 0.85f, 5000.0f, 0.0f, 0.72f, 0.02f } },
            { "Acid Glide Lent",
              { 0.80f, 0.0f, 0.55f, 0.002f, 2.5f, 0.35f, 0.22f, 0.0f,
                0.15f, 0.0f, 0.32f, 0.55f, 2200.0f, 0.0f, 0.52f, 0.10f } },
            { "Acid Minimal",
              { 0.78f, 0.0f, 0.50f, 0.002f, 2.0f, 0.30f, 0.18f, 0.0f,
                0.12f, 0.0f, 0.30f, 0.52f, 2000.0f, 0.0f, 0.48f, 0.03f } },
            { "Acid Drive Lourd",
              { 0.86f, 0.0f, 0.72f, 0.001f, 1.2f, 0.15f, 0.10f, 0.0f,
                0.42f, 0.0f, 0.22f, 0.78f, 4000.0f, 0.0f, 0.65f, 0.02f } },
            { "Acid R\xC3\xA9""tro",
              { 0.80f, 0.0f, 0.58f, 0.002f, 1.8f, 0.25f, 0.15f, 0.0f,
                0.18f, 0.0f, 0.28f, 0.60f, 2500.0f, 0.0f, 0.55f, 0.03f } },
            { "Acid EBM",
              { 0.84f, 0.0f, 0.65f, 0.001f, 1.5f, 0.20f, 0.12f, 0.0f,
                0.25f, 0.0f, 0.25f, 0.68f, 3200.0f, 0.0f, 0.58f, 0.02f } },
            { "Acid Filter Env",
              { 0.82f, 0.0f, 0.60f, 0.001f, 1.5f, 0.22f, 0.12f, 0.0f,
                0.20f, 0.0f, 0.28f, 0.62f, 2800.0f, 0.0f, 0.60f, 0.03f } },
            { "Acid Sustain",
              { 0.80f, 0.0f, 0.52f, 0.003f, 3.0f, 0.40f, 0.28f, 0.0f,
                0.12f, 0.0f, 0.32f, 0.52f, 2000.0f, 0.0f, 0.48f, 0.04f } },
            { "Acid Lo-Fi Tape",
              { 0.80f, 0.0f, 0.48f, 0.003f, 2.2f, 0.32f, 0.20f, 0.0f,
                0.18f, 0.0f, 0.32f, 0.55f, 1800.0f, 0.0f, 0.45f, 0.03f } },
        },

        }};

        for (int bassIndex = 0; bassIndex < kNumBasses; ++bassIndex)
        {
            auto& bank = presetBanks[static_cast<std::size_t>(bassIndex)];
            for (auto& preset : bank)
            {
                maskUnavailableFx(bassIndex, preset.fx);
                preset.outputBus = std::clamp(preset.outputBus, 0, 4);
                preset.metadata = buildPresetMetadata(bassIndex, preset);
            }
        }

        // Family-wide trims keep later banks inside the shared render QA ceiling.
        scaleBankLevels(presetBanks[3], 0.78f);
        scaleBankLevels(presetBanks[4], 0.58f);
        scaleBankLevels(presetBanks[5], 0.60f);
        scaleBankLevels(presetBanks[6], 0.35f);
        scaleBankLevels(presetBanks[7], 0.35f);
        scaleBankLevels(presetBanks[8], 0.35f);

        return presetBanks;
    }();

    return banks;
}

std::size_t getTotalFactoryPresetCount()
{
    std::size_t total = 0;
    for (const auto& bank : getFactoryPresetBanks())
        total += bank.size();
    return total;
}

} // namespace mbs
