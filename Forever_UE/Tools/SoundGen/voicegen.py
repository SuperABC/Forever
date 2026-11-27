"""Synthesize placeholder dialog voices (speech-like babble, not a real voice) as 16-bit mono WAV, numpy only.

Usage: python voicegen.py <out_dir> [--ogg]
  --ogg  also convert every WAV to OGG Vorbis with ffmpeg (must be on PATH), keeping both files.
Each line is a (name, syllables, base pitch Hz, seed) entry in LINES; the output is <name>.wav (and <name>.ogg).
The game plays these through the dialog "voice" field, see Source/Forever/Player/ForeverVoiceSubsystem.md.
"""
import subprocess
import sys
import wave
from pathlib import Path

import numpy as np

SR = 44100

# name, syllable count, base pitch (Hz), random seed
LINES = [
    ("voice_test_confirm", 6, 180.0, 1),
    ("voice_test_success", 3, 210.0, 2),
    ("voice_test_revenge", 9, 150.0, 3),
]


def syllable(rng, duration, pitch):
    """One voiced syllable: a glottal-like pulse train shaped by two formants, with a pitch glide and soft edges."""
    t = np.arange(int(duration * SR)) / SR
    glide = pitch * (1.0 + rng.uniform(-0.12, 0.12) * t / duration)
    phase = 2 * np.pi * np.cumsum(glide) / SR
    source = sum(np.sin(k * phase) / k for k in range(1, 12))  # band-limited sawtooth
    f1, f2 = rng.uniform(400, 900), rng.uniform(1100, 2300)
    spec = np.fft.rfft(source)
    freqs = np.fft.rfftfreq(len(source), 1.0 / SR)
    gain = np.exp(-((freqs - f1) / 160.0) ** 2) + 0.6 * np.exp(-((freqs - f2) / 220.0) ** 2)
    voiced = np.fft.irfft(spec * gain, len(source))
    env = np.sin(np.pi * np.clip(t / duration, 0, 1)) ** 0.7
    return voiced * env


def line(syllables, pitch, seed):
    rng = np.random.default_rng(seed)
    parts = []
    for _ in range(syllables):
        parts.append(syllable(rng, rng.uniform(0.14, 0.26), pitch * rng.uniform(0.9, 1.15)))
        parts.append(np.zeros(int(rng.uniform(0.03, 0.09) * SR)))
    signal = np.concatenate([np.zeros(int(0.05 * SR))] + parts)
    return 0.6 * signal / (np.max(np.abs(signal)) + 1e-9)


def write_wav(path, signal):
    data = (np.clip(signal, -1, 1) * 32767).astype("<i2").tobytes()
    with wave.open(str(path), "wb") as out:
        out.setnchannels(1)
        out.setsampwidth(2)
        out.setframerate(SR)
        out.writeframes(data)


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    out_dir = Path(sys.argv[1])
    out_dir.mkdir(parents=True, exist_ok=True)
    make_ogg = "--ogg" in sys.argv[2:]
    for name, syllables, pitch, seed in LINES:
        wav = out_dir / f"{name}.wav"
        write_wav(wav, line(syllables, pitch, seed))
        print("wrote", wav)
        if make_ogg:
            ogg = out_dir / f"{name}.ogg"
            subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", str(wav), "-c:a", "libvorbis", "-q:a", "4", str(ogg)], check=True)
            print("wrote", ogg)
    return 0


if __name__ == "__main__":
    sys.exit(main())
