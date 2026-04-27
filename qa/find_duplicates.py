import re, math

text = open('Source/Engine/FactoryPresets.cpp', encoding='utf-8').read()

bank_names = ['Contrebasse','Fingered','Slap','Sub808','Boom808','Dist808','Moog','Reese','Acid']

bank_sections = {}
for i, name in enumerate(bank_names):
    marker = f'// [{i}]'
    idx = text.find(marker)
    if idx >= 0:
        bank_sections[i] = idx

sorted_banks = sorted(bank_sections.items(), key=lambda x: x[1])
bank_boundaries = []
for j, (bi, start) in enumerate(sorted_banks):
    end = sorted_banks[j+1][1] if j+1 < len(sorted_banks) else len(text)
    bank_boundaries.append((bi, start, end))

def euclid(v1, v2):
    return math.sqrt(sum((a-b)**2 for a,b in zip(v1,v2)))

print(f"{'Bank':12} | {'Preset A':28} | {'Preset B':28} | dist")
print('-'*90)

for bi, start, end in bank_boundaries:
    seg = text[start:end]
    matches = re.findall(r'\{\s*"([^"]+)"\s*,\s*\{\s*([\d.f, \-\+\\xC3\\xA9\\xA0]+)\s*\}', seg)
    # Simpler approach: find lines with float arrays
    presets = []
    # Match preset entries: a name line followed by a float array
    blocks = re.findall(r'\{\s*"([^"\\\\]+(?:\\\\[xX][0-9a-fA-F]{2}[^"\\\\]*)*)"\s*,\s*\{\s*((?:[^\{\}])+)\s*\}\s*\}', seg)
    for name, vals_raw in blocks:
        # Parse name: decode hex escapes
        name_clean = re.sub(r'\\x[0-9A-Fa-f]{2}', '?', name)
        vals = re.findall(r'[-+]?[0-9]*\.?[0-9]+f?', vals_raw)
        floats = []
        for v in vals:
            try:
                floats.append(float(v.rstrip('f')))
            except ValueError:
                pass
        if len(floats) >= 15:
            brightness = floats[2]
            cutoff_n   = floats[12] / 8000.0
            sustain    = floats[5]
            decay_n    = floats[4] / 10.0
            drive      = floats[8]
            vec = (brightness, cutoff_n, sustain, decay_n, drive)
            presets.append((name_clean, vec, floats))

    for i in range(len(presets)):
        for j in range(i+1, len(presets)):
            d = euclid(presets[i][1], presets[j][1])
            if d < 0.10:
                print(f"[{bi}]{bank_names[bi]:10} | {presets[i][0]:28} | {presets[j][0]:28} | {d:.3f}")
