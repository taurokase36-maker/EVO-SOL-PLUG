// EVO SOL — suavizador de resonancias inteligente (STFT).
// Analiza el espectro de la voz en tiempo real, detecta picos que sobresalen
// de su envolvente (resonancias, "pinchazos", nasalidad, dureza) y los atenúa
// solo cuando aparecen.
#pragma once

#include "Common.h"
#include <complex>

namespace evo
{
class FFT
{
public:
    void prepare (int size)
    {
        n = size;
        logN = 0;
        while ((1 << logN) < n) ++logN;
        rev.resize ((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            int r = 0;
            for (int b = 0; b < logN; ++b) if (i & (1 << b)) r |= 1 << (logN - 1 - b);
            rev[(size_t) i] = r;
        }
        tw.resize ((size_t) n / 2);
        for (int k = 0; k < n / 2; ++k)
            tw[(size_t) k] = std::polar (1.0f, (float) (-kTwoPi * k / n));
    }

    // FFT compleja in-place; inverse = true calcula la inversa (sin normalizar)
    void transform (std::complex<float>* x, bool inverse) const
    {
        for (int i = 0; i < n; ++i)
            if (i < rev[(size_t) i]) std::swap (x[i], x[rev[(size_t) i]]);
        for (int len = 2; len <= n; len <<= 1)
        {
            const int half = len >> 1;
            const int step = n / len;
            for (int i = 0; i < n; i += len)
                for (int j = 0; j < half; ++j)
                {
                    std::complex<float> w = tw[(size_t) (j * step)];
                    if (inverse) w = std::conj (w);
                    const std::complex<float> u = x[i + j];
                    const std::complex<float> v = x[i + j + half] * w;
                    x[i + j] = u + v;
                    x[i + j + half] = u - v;
                }
        }
    }
    int size() const { return n; }

private:
    int n = 0, logN = 0;
    std::vector<int> rev;
    std::vector<std::complex<float>> tw;
};

class ResonanceSuppressor
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        N = fs <= 50000.0 ? 512 : (fs <= 100000.0 ? 1024 : 2048);
        hop = N / 4;
        fft.prepare (N);
        win.resize ((size_t) N);
        for (int i = 0; i < N; ++i)
            win[(size_t) i] = (float) std::sqrt (0.5 - 0.5 * std::cos (kTwoPi * i / N)); // sqrt-Hann periódica
        in.assign ((size_t) N, 0.0f);
        acc.assign ((size_t) N, 0.0f);
        frame.assign ((size_t) N, {});
        const int bins = N / 2 + 1;
        level.assign ((size_t) bins, 0.0f);
        prefix.assign ((size_t) bins + 1, 0.0f);
        red.assign ((size_t) bins, 0.0f);
        redSm.assign ((size_t) bins, 0.0f);
        weight.assign ((size_t) bins, 0.0f);
        for (int k = 0; k < bins; ++k)
        {
            const double f = (double) k * fs / N;
            double w = 0.0;
            if (f > 160.0 && f < 15000.0)
            {
                w = 0.65;
                if (f > 1200.0 && f < 6500.0) w = 1.0;            // zona de dureza
                else if (f > 250.0 && f < 1200.0) w = 0.85;       // nasal / "boxy"
            }
            weight[(size_t) k] = (float) w;
        }
        relCoef = 1.0f - (float) std::exp (-(double) hop / (0.09 * fs));
        atkCoef = 1.0f - (float) std::exp (-(double) hop / (0.004 * fs));
        ip = ap = hc = 0;
        avgReductionDb = 0.0f;
    }

    int getLatencySamples() const noexcept { return N; }
    void setDepth (float d) { depth = clampv (d, 0.0f, 1.0f); }
    float getReductionDb() const noexcept { return avgReductionDb; }

    inline float process (float x) noexcept
    {
        in[(size_t) ip] = x;
        ip = (ip + 1) % N;
        const float y = acc[(size_t) ap];
        acc[(size_t) ap] = 0.0f;
        ap = (ap + 1) % N;
        if (++hc >= hop)
        {
            hc = 0;
            processFrame();
        }
        return y;
    }

private:
    void processFrame() noexcept
    {
        for (int i = 0; i < N; ++i)
            frame[(size_t) i] = { in[(size_t) ((ip + i) % N)] * win[(size_t) i], 0.0f };
        fft.transform (frame.data(), false);

        const int bins = N / 2 + 1;
        const float d = depth;
        float sumRed = 0.0f, sumW = 0.0f;

        if (d > 0.0f)
        {
            for (int k = 0; k < bins; ++k)
                level[(size_t) k] = 20.0f * std::log10 (std::abs (frame[(size_t) k]) + 1.0e-9f);
            prefix[0] = 0.0f;
            for (int k = 0; k < bins; ++k) prefix[(size_t) k + 1] = prefix[(size_t) k] + level[(size_t) k];

            const float thr = 8.0f - 4.5f * d;
            for (int k = 0; k < bins; ++k)
            {
                const int hw = std::max (3, (int) std::lround (k * 0.18));
                const int a = std::max (0, k - hw), b = std::min (bins - 1, k + hw);
                const float env = (prefix[(size_t) b + 1] - prefix[(size_t) a]) / (float) (b - a + 1);
                const float excess = level[(size_t) k] - env - thr;
                red[(size_t) k] = excess > 0.0f ? std::min (excess * (0.5f + 0.5f * d) * weight[(size_t) k], 12.0f * d) : 0.0f;
            }
        }
        else
        {
            std::fill (red.begin(), red.end(), 0.0f);
        }

        for (int k = 0; k < bins; ++k)
        {
            // Suavizado en frecuencia (3 puntos) y en tiempo (ataque rápido / release lento)
            const float r = 0.25f * red[(size_t) std::max (0, k - 1)] + 0.5f * red[(size_t) k]
                            + 0.25f * red[(size_t) std::min (bins - 1, k + 1)];
            float& s = redSm[(size_t) k];
            s += (r > s ? atkCoef : relCoef) * (r - s);
            const float g = dbToGain (-s);
            frame[(size_t) k] *= g;
            if (k > 0 && k < N / 2) frame[(size_t) (N - k)] *= g;
            const float mag = std::abs (frame[(size_t) k]);
            sumRed += s * mag; sumW += mag;
        }
        avgReductionDb = sumW > 0.0f ? sumRed / sumW : 0.0f;

        fft.transform (frame.data(), true);
        const float norm = 0.5f / (float) N; // 1/N de la IFFT y 1/2 de la suma de ventanas
        for (int i = 0; i < N; ++i)
            acc[(size_t) ((ap + i) % N)] += frame[(size_t) i].real() * win[(size_t) i] * norm;
    }

    double fs = 44100.0;
    int N = 512, hop = 128, ip = 0, ap = 0, hc = 0;
    FFT fft;
    std::vector<float> win, in, acc, level, prefix, red, redSm, weight;
    std::vector<std::complex<float>> frame;
    float depth = 0.0f, relCoef = 0.1f, atkCoef = 0.5f, avgReductionDb = 0.0f;
};

} // namespace evo
