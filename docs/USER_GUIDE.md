# GAIAXXL — user guide (v0.1.0)

GAIAXXL is a three-tone virtual-analog synth for the Akai Force and MPC (MPC OS 3.x). Its parameters are the GAIA SH-01
editor's, in the same units (0..127, -63..+63, -24..+24 ...), so values read like the original's.

## Getting a sound

- Insert GAIAXXL on a track. It opens on **Supersaw Lead**.
- Change patch with the **◀ ▶** arrows on the patch bar, or MPC's **PRESET** menu (64 factory patches).
  An asterisk after the name means the patch has been edited. The project saves the whole sound.
- **RANDOM** (bottom left, every page) makes a new patch of the type chosen on the ARP/COMMON page.

## The left column (every page)

| Control | What it does |
|---|---|
| TONE ON | (tone pages) the tone sounds or not |
| MONO | one voice, legato: overlapping notes glide without restarting the envelopes |
| PORTA / PORTA TIME | glide between notes (0..127 = 0..2 s); in poly mode each new note glides from the last one |
| OCTAVE ▼ ▲ | the patch's octave, -3..+3 |
| ARPEGGIO | the arpeggiator on/off |
| EFFECTS | the whole effects chain on/off |
| LEVEL | the patch's level |
| RANDOM | a new patch (see Randomizer) |

## TONE 1, TONE 2, TONE 3

Each tone has the same four sections.

**LFO** (blue): SHAPE (TRI, SIN, SAW, SQR, S&H, RND), RATE (0.05..30 Hz), SYNC (to MPC's tempo; then the NOTE knob
below picks the length, 16 bars down to 1/32), KEY (restart the LFO on every note), and the faders FADE (the LFO fades
in after the note, up to 6 s), PITCH, FILTER and AMP depths (-63..+63; at 63: ±1 octave, ±4 octaves of cutoff, ±50 %
level).

**OSC**: WAVE and VARIATION (the table in the README), PITCH (±24 semitones), DETUNE (±50 cents), and the faders
PWM (the LFO moving PW), PW (pulse width; on S-SAW the detune spread; on the KORG-bank variations the morph through
the bank; on SINE C the FM index), and the pitch envelope ENV A, ENV D and DEPTH (±63 = ±3 octaves).

**FILTER**: MODE (LPF, HPF, BPF, PKG = a peak at the cutoff whose height is RESONANCE, BYPASS), -12 / -24 dB,
CUTOFF (16 Hz..19.5 kHz), RESONANCE (self-oscillates near the top), KEY FOLLOW (-100..+100 %: at 100 the cutoff
follows the keyboard exactly), and the filter envelope A, D, S, R and DEPTH (±63 = ±8 octaves).

**AMP**: LEVEL and the amp envelope A, D, S, R.

Envelope times: 0 is 0.5 ms, 32 about 50 ms, 64 about 0.8 s, 96 about 4 s, 127 about 12 s.

**TONE 1 > 2** (on the TONE 2 page and on ARP/COMMON): **SYNC** restarts tone 2's oscillator with tone 1's (tune tone 2
up and sweep its pitch for the classic sync sound); **RING** multiplies them. Tone 1 still runs when it is off, so it
can drive tone 2 silently.

## MOD LFO

One row per tone: the second LFO, which sounds as far as the **modulation lever** (MIDI CC 1) is pushed: SHAPE,
TEMPO SYNC, RATE, NOTE, and its PITCH, FILTER, AMP and PAN depths. On the right of each row: the LFO's PAN depth, the
tone's PAN, and the velocity sensitivities of the filter envelope (F VELO) and the level (A VELO): positive = softer
notes darker / quieter, negative = the other way round.

## EFFECTS

In this order:

- **DISTORTION**: DIST (smooth overdrive), FUZZ (hard, asymmetric, with a rectified octave), BIT CRASH (DRIVE lowers
  the bits, TONE lowers the sample rate); DRIVE, TONE, LEVEL. **LOW BOOST**: +9 dB below 110 Hz.
- **FLANGER**: FLANGER, PHASER (six stages) or PITCH SHIFT; RATE, DEPTH, FEEDBACK, MANUAL/PITCH (the flanger's delay,
  or the shift in semitones, ±24), LEVEL (the wet mix).
- **DELAY**: DELAY or PAN DELAY (echoes cross left and right); TEMPO SYNC with NOTE, or TIME (up to 1.3 s); FEEDBACK,
  HIGH DAMP (darker repeats), LEVEL.
- **REVERB**: ROOM, HALL or PLATE; TIME (0.25..10 s), HIGH DAMP, LEVEL.

## ARP/COMMON

**ARPEGGIO**: KEY HOLD (the chord keeps playing after the keys are up; a new chord replaces it), GRID (4_, 8_, 16_
straight; L light and H heavy shuffle; t triplets), DURATION (the note length in % of a step; FUL ties), MOTIF:

| Motif | Plays |
|---|---|
| UP, DOWN, UP&DN, RND | the held notes in that order |
| …/L | with the lowest note on every other step (a pedal) |
| …/L&H | with the lowest and highest notes taking turns between the others |
| PHRASE | a fixed figure over the chord |

OCTAVE spreads the notes over up to three octaves up (or down), ACCENT makes the beats stronger than the off-beats,
VELOCITY fixes the velocity (0 = the keys' own). While MPC plays, the steps sit on its grid.

**RANDOMIZER**: TYPE (PAD, LEAD, BASS, PLUCK, KEYS, BRASS, STRINGS, FX), AMOUNT (100 = a new patch; 10..30 = a variation
of the one playing), RANDOMIZE. It keeps things musical: tones in unison, octaves or fifths (FX may go further), a little
detune for width, envelopes and filter shaped for the type, gentle LFOs, effects in tasteful amounts, and the patch level
evened out. It leaves the arpeggiator, the beams and the octave alone. RANDOM in the left column does the same.

**COMMON**: BEND UP and BEND DOWN (0..24 semitones), TONE 1 > 2.

**BEAMS**: two touch strips standing in for the D-Beam. ASSIGN picks the destination (any of the GAIA's 30: LFO, OSC,
filter and amp settings on all three tones, FX CTRL = distortion drive, PORTA TIME, BEND, MODULATION, FX LEVEL = delay
and reverb levels). Sliding up raises the destination from where the patch has it.

## Q-Links

Each page's knobs and faders are on Q-Link banks of 8 (MPC shows the bank):
TONE pages: 1 filter (CUTOFF, RESO, KEY FOLLOW, DEPTH, A, D, S, R), 2 OSC (PITCH, DETUNE, PW, PWM, pitch A, D, DEPTH,
LEVEL), 3 amp and LFO (A, D, S, R, RATE, PITCH, FILTER, AMP), 4 LFO FADE and NOTE.
MOD LFO: one bank per tone. EFFECTS: two banks. ARP/COMMON: one bank.

## MIDI

Notes and velocity, pitch bend, CC 1 (the modulation lever: the MOD LFO), CC 64 (sustain), CC 120 / 123 (all off).

## Factory patches

INIT, Supersaw Lead, Trance Pad, Acid Squelch, Sync Scream, PWM Strings, Poly Brass, Fat Saw Bass, Sub Pulse, Glass Bell,
Drawbar Organ, Electric Keys, Hollow Flute, Square Lead, Ring Bells, Noise Sweep, Wobble Bass, Analog Strings, Pluck Arp,
Fuzz Lead, Bit Crushed, Phaser Chords, Octave Shifter, Soft Sine Bass, Saw Pluck, Hoover Rave, Choir Pad, Laser Zap,
Mono Synth Lead, Shimmer Keys, Gated Pad, Deep Space; then 32 made by the randomizer and kept: four each of pads, leads,
basses, plucks, keys, brass, strings and FX.
