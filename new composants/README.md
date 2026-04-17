# JUCE UI Components v5 + APVTS

Version v5 orientée **intégration réelle dans un plugin JUCE**.

## Contenu
### Thème / style
- `UIThemeV5.h`

### Composants UI individuels
- `KnobComponentV5.h/.cpp`
- `FaderComponentV5.h/.cpp`
- `VUMeterComponentV5.h/.cpp`
- `EnvelopeDisplayComponentV5.h/.cpp`
- `OutputMeterComponentV5.h/.cpp`
- `ToggleButtonComponentV5.h/.cpp`
- `SelectorComponentV5.h/.cpp`

### Helpers APVTS
- `ParameterIds.h`
- `PluginParameters.h/.cpp`
- `AttachmentHelpers.h`

### Démo intégrable
- `PluginEditorContentV5.h/.cpp`

## But
- garder le style v4
- ajouter une structure propre pour brancher les composants à `AudioProcessorValueTreeState`
- fournir une base réutilisable dans un vrai `AudioProcessorEditor`

## Notes
- `PluginParameters::createParameterLayout()` fournit un layout exemple
- `PluginEditorContentV5` montre comment créer les composants et les attacher
- les meters restent pilotés par setters, pas par APVTS
