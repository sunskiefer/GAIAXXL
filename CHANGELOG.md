# Changelog

## 0.1.3 (2026-10-22)
- Built on the latest wrapper and includes the Q-Link and automation fix (mpc-vst-plugins #486-#489): Q-Links now
  move this plugin's parameters on the track's automation screen (MPC no longer rebuilds its parameter screen on every
  write, at most one refresh per 50 ms), and the build keeps every parameter's index fixed (`params.lock`), so saved
  projects, automation and Q-Link maps stay on the right parameters in later versions. Nothing else changed.
- Packaged with the fixed L'Cronx release tools (the ' in the plugin folder name is escaped in install.sh and
  uninstall.sh, and the catalog check no longer flags it). Installs over 0.1.2 in place: no need to uninstall first.

## 0.1.2 (2026-10-17)
PKG filter fixed: it is now a real peaking filter. RESONANCE sets how much it boosts the cutoff (+6 dB at 0 up to
+24 dB at 127) and how narrow the peak is; -24 dB adds a second stage for steeper skirts. In 0.1.0 and 0.1.1 the boost
was only a few dB and the peak very wide, so PKG barely changed the sound. Patches using PKG will sound stronger now.

## 0.1.1 (2026-10-16)
Packed under L'Cronx: the plugin folder is now `L'Cronx - VST - GAIAXXL` and MPC lists the maker as L'Cronx. If you
installed 0.1.0, run its `uninstall.sh` first, then install 0.1.1. The installer handles the ' in the folder name.

## 0.1.0 (2026-10-16)
First release. Three tones per voice, eight voices: OSC (7 waves x 3 variations, KORG logue-sdk tables and wave banks),
pitch envelope, filter (LPF/HPF/BPF/PKG/BYPASS, -12/-24 dB) with its envelope, amp with its envelope, LFO and MOD LFO;
TONE 1 > 2 SYNC and RING; mono, portamento; DISTORTION, LOW BOOST, FLANGER/PHASER/PITCH SHIFT, DELAY/PAN DELAY,
REVERB; the arpeggiator; two beams; a musical randomizer; 64 factory patches; six pages. Offline-tested only.
