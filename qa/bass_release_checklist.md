# Bass Release Checklist

## Scope
- Produit: `synth-bass`
- Objectif: valider la release commerciale d'un instrument orienté basses sans dérive de grave, de justesse ni de cohérence de banque.

## Build
- Compiler les tests:
```powershell
cmake --build synth-bass/build_standalone_local --config Release --target UWdeVST_bass_tests
```
- Compiler le renderer QA:
```powershell
cmake --build synth-bass/build_standalone_local --config Release --target UWdeVST_bass_renderer
```

## QA automatique
- Suite production complète:
```powershell
.\synth-bass\build_standalone_local\UWdeVST_bass_tests_artefacts\Release\UWdeVST_bass_tests.exe
```
- Validation preset report:
```powershell
.\synth-bass\build_standalone_local\UWdeVST_bass_renderer_artefacts\Release\UWdeVST_bass_renderer.exe --validate-presets --report qa/bass_preset_qa_report.csv
```
- Benchmark CPU:
```powershell
.\synth-bass\build_standalone_local\UWdeVST_bass_renderer_artefacts\Release\UWdeVST_bass_renderer.exe --benchmark-cpu --report qa/bass_cpu_benchmark.csv --baseline qa/bass_cpu_benchmark.csv --threshold 0.20
```

## Validation de banque
- Aucun doublon de nom dans une banque modèle.
- Chaque preset factory a:
  - un `mix_role` autorisé
  - un `family` correct
  - des tags contenant `bass`, `factory`, la famille, le modèle et le rôle
- Couverture minimale attendue:
  - acoustique: `organic-foundation` et `transient-bass`
  - 808: `sub-foundation` et `transient-bass`
  - synth: `character-bass`, `lead-bass`, `texture-bass`

## Écoute manuelle obligatoire
- Mono:
  - vérifier la stabilité de fondamentale sur sub 808, boom 808, moog deep, reese sub
- Kick simple:
  - tester au moins un preset par famille avec un kick court et un kick long
- Mini-mix:
  - 1 kick, 1 snare, 1 hat, 1 stab, 1 lead
  - vérifier lisibilité du centre tonal et place de la basse
- Notes tenues:
  - vérifier absence de wobble involontaire, faux pitch perçu, dérive de saturation
- Notes répétées:
  - vérifier que les presets `transient-bass` restent lisibles et ne s'écrasent pas
- Glide:
  - tester au moins un 808 glide, un moog glide, un acid glide
- Registre:
  - tester C1, C2, C3 minimum sur presets utilitaires
- Browser:
  - rechercher par rôle (`sub-foundation`, `texture-bass`, `lead-bass`) et vérifier la cohérence des résultats

## Blockers release
- Un test production rouge
- Un preset factory silencieux, NaN, clip ou incohérent de taxonomie
- Une basse 808 qui brouille le kick en mono sur preset utilitaire
- Un preset `texture-bass` ou `lead-bass` mal classé dans le browser
- Un écart CPU au-delà du seuil benchmark

## Go / No-Go
- `GO` seulement si QA auto verte + écoute manuelle validée + report preset/CPU régénérés.
- `NO-GO` si une correction grave/justesse/mono impose encore de retoucher le moteur ou la banque factory.
