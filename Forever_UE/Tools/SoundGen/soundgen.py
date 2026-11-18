"""Synthesize placeholder sound effects as 16-bit mono WAV files (numpy only).

Usage: python soundgen.py [out_dir]
Each sound is a function below; tweak the constants at the top of the function and rerun.
"""
import sys
import wave
from pathlib import Path

import numpy as np

SR = 44100
rng = np.random.default_rng(1234)


def t_axis(duration):
    return np.arange(int(duration * SR)) / SR


def noise(duration):
    return rng.standard_normal(int(duration * SR))


def decay(duration, tau, attack=0.0005):
    """Exponential decay envelope with a short linear attack so there is no click at the start."""
    t = t_axis(duration)
    env = np.exp(-t / tau)
    if attack > 0:
        env *= np.minimum(1.0, t / attack)
    return env


def fft_filter(x, low=None, high=None):
    """Brick-wall band filter in the frequency domain with a short cosine transition (cheap, zero phase)."""
    spec = np.fft.rfft(x)
    freqs = np.fft.rfftfreq(len(x), 1.0 / SR)
    gain = np.ones_like(freqs)
    if high is not None:   # low-pass: pass below `high`
        gain *= 1.0 / (1.0 + (freqs / high) ** 4)
    if low is not None:    # high-pass: pass above `low`
        gain *= 1.0 / (1.0 + (low / np.maximum(freqs, 1e-6)) ** 4)
    return np.fft.irfft(spec * gain, len(x))


def sweep(duration, f0, f1, tau):
    """Sine whose frequency glides exponentially f0 -> f1, with decay envelope."""
    t = t_axis(duration)
    freq = f1 + (f0 - f1) * np.exp(-t / (duration * 0.25))
    phase = 2 * np.pi * np.cumsum(freq) / SR
    return np.sin(phase) * decay(duration, tau)


def reverb(x, tail, mix, tau):
    """Convolve with an exponentially decaying noise impulse response (FFT convolution)."""
    ir = noise(tail) * decay(tail, tau, attack=0.002)
    ir = fft_filter(ir, high=6000)
    n = len(x) + len(ir)
    wet = np.fft.irfft(np.fft.rfft(x, n) * np.fft.rfft(ir, n), n)
    wet *= np.max(np.abs(x)) / (np.max(np.abs(wet)) + 1e-9)
    out = np.zeros(n)
    out[:len(x)] += x
    return out + wet * mix


def mix_at(dst, src, offset_s, gain=1.0):
    i = int(offset_s * SR)
    j = min(len(dst), i + len(src))
    dst[i:j] += src[:j - i] * gain


def gunshot(crack_tau, crack_gain, body_tau, body_low, thump_f0, thump_f1, thump_tau, thump_gain,
            tail, tail_mix, tail_tau, length, tonal=0.15):
    """Generic firearm: sharp high-frequency crack + filtered noise body + low thump + room tail."""
    out = np.zeros(int(length * SR))
    # A real shot is a broadband noise transient, not a pitched drum hit: the "boom" is low-passed
    # noise, the tonal sine is only a faint hint, and the sum is saturated so it sounds harsh and dense.
    crack = fft_filter(noise(0.3), low=1500) * decay(0.3, crack_tau * 0.4, 0.0002)
    body = fft_filter(noise(0.6), high=body_low * 1.5, low=250) * decay(0.6, body_tau * 0.7, 0.0003)
    boom = fft_filter(noise(0.6), high=thump_f0 * 2.5) * decay(0.6, thump_tau, 0.001)
    hint = sweep(0.5, thump_f0, thump_f1, thump_tau)
    mix_at(out, crack, 0, crack_gain * 1.5)
    mix_at(out, body, 0, 1.0)
    mix_at(out, boom, 0, thump_gain * 1.2)
    mix_at(out, hint, 0, thump_gain * tonal)
    out = np.tanh(out * 2.5 / (np.max(np.abs(out)) + 1e-9))
    return reverb(out, tail, tail_mix, tail_tau)


def pistol():
    return gunshot(0.012, 1.0, 0.045, 3500, 220, 70, 0.07, 0.8, 0.35, 0.18, 0.09, 0.4)


def rifle():
    return gunshot(0.015, 1.1, 0.07, 3000, 170, 55, 0.09, 1.0, 0.6, 0.25, 0.15, 0.6)


def shotgun():
    return gunshot(0.02, 1.0, 0.14, 2400, 130, 38, 0.16, 1.6, 0.8, 0.3, 0.2, 0.8)


def sniper():
    return gunshot(0.02, 1.3, 0.12, 2800, 110, 32, 0.22, 1.8, 1.8, 0.45, 0.45, 1.8)


def camera():
    """Two-stage shutter: sharp click, then a softer lower click about 70 ms later."""
    out = np.zeros(int(0.25 * SR))

    def click(brightness, gain):
        n = fft_filter(noise(0.03), low=1800, high=brightness) * decay(0.03, 0.004, 0.0002)
        body = np.sin(2 * np.pi * 380 * t_axis(0.03)) * decay(0.03, 0.006, 0.0002)
        return (n + 0.5 * body) * gain

    mix_at(out, click(7000, 1.0), 0.0)
    mix_at(out, click(4500, 0.6), 0.07)
    return out


def grenade():
    """Explosion: crack, deep boom, rumbling noise, scattered debris ticks, long tail."""
    length = 2.2
    out = np.zeros(int(length * SR))
    mix_at(out, fft_filter(noise(0.4), low=1500) * decay(0.4, 0.03), 0, 0.9)
    mix_at(out, fft_filter(noise(1.6), high=1100, low=40) * decay(1.6, 0.35, 0.003), 0, 1.1)
    mix_at(out, sweep(1.2, 95, 24, 0.4), 0, 2.2)
    for _ in range(40):   # debris falling back down
        at = 0.15 + rng.random() * 1.6
        tick = fft_filter(noise(0.02), low=1500, high=6000) * decay(0.02, 0.003, 0.0002)
        mix_at(out, tick, at, 0.25 * np.exp(-at / 0.9))
    return reverb(out, 1.5, 0.35, 0.5)


def smoke():
    """Pop of the canister followed by a long gas hiss."""
    length = 2.8
    out = np.zeros(int(length * SR))
    mix_at(out, sweep(0.15, 260, 80, 0.04), 0, 1.2)
    mix_at(out, fft_filter(noise(0.1), low=800, high=5000) * decay(0.1, 0.02), 0, 0.8)
    n = int(2.6 * SR)
    t = np.arange(n) / SR
    env = np.minimum(1.0, t / 0.12) * np.exp(-t / 1.1)
    hiss = fft_filter(noise(2.6), low=2200, high=9000)
    flutter = 1.0 + 0.25 * np.sin(2 * np.pi * 7 * t + rng.random() * 6.28)
    mix_at(out, hiss * env * flutter, 0.04, 0.55)
    return out


def resample(x, ratio):
    """Change pitch (and length) by `ratio` with linear interpolation; ratio > 1 is higher and shorter."""
    idx = np.arange(0, len(x) - 1, ratio)
    return np.interp(idx, np.arange(len(x)), x)


def rifle_shot():
    """One round without the long room tail: crack, body and thump only."""
    return gunshot(0.015, 1.1, 0.07, 3000, 170, 55, 0.09, 1.0, 0.15, 0.1, 0.06, 0.3)


def rifle_burst(shots=15, interval=0.1):
    """Automatic fire. Every round is synthesized from fresh noise with its own randomized crack
    length, body brightness, thump pitch, level, pitch shift and timing, so no two rounds match.
    One sustained tail layer sits under the whole burst instead of one tail per round."""
    out = np.zeros(int((shots * interval + 1.5) * SR))
    for i in range(shots):
        u = rng.uniform
        shot = gunshot(0.015 * u(0.85, 1.2), u(0.85, 1.15), 0.07 * u(0.8, 1.25), 3000 * u(0.8, 1.25),
                       170 * u(0.9, 1.1), 55 * u(0.9, 1.1), 0.09 * u(0.85, 1.2), u(0.8, 1.15),
                       0.15, 0.1, 0.06, 0.3)
        shot = resample(shot, u(0.95, 1.05))
        gain = u(0.75, 1.0)
        if rng.random() < 0.1:      # now and then a slightly weaker round
            gain *= 0.8
        at = max(0.0, i * interval + u(-0.005, 0.005))
        mix_at(out, shot, at, gain)
    end = (shots - 1) * interval
    t = t_axis(len(out) / SR)
    env = np.minimum(1.0, t / 0.04) * np.where(t < end, 1.0, np.exp(-(t - end) / 0.3))
    tail = fft_filter(noise(len(out) / SR), low=180, high=1800) * env
    out += tail * 0.1 * np.max(np.abs(out))
    return reverb(out, 0.5, 0.15, 0.12)


def rifle_burst_naive(shots=15, interval=0.1):
    """For comparison: the full single-shot rifle.wav (with its long tail) layered at every round."""
    shot = rifle()
    out = np.zeros(int((shots * interval) * SR) + len(shot))
    for i in range(shots):
        mix_at(out, shot, i * interval)
    return out


def rocket_launch():
    """Rocket motor ignition: dull boom, rising whoosh and a hiss that fades into the distance."""
    length = 1.6
    out = np.zeros(int(length * SR))
    mix_at(out, fft_filter(noise(0.6), high=500, low=40) * decay(0.6, 0.18, 0.002), 0, 1.4)
    n = int(1.4 * SR)
    t = np.arange(n) / SR
    env = np.minimum(1.0, t / 0.08) * np.exp(-t / 0.5)
    mix_at(out, fft_filter(noise(1.4), low=300, high=3500) * env, 0, 0.9)
    mix_at(out, fft_filter(noise(0.15), low=1500) * decay(0.15, 0.02), 0, 0.6)   # ignition snap
    out = np.tanh(out * 2.0 / (np.max(np.abs(out)) + 1e-9))
    return reverb(out, 0.5, 0.15, 0.15)


def rocket_flight(length=2.0, crossfade=0.3):
    """Seamless loop of the motor roar: crossfade the tail into the head so it repeats without a click."""
    total = length + crossfade
    roar = fft_filter(noise(total), low=120, high=2800)
    rumble = fft_filter(noise(total), low=50, high=250)
    hiss = fft_filter(noise(total), low=2500, high=7000)
    t = t_axis(total)
    flutter = 1.0 + 0.15 * np.sin(2 * np.pi * 9 * t)
    x = roar * flutter + rumble * 1.2 + hiss * 0.25
    n = int(length * SR)
    xf = min(int(crossfade * SR), len(x) - n)
    out = x[:n].copy()
    ramp = np.linspace(0, 1, xf)
    out[:xf] = out[:xf] * ramp + x[n:n + xf] * (1 - ramp)
    return out


def rocket_explosion():
    """Bigger and lower than the grenade: the grenade explosion slowed down by 20 percent."""
    return resample(grenade(), 0.8)


LOOPS = {"rocket_flight"}

SOUNDS = {
    "pistol": pistol,
    "rifle": rifle,
    "shotgun": shotgun,
    "sniper": sniper,
    "camera": camera,
    "grenade": grenade,
    "smoke": smoke,
    "rocket_launch": rocket_launch,
    "rocket_flight": rocket_flight,
    "rocket_explosion": rocket_explosion,
    "rifle_burst": rifle_burst,
    "rifle_burst_naive": rifle_burst_naive,
}


def finish(x, peak=0.9, fade_ms=15, loop=False):
    """Trim silence at the end, fade out, normalize the peak and quantize to 16 bit."""
    x = x - np.mean(x)
    x = x / (np.max(np.abs(x)) + 1e-9) * peak
    if loop:    # a loop must keep its exact length and ends, or the seam clicks
        return np.round(x * 32767).astype(np.int16)
    above = np.nonzero(np.abs(x) > 0.002)[0]
    if len(above):
        x = x[:above[-1] + 1]
    fade = min(len(x), int(fade_ms / 1000 * SR))
    x[-fade:] *= np.linspace(1, 0, fade)
    return np.round(x * 32767).astype(np.int16)


def write_wav(path, samples):
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(samples.tobytes())


def main():
    out_dir = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).parent / "out"
    out_dir.mkdir(parents=True, exist_ok=True)
    for name, fn in SOUNDS.items():
        samples = finish(fn(), loop=name in LOOPS)
        write_wav(out_dir / f"{name}.wav", samples)
        f = samples.astype(np.float64) / 32767
        rms_db = 20 * np.log10(np.sqrt(np.mean(f ** 2)) + 1e-9)
        clipped = int(np.sum(np.abs(samples) >= 32767))
        print(f"{name:8s} {len(samples) / SR:5.2f}s  peak {np.max(np.abs(f)):.2f}  rms {rms_db:6.1f} dB  clipped {clipped}")


if __name__ == "__main__":
    main()
