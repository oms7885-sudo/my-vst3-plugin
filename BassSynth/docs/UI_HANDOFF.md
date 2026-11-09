# UI handoff (Bass Ladder Synth)

Two people/tools work on this plugin in the same repository:

| Area | Owner | Files |
|---|---|---|
| Sound engine, parameters, presets | Claude | `Source/SynthEngine.h`, `Source/Parameters.h` (ids, ranges, defaults, kinds), `Source/PluginProcessor.*`, `tests/` |
| User interface | ChatGPT | `Source/PluginEditor.*`, any new UI files, images/fonts |

Keep to your own files so changes never collide. If the UI needs something from the engine
(a new meter, a new value), ask for it instead of editing engine files.

## Two rules for the UI (so later engine updates need no UI work)

1. **Start every UI task from the latest `bass-synth`.** Before starting, update your `ui/...` branch
   from `bass-synth` (create a fresh branch from it, or merge `bass-synth` into yours). A UI branch
   cut from an old commit would silently drop engine updates when merged.
2. **Draw every parameter in the table, even ones the design has no place for.** Any row of
   `bsyn::spec::params` that the designed panel does not place by id must still appear
   automatically, in an "extra" area, using its `kind`, `label` and `tip`: a knob for `Float` and
   `Choice`, a switch for `Bool`. Then when the engine adds a parameter, it shows up in the next build
   without any UI change; giving it a proper place in the design can wait. Likewise, a placed id
   that no longer exists in the table must simply be skipped, never crash.

The UI must never hard-code ranges, defaults or option lists: read them from the table or the host
parameter, so engine-side changes to them need no UI change either.

## Branches and builds

- Engine work lands on `bass-synth`.
- UI work goes on a branch named `ui/<something>` (for example `ui/panel-v1`) and is merged into
  `bass-synth` with a pull request.
- GitHub Actions builds every push to `bass-synth` and to `ui/**`, and every pull request into
  `bass-synth`. The Windows VST3 is the artifact `BassLadderSynth-VST3-Windows` on the run page.

## The parameter table: `Source/Parameters.h`

Every parameter is one row in `bsyn::spec::params`:

- `id`: the host/preset id. **Never rename or reuse.** Use it with
  `AudioProcessorValueTreeState::SliderAttachment` / `ButtonAttachment` (`proc.apvts`).
- `kind`: `Float` (knob), `Choice` (stepped knob or selector: options are in `choices`, separated by `|`),
  `Bool` (switch).
- `min`, `max`, `def`, `centre`, `unit`: engine values. The host parameter already has the right
  range and display text, so a slider attached by id shows correct values without extra code.
- `section`, `label`: where it sits on the panel and its short name. The order of rows inside a
  section is the Model D panel order.
- `tip`: Korean hover help, `"TITLE\n설명"`. UTF-8; build with `/utf-8` (already set in CMake).
  Use `juce::String (juce::CharPointer_UTF8 (d.tip))`.

UI-only text (`label`, `section`, `tip`, row order inside a section) may be edited in `Parameters.h`
by the UI side. Ranges, defaults, kinds and ids may not.

Sections, left to right (Model D order), then the plugin-only strip:

| Section | Controls |
|---|---|
| Controllers | Mod Amount (the mod wheel), Modulation Mix (osc 3 ↔ noise), Filter Mod switch |
| Oscillators | Osc 1 Range / Waveform, Osc 3 Control switch, Osc 2 Range / Frequency / Waveform, Osc 3 Range / Frequency / Waveform |
| Mixer | Osc 1/2/3, External, Noise volumes, each with an On switch; Noise White/Pink switch |
| Filter | Cutoff, Emphasis, Amount of Contour, Keyboard Control 1 and 2 switches, Attack, Decay, Sustain |
| Loudness | Attack, Decay, Sustain, Decay switch (Model D release), Dynamics (plugin only) |
| Setup (plugin only) | Comp switch, Comp Thresh, Comp Ratio, Gate, Pick Sens, Tail Follow, Ext Tone, Vintage, Input, Output, Dry DI |

Range choices: Osc 1/2 `32' 16' 8' 4' 2'`; Osc 3 `LO 32' 16' 8' 4' 2'` (index 0 = LO).
Waveforms: `Triangle, Tri-Saw, Saw, Square, Wide Pulse, Narrow Pulse` (Osc 3 has `Rev Saw` in place of Tri-Saw).

## Live values for meters and lamps (`BassSynthProcessor`, read from a UI timer)

| Member | Meaning |
|---|---|
| `displayFreq` (float) | Hz of the note being played, 0 = none |
| `displayGate` (bool) | a note is sounding |
| `displayTrig` (uint32) | counts note starts; when it changes, a new pick happened (flash a lamp) |
| `displayInput` (float) | input peak after Input Gain, 1.0 = full scale; Model D style OVERLOAD lamp at > 0.9 |
| `displayFilterEnv` (float) | filter contour 0..1 |
| `displayAmpEnv` (float) | loudness contour 0..1 |

All are `std::atomic`, written once per audio block. Read them with `.load()`; never call into the
engine from the UI.

## Presets

`proc.getNumPrograms()`, `proc.getProgramName (i)`, `proc.setCurrentProgram (i)`,
`proc.getCurrentProgram()`.

## Things the UI should keep

- Double-click on a knob returns it to the default (`setDoubleClickReturnValue`).
- Fine adjustment while dragging (the current panel: hold Ctrl / Cmd). Needed to detune
  Osc 2/3 Frequency by a few cents.
- Korean hover help after about 1 second (`juce::TooltipWindow` with a 1000 ms delay).
- New source files must be added to `target_sources` in `BassSynth/CMakeLists.txt`; images and
  fonts go through `juce_add_binary_data`.

## The current editor

`Source/PluginEditor.*` is a working baseline that builds every control straight from the table,
so a new parameter always appears even before the designed UI has a place for it. A designed UI
may replace it entirely, as long as every id in the table remains reachable.
