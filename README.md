# Glizzarine Harmonizer

Live-Harmonizer: Du singst ins Mikro und spielst auf dem Keyboard Akkorde. Jede gedrückte Taste wird zu einer eigenen Stimme aus deiner Stimme, genau auf der Tonhöhe der Taste (wie bei Jacob Collier).

## Bauen (GitHub Actions)

1. Neues GitHub-Repository anlegen, z. B. `GlizzarineHarmonizer`.
2. Alle Dateien hochladen. `build.yml` muss in `.github/workflows/build.yml` liegen.
3. Unter **Actions** läuft der Build automatisch. Am Ende bei **Artifacts** `GlizzarineHarmonizer-Windows` herunterladen.
4. Darin sind zwei Sachen:
   - `VST3/Glizzarine Harmonizer.vst3`: nach `C:\Program Files\Common Files\VST3\` kopieren
   - `Standalone/Glizzarine Harmonizer.exe`: läuft ohne DAW

## Schnellster Test: Standalone

1. `Glizzarine Harmonizer.exe` starten, oben auf **Options → Audio/MIDI Settings**.
2. Audio-Gerät: dein Interface (am besten **ASIO**), Eingang = Mikro-Kanal, Ausgang = Kopfhörer.
3. Unter **Active MIDI inputs** die KeyLab anhaken.
4. Singen und Akkorde spielen.

## In Cakewalk Sonar

1. **Treiber:** Edit → Preferences → Audio → Playback and Recording → Driver Mode **ASIO**. Notfalls WASAPI Exclusive, nicht Shared (zu viel Verzögerung). Puffer klein (64–128 Samples).
2. **Gesangsspur:** Audiospur anlegen, Eingang = Mikro, **Input Echo** (Monitoring) an.
3. In die FX-Box dieser Spur **Glizzarine Harmonizer** einfügen.
4. **Keyboard-Spur:** MIDI-Spur anlegen, Eingang = KeyLab.
5. Den **Ausgang** der MIDI-Spur auf **Glizzarine Harmonizer** stellen. MIDI-fähige Effekte erscheinen dort in der Ausgangs-Liste.
6. Auch bei der MIDI-Spur **Input Echo** an, dann singen und spielen.

## Presets

Oben rechts, mit < und > durchschalten (auch im Preset-Menü von Sonar). Der Klang kommt immer aus deiner Stimme, ein Preset stellt alles andere ein. Deine Einrichtung (Input, Sensitivity, Gate, Lowest note, Tempo, Output, Solo-Timing) bleibt beim Wechseln erhalten.

Collier Choir · Close Harmony (dry) · Cathedral Shimmer Choir · Pyramid Swell · Pyramid Outside-In · Ping-Pong Voice Arp · Ghost Choir · Deep Octave Pad · Robot Voices · Bedroom Lo-Fi · Auto-Wah Funk

## Regler

**Links (immer sichtbar):** Eingangspegel, ob Gesang erkannt wird, erkannter Ton, gehaltene Tasten, Verzögerung, Tempo, Aftertouch-Anzeige und der **TAP**-Button (Tempo eintippen, stellt automatisch auf eigenes Tempo).

### Tab MAIN
**VOICE**
- **Input:** wie gut das Plugin dich hört (nicht, wie laut du zu hören bist)
- **Sensitivity:** ab welcher Lautstärke Gesang erkannt wird. Weiter links reagiert es auch auf leises, feines Singen.
- **Soft Gate:** Geräusche unter dieser Schwelle werden leiser, nicht stumm
- **Solo Voice:** Lautstärke deiner Originalstimme (0 = nur Harmonien)
- **Harmonies:** Lautstärke der Harmonien
- **Leveling:** links folgen die Harmonien deiner Lautstärke, rechts bleiben sie auch beim leisen Singen voll da
- **Formant:** Stimmcharakter dunkler / heller
- **Consonants:** wie viel S, T, Atem in die Harmonien geht
- **Follow Pitch:** überträgt dein Vibrato und kleine Tonhöhen-Bewegungen auf die Harmonien (0 = starr auf der Taste, klingt roboterhaft)

**TONE & PLAY**
- **Warmth:** mehr Körper unten
- **Octave Down:** zusätzliche Stimme eine Oktave unter deiner tiefsten Taste
- **Attack:** wie schnell Stimmen einsetzen (hoch = Swell)
- **Release:** Ausklingen nach Loslassen
- **Sustain:** Klingt mit deinem letzten Gesangsklang weiter aus, wenn du aufhörst zu singen und Taste oder Pedal noch hältst
- **Width:** Breite der Stimmen im Raum
- **Lowest note:** tiefster gesungener Ton (höher = weniger Verzögerung)
- **Solo Voice (Timing):** *Direct* = deine Stimme ohne Verzögerung (live singen), *Aligned* = exakt zeitgleich mit den Harmonien (für Aufnahmen). Hörst du dich direkt über den Kopfhörer am QuadCast, stell den Solo-Voice-Regler auf 0.

**PYRAMID / ARP**
- **Mode:** Off / Pyramid (Töne setzen nacheinander ein und bleiben liegen) / Arp (ein Ton pro Schritt)
- **Order:** Up, Down, Crawl (Pyramid: von außen nach innen, Arp: hoch und wieder runter), Random, As played
- **Rate:** Notenwert pro Schritt (1/4, 1/8, 1/16, Triolen)
- **Steps:** nach wie vielen Schritten der Arp von vorn beginnt (7 + 1/8 = 7/8-Takt)
- **Arp Gate:** wie lang jeder Arp-Ton klingt
- **Octaves:** über wie viele Oktaven der Arp läuft
- **Tempo:** Host (Sonar) oder Own (eigener BPM-Regler / TAP)

Stereo-Reihenfolge: erster Ton in der Mitte, dann hart links, hart rechts, dann weiter innen.

### Tab EFFECTS
Jeder Effekt hat einen eigenen An/Aus-Knopf. Chorus, Phaser, Auto-Wah, Tremolo und Lo-Fi wirken nur auf die Harmonien, deine Stimme bleibt klar. Delay und Reverb wirken auf alles.
- **MOTION:** Chorus, Phaser
- **FILTER & RHYTHM:** Auto-Wah (öffnet mit deiner Singlautstärke), Tremolo (im Tempo), Lo-Fi
- **SPACE:** Delay (Ping-Pong, im Tempo oder frei), Reverb, Shimmer (Oktave nach oben im Hall), Output

### Tab AFTERTOUCH
Wirkt, wenn du nach dem Anschlag fester auf die Tasten drückst. Jede Funktion mit An/Aus und Stärke:
- **SPACE:** Swell (lauter), Shimmer, Reverb
- **COLOUR:** Filter (öffnet), Vibrato, Detune, Formant
- **Response:** wie schnell der Aftertouch reagiert

### Tab VELOCITY
- **To Level:** Anschlag auf Lautstärke
- **Brightness:** sanft = wärmer, fest = offener
- **Soft Attack:** sanft = weiches Einblenden
- **Echo Send:** fest angeschlagene Töne bekommen mehr Delay

Sustain-Pedal (CC64) funktioniert wie beim Klavier, auch im Pyramid- und Arp-Modus.
