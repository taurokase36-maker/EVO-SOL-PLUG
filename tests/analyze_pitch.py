"""Mide el pitch de los WAV de salida con un estimador independiente (NSDF / McLeod)
y verifica que caigan en las notas esperadas."""
import sys
import numpy as np
from scipy.io import wavfile

FS = 44100
LAT = int(sys.argv[1]) if len(sys.argv) > 1 else 2190


def load(path):
    fs, x = wavfile.read(path)
    if x.ndim > 1:
        x = x[:, 0]
    return x.astype(np.float64)


def nsdf_pitch(frame, fs, fmin=60, fmax=1200):
    n = len(frame)
    frame = frame - frame.mean()
    if np.sqrt(np.mean(frame ** 2)) < 1e-4:
        return np.nan, 0
    size = 1 << int(np.ceil(np.log2(2 * n)))
    F = np.fft.rfft(frame, size)
    acf = np.fft.irfft(F * np.conj(F))[:n]
    sq = frame ** 2
    m = np.cumsum(sq[::-1])[::-1] + np.cumsum(sq)[::-1][::-1]
    m = sq.sum() * 2 - np.concatenate(([0], np.cumsum(sq)[:-1])) - np.concatenate(([0], np.cumsum(sq[::-1])[:-1]))
    nsdf = 2 * acf / np.maximum(m, 1e-12)
    tmin, tmax = int(fs / fmax), int(fs / fmin)
    seg = nsdf[tmin:tmax]
    if len(seg) < 3:
        return np.nan, 0
    peaks = [i for i in range(1, len(seg) - 1) if seg[i] > seg[i - 1] and seg[i] >= seg[i + 1] and seg[i] > 0]
    if not peaks:
        return np.nan, 0
    best = max(seg[p] for p in peaks)
    for p in peaks:
        if seg[p] >= 0.85 * best:
            a, b, c = seg[p - 1], seg[p], seg[p + 1]
            off = 0.5 * (a - c) / (a - 2 * b + c) if (a - 2 * b + c) != 0 else 0
            tau = tmin + p + off
            return fs / tau, b
    return np.nan, 0


def track(x, fs, hop=0.01, win=0.04):
    out = []
    w = int(win * fs)
    for start in range(0, len(x) - w, int(hop * fs)):
        f, c = nsdf_pitch(x[start:start + w] * np.hanning(w), fs)
        t = (start + w / 2) / fs
        out.append((t, f, c))
    return out


def midi(f):
    return 69 + 12 * np.log2(f / 440.0)


# Segmentos (tiempo de entrada) con la nota esperada para key C mayor
SEGMENTS = [
    ("A3 +35c", 0.30, 0.85, 57.35, 57),
    ("C4 -40c", 1.10, 1.65, 59.60, 60),
    ("D4 vib", 1.95, 2.75, 62.20, 62),
    ("E3 -30c", 3.10, 3.75, 51.70, 52),
]


def report(name, path, shift=0.0, lat=LAT):
    x = load(path)
    tr = track(x, FS)
    print(f"\n== {name} ==")
    for label, t0, t1, src, tgt in SEGMENTS:
        vals = [midi(f) for (t, f, c) in tr if t0 + lat / FS <= t <= t1 + lat / FS and np.isfinite(f) and c > 0.6]
        if not vals:
            print(f"  {label}: sin pitch")
            continue
        v = np.array(vals)
        exp = tgt + shift
        print(f"  {label}: mediana={np.median(v):7.3f}  esperado={exp:6.2f}  error={100*(np.median(v)-exp):+6.1f} cents  "
              f"desvío={100*np.std(v):5.1f} cents  (entrada {src})")


if __name__ == "__main__":
    report("entrada (sin procesar)", "out/pitch_input.wav", lat=0)
    report("hard (retune 0)", "out/pitch_hard.wav")
    report("natural (60 ms)", "out/pitch_natural.wav")
    report("transpose +3", "out/pitch_transpose3.wav", shift=3)
    report("throat +3", "out/pitch_throat_up.wav")
    report("throat -3", "out/pitch_throat_down.wav")
    report("flex 0.8", "out/pitch_flex.wav")
    report("armonía 1 (3ra arriba)", "out/pitch_harmony_h1.wav")
    report("armonía 2 (5ta abajo)", "out/pitch_harmony_h2.wav")
