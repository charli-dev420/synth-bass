#include "BassDefs.h"

#include <algorithm>
#include <cmath>

namespace mbs
{
namespace
{
// =========================================================================
// Names
// =========================================================================
constexpr std::array<const char*, kNumBasses> kNames = {
    // Acoustic
    "Contrebasse", "Basse Fingered", "Basse Slap",
    // 808
    "Sub 808", "Boom 808", "Distorted 808",
    // Synth
    "Moog Bass", "Reese Bass", "Acid Bass"
};

constexpr std::array<const char*, kNumBasses> kShortNames = {
    "CTRBS", "FINGR", "SLAP",
    "SUB",   "BOOM",  "DIST",
    "MOOG",  "REESE", "ACID"
};

constexpr std::array<const char*, kNumFamilies> kFamilyNames = {
    "ACOUSTIQUE", "808", "SYNTH\xC3\x89"
};

// =========================================================================
// Bass characteristics
// =========================================================================
constexpr std::array<BassCharacteristics, kNumBasses> kChars = {{
    // --- Acoustic ---
    // Contrebasse: warm, woody, plucked string with body resonance
    { OscMode::Additive, 8, 0.0003f, 0.000f, 1,
      0.0f, 0.0f,
      0.40f, 2.0f, 0.28f,
      1.00f, 0.75f, 0.35f,
      0.70f, 0.008f,
      0.0f, false },
    // Basse Fingered: electric bass fingerstyle, round, warm
    { OscMode::Additive, 6, 0.00004f, 0.000f, 1,
      0.0f, 0.0f,
      0.45f, 1.8f, 0.25f,
      1.00f, 0.50f, 0.25f,
      0.40f, 0.005f,
      0.0f, false },
    // Basse Slap: slap technique, bright attack, percussive
    { OscMode::Additive, 10, 0.00004f, 0.000f, 1,
      0.0f, 0.0f,
      0.55f, 1.2f, 0.18f,
      1.00f, 0.40f, 0.20f,
      1.00f, 0.003f,
      0.0f, false },

    // --- 808 ---
    // Sub 808: pure sub sine, pitch drop, very clean
    { OscMode::Sine, 0, 0.0f, 0.0f, 1,
      7.0f, 0.06f,
      0.25f, 5.0f, 0.35f,
      0.0f, 0.0f, 0.0f,
      0.0f, 0.0f,
      0.0f, true },
    // Boom 808: richer, bigger pitch drop, long sustain
    // FIX Phase 2.3: Added body delay (0.35) + body damping (0.12) for subtle resonance
    // This gives Boom 808 a "massif" feel instead of sounding "vide"
    { OscMode::Additive, 5, 0.0001f, 0.0f, 1,
      8.0f, 0.08f,
      0.20f, 6.0f, 0.40f,
      0.0f, 0.0f, 0.0f,
      0.35f, 0.12f,  // bodyDelayRatio=0.35 (5th harmonic), bodyDamping=0.12 (subtle)
      1.5f, true },
    // Distorted 808: saturated, aggressive, big pitch sweep
    // FIX: Reduced pitchEnvSemitones from 18.0 to 10.0 to prevent zipper/glitch
    // during sweep while keeping aggressive character. 10 st in 40ms = ~250 Hz/s.
    { OscMode::Sine, 0, 0.0f, 0.0f, 1,
      10.0f, 0.04f,
      0.35f, 3.0f, 0.25f,
      0.0f, 0.0f, 0.0f,
      0.0f, 0.0f,
      4.0f, true },

    // --- Synth ---
    // Moog Bass: classic saw, warm saturation
    { OscMode::Saw, 0, 0.0f, 0.000f, 1,
      0.0f, 0.0f,
      0.40f, 2.5f, 0.30f,
      0.0f, 0.0f, 0.0f,
      0.0f, 0.0f,
      3.0f, false },
    // Reese Bass: 3 detuned saws, dark and pulsating
    { OscMode::Saw, 0, 0.0f, 0.015f, 3,
      0.0f, 0.0f,
      0.30f, 4.0f, 0.35f,
      0.0f, 0.0f, 0.0f,
      0.0f, 0.0f,
      0.8f, false },
    // Acid Bass: square wave, filter resonance focused
    { OscMode::Square, 0, 0.0f, 0.000f, 1,
      0.0f, 0.0f,
      0.50f, 1.5f, 0.20f,
      0.0f, 0.0f, 0.0f,
      0.0f, 0.0f,
      1.0f, false },
}};

// =========================================================================
// Default settings per bass
// =========================================================================
constexpr std::array<BassSettings, kNumBasses> kDefaults = {{
    // Contrebasse                                                                            reso  glide
    { 0.82f, 0.0f, 0.45f, 0.005f, 3.00f, 0.22f, 0.35f, 0.65f, 0.0f, 0.0f, 0.40f, 0.50f, 3000.0f, 0.0f, 0.20f, 0.0f, 0.0f },
    // Basse Fingered
    { 0.84f, 0.0f, 0.50f, 0.004f, 2.50f, 0.25f, 0.25f, 0.45f, 0.0f, 0.0f, 0.50f, 0.45f, 3500.0f, 0.0f, 0.22f, 0.0f, 0.0f },
    // Basse Slap
    { 0.85f, 0.0f, 0.65f, 0.001f, 1.50f, 0.15f, 0.15f, 0.35f, 0.10f, 0.0f, 0.45f, 0.60f, 5000.0f, 0.0f, 0.25f, 0.0f, 0.0f },
    // Sub 808
    { 0.90f, 0.0f, 0.20f, 0.002f, 5.00f, 0.35f, 0.30f, 0.0f, 0.0f, 0.80f, 0.70f, 0.50f, 800.0f, 0.0f, 0.15f, 0.0f, 0.0f },
    // Boom 808
    { 0.88f, 0.0f, 0.30f, 0.003f, 6.00f, 0.40f, 0.35f, 0.0f, 0.15f, 0.65f, 0.75f, 0.55f, 1200.0f, 0.0f, 0.18f, 0.0f, 0.0f },
    // Distorted 808
    { 0.85f, 0.0f, 0.35f, 0.001f, 3.00f, 0.20f, 0.20f, 0.0f, 0.55f, 0.90f, 0.60f, 0.65f, 1500.0f, 0.0f, 0.30f, 0.0f, 0.0f },
    // Moog Bass
    { 0.82f, 0.0f, 0.55f, 0.003f, 2.50f, 0.30f, 0.25f, 0.0f, 0.20f, 0.0f, 0.35f, 0.50f, 2000.0f, 0.0f, 0.45f, 0.05f, 0.58f },
    // Reese Bass
    { 0.80f, 0.0f, 0.40f, 0.005f, 4.00f, 0.35f, 0.50f, 0.0f, 0.10f, 0.0f, 0.50f, 0.60f, 1200.0f, 0.0f, 0.25f, 0.0f, 0.52f },
    // Acid Bass
    { 0.84f, 0.0f, 0.60f, 0.001f, 1.50f, 0.20f, 0.12f, 0.0f, 0.15f, 0.0f, 0.30f, 0.55f, 2500.0f, 0.0f, 0.55f, 0.0f, 0.72f },
}};

// =========================================================================
// Bass descriptions (French, UTF-8)
// =========================================================================
constexpr std::array<const char*, kNumBasses> kDescriptions = {{
    // Contrebasse
    "La contrebasse est le plus grand et le plus grave des instruments \xC3\xA0 cordes "
    "de l'orchestre. Apparue au XVI\xC3\xA8me si\xC3\xA8""cle, elle mesure environ 1,80 m et se joue "
    "debout ou sur un tabouret \xC3\xA9lev\xC3\xA9. En jazz, la technique du pizzicato (cordes "
    "pinc\xC3\xA9""es) produit un son chaud et rond, avec une attaque douce suivie d'un "
    "sustain bois\xC3\xA9. La caisse de r\xC3\xA9sonance en \xC3\xA9rable et \xC3\xA9pic\xC3\xA9""a "
    "amplifie les fr\xC3\xA9quences graves et donne au son cette profondeur "
    "organique caract\xC3\xA9ristique. De Charles Mingus \xC3\xA0 Ron Carter, "
    "elle est le pilier harmonique du jazz et de la musique classique.",

    // Basse Fingered
    "La basse \xC3\xA9lectrique jou\xC3\xA9""e aux doigts (fingered) est la technique la plus "
    "r\xC3\xA9pandue du bassiste. Invent\xC3\xA9""e par Leo Fender en 1951 avec la Precision "
    "Bass, elle a transform\xC3\xA9 la musique populaire. Le jeu aux doigts produit "
    "un son rond, chaud et articul\xC3\xA9 : l'index et le majeur alternent sur les cordes "
    "pour un groove r\xC3\xA9gulier et expressif. James Jamerson (Motown), Jaco Pastorius "
    "(jazz fusion) et Paul McCartney ont d\xC3\xA9""fini le son de la basse fingered. "
    "C'est le son de base de pratiquement toute la pop, du rock et du R&B.",

    // Basse Slap
    "Le slap est une technique percussive de basse \xC3\xA9lectrique o\xC3\xB9 le pouce frappe "
    "la corde (slap) et l'index tire et rel\xC3\xA2""che une corde aigu\xC3\xAB (pop). "
    "Popularis\xC3\xA9""e par Larry Graham dans les ann\xC3\xA9""es 1960, cette technique "
    "produit une attaque explosive suivie d'un claquement m\xC3\xA9tallique. Le son est "
    "hyper-percussif, avec des transitoires tr\xC3\xA8s rapides et un spectre riche en "
    "harmoniques. Flea (Red Hot Chili Peppers), Marcus Miller et Victor Wooten "
    "ont pouss\xC3\xA9 cette technique \xC3\xA0 des sommets de virtuosit\xC3\xA9. "
    "Le slap est indissociable du funk et du jazz fusion.",

    // Sub 808
    "Le Sub 808 est le son de basse le plus embl\xC3\xA9matique du hip-hop et du trap. "
    "Inspir\xC3\xA9 du kick drum de la bo\xC3\xAEte \xC3\xA0 rythmes Roland TR-808 (1980), il "
    "utilise une onde sinuso\xC3\xAF""dale pure avec une enveloppe de hauteur : la note "
    "d\xC3\xA9marre aigu\xC3\xAB puis descend rapidement vers les sub-basses. Le r\xC3\xa9sultat "
    "est un boom profond et propre qui fait vibrer les subwoofers. Ce son d\xC3\xa9""finit "
    "le hip-hop moderne de 808 Mafia, Metro Boomin et Lex Luger. "
    "Sa puret\xC3\xa9 harmonique permet de remplir les graves sans encom\xc2\xad""brer le mix.",

    // Boom 808
    "Le Boom 808 est une variante enrichie du 808 classique. Contrairement au Sub 808 "
    "pur, il ajoute des harmoniques l\xC3\xa9g\xC3\xa8res et un pitch drop plus prononc\xC3\xa9 "
    "pour cr\xC3\xa9""er un impact plus massif. Le decay tr\xC3\xa8s long produit une queue "
    "de basse qui remplit l'espace sonore sur plusieurs mesures. Une saturation "
    "douce ajoute de la pr\xC3\xa9sence dans les m\xC3\xa9""diums bas. Le Boom 808 est "
    "omnipr\xC3\xa9sent dans le trap, le drill UK, le reggaeton et "
    "la lo-fi hip-hop, o\xC3\xB9 il donne cette sensation de puissance subsonique.",

    // Distorted 808
    "Le 808 Distorted pousse la saturation au maximum. Le signal sinuso\xC3\xAF""dal "
    "passe \xC3\xA0 travers un \xC3\xa9tage de drive agressif qui g\xC3\xa9n\xC3\xa8re des harmoniques "
    "riches, transformant le sub propre en un growl puissant. Le pitch sweep est "
    "plus large et plus rapide, cr\xC3\xa9""ant un effet de \"zap\" distinctif. Ce son "
    "est l'\xC3\xa2me du trap hard, du phonk et du drill. Des producteurs comme "
    "Southside, Pi'erre Bourne et BEAM utilisent cette variante pour "
    "des drops qui saturent les syst\xC3\xa8mes audio.",

    // Moog Bass
    "Le Moog Bass est le son de basse synth\xC3\xa9tique le plus l\xC3\xa9gendaire. "
    "Cr\xC3\xa9\xC3\xa9 par Robert Moog avec le Minimoog Model D en 1970, il utilise "
    "un oscillateur en dent de scie pass\xC3\xa9 \xC3\xa0 travers un filtre passe-bas "
    "r\xC3\xa9sonant ladder \xC3\xa0 4 p\xC3\xB4les. Le r\xC3\xa9sultat est un son gras, chaud et "
    "puissant avec une saturation naturelle due aux circuits analogiques. "
    "De Bernie Worrell (Parliament) \xC3\xa0 Trent Reznor (NIN), le Moog Bass "
    "a d\xC3\xa9""fini le son du funk, du prog rock et de la musique \xC3\xa9lectronique. "
    "Sa chaleur harmonique reste ini\xC3\xa9gal\xC3\xa9""e.",

    // Reese Bass
    "Le Reese Bass doit son nom \xC3\xa0 Kevin \"Reese\" Saunderson, pionnier de la "
    "techno de Detroit. Il consiste en plusieurs oscillateurs en dent de scie "
    "l\xC3\xa9g\xC3\xa8rement d\xC3\xa9saccord\xC3\xa9s entre eux, cr\xC3\xa9""ant un battement (phasing) "
    "qui produit un son massif et mouvant. Quand les fr\xC3\xa9quences se rejoignent "
    "et s'\xC3\xa9""cartent, le timbre \xC3\xa9volue constamment, cr\xC3\xa9""ant une texture vivante. "
    "Ce son est devenu la signature du drum & bass (Goldie, Noisia), du dubstep "
    "(Skrillex, Burial) et de l'electro. Sa capacit\xC3\xa9 \xC3\xa0 remplir les graves "
    "tout en restant expressif en fait un outil essentiel.",

    // Acid Bass
    "L'Acid Bass est le son embl\xC3\xa9matique du Roland TB-303 Bass Line (1981). "
    "Initialement con\xC3\xa7u comme accompagnateur de guitare basse, il a \xC3\xa9t\xC3\xa9 "
    "d\xC3\xa9tourn\xC3\xa9 par DJ Pierre et Phuture \xC3\xa0 Chicago pour cr\xC3\xa9""er l'acid house. "
    "Un oscillateur carr\xC3\xa9 (ou dent de scie) passe \xC3\xa0 travers un filtre "
    "passe-bas tr\xC3\xa8s r\xC3\xa9sonant avec des glissandos de fr\xC3\xa9quence caract\xC3\xa9ristiques. "
    "Le r\xC3\xa9sultat est un son liquide, grinc\xC3\xa7""ant et hypnotique qui d\xC3\xa9""finit "
    "un genre entier. De l'acid house au psytrance, en passant par l'electro, "
    "le son du 303 reste instantan\xC3\xa9ment reconnaissable et irremplacable.",
}};

} // namespace

Family getFamily(const int bassIndex)
{
    const auto idx = std::clamp(bassIndex, 0, kNumBasses - 1);
    for (int f = kNumFamilies - 1; f > 0; --f)
        if (idx >= kFamilyStart[f]) return static_cast<Family>(f);
    return Family::Acoustic;
}

int getFamilyStartIndex(const Family family)
{
    return kFamilyStart[std::clamp(static_cast<int>(family), 0, kNumFamilies - 1)];
}

const char* getFamilyName(const int familyIndex)
{
    return kFamilyNames[static_cast<std::size_t>(std::clamp(familyIndex, 0, kNumFamilies - 1))];
}

const char* getBassName(const int bassIndex)
{
    return kNames[static_cast<std::size_t>(std::clamp(bassIndex, 0, kNumBasses - 1))];
}

const char* getBassShortName(const int bassIndex)
{
    return kShortNames[static_cast<std::size_t>(std::clamp(bassIndex, 0, kNumBasses - 1))];
}

const BassCharacteristics& getCharacteristics(const int bassIndex)
{
    return kChars[static_cast<std::size_t>(std::clamp(bassIndex, 0, kNumBasses - 1))];
}

BassSettings getDefaultSettings(const int bassIndex)
{
    return kDefaults[static_cast<std::size_t>(std::clamp(bassIndex, 0, kNumBasses - 1))];
}

const char* getBassDescription(const int bassIndex)
{
    return kDescriptions[static_cast<std::size_t>(std::clamp(bassIndex, 0, kNumBasses - 1))];
}

// =========================================================================
// FX availability per bass
// =========================================================================
static constexpr FxAvailability kFxAvailability[kNumBasses] =
{
    //                        Sat    Trans  Comp   EQ     Chor   Delay  Rev    Lim
    /* 0  Contrebasse   */ { false, true,  true,  true,  false, true,  true,  true  },
    /* 1  Fingered      */ { false, true,  true,  true,  true,  true,  true,  true  },
    /* 2  Slap          */ { true,  true,  true,  true,  false, true,  true,  true  },
    /* 3  Sub808        */ { true,  false, true,  true,  false, false, false, true  },
    /* 4  Boom808       */ { true,  true,  true,  true,  false, false, false, true  },
    /* 5  Distorted808  */ { true,  false, true,  true,  false, false, false, true  },
    /* 6  Moog          */ { true,  true,  true,  true,  true,  true,  true,  true  },
    /* 7  Reese         */ { true,  false, true,  true,  true,  true,  true,  true  },
    /* 8  Acid          */ { true,  true,  true,  true,  true,  true,  true,  true  },
};

const FxAvailability& getFxAvailability(const int bassIndex)
{
    return kFxAvailability[static_cast<std::size_t>(std::clamp(bassIndex, 0, kNumBasses - 1))];
}

bool isFxAvailable(const int bassIndex, const GlobalFxSlot slot)
{
    const auto& a = getFxAvailability(bassIndex);
    switch (slot)
    {
        case GlobalFxSlot::Saturator:  return a.saturator;
        case GlobalFxSlot::Transient:  return a.transient;
        case GlobalFxSlot::Compressor: return a.compressor;
        case GlobalFxSlot::Eq:         return a.eq;
        case GlobalFxSlot::Chorus:     return a.chorus;
        case GlobalFxSlot::Delay:      return a.delay;
        case GlobalFxSlot::Reverb:     return a.reverb;
        case GlobalFxSlot::Limiter:    return a.limiter;
        default:                       return false;
    }
}

void maskUnavailableFx(const int bassIndex, GlobalFxSettings& fx)
{
    const auto& a = getFxAvailability(bassIndex);
    if (!a.saturator)  fx.saturatorOn  = false;
    if (!a.transient)  fx.transientOn  = false;
    if (!a.compressor) fx.compressorOn = false;
    if (!a.eq)         fx.eqOn         = false;
    if (!a.chorus)     fx.chorusOn     = false;
    if (!a.delay)      fx.delayOn      = false;
    if (!a.reverb)     fx.reverbOn     = false;
    if (!a.limiter)    fx.limiterOn    = false;
}

bool supportsBodyControl(const int bassIndex)
{
    return getCharacteristics(bassIndex).bodyDelayRatio > 0.01f;
}

bool supportsPitchEnvControl(const int bassIndex)
{
    return std::abs(getCharacteristics(bassIndex).pitchEnvSemitones) > 0.01f;
}

bool supportsFilterEnvControl(const int bassIndex)
{
    return getFamily(bassIndex) == Family::Synth;
}

} // namespace mbs
