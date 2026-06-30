// by chatgpt im sorry

#include "fft.h"
#include "../libs/kissfft/kiss_fft.h"

#include <cmath>
#include <vector>

float getFundamental(const std::vector<float>& frame, int sampleRate)
{
    int N = static_cast<int>(frame.size());

    int minBin = (int)(20.0f * N / sampleRate);
    int maxBin = (int)(220.0f * N / sampleRate);

    kiss_fft_cfg cfg = kiss_fft_alloc(N, 0, nullptr, nullptr);

    if (!cfg)
        return 0.0f;

    std::vector<kiss_fft_cpx> in(N);
    std::vector<kiss_fft_cpx> out(N);

    // Hann window + mono input
    for (int i = 0; i < N; i++)
    {
        float mono = frame[i];

        float w =
            0.5f *
            (1.0f - cosf(2.0f * M_PI * i / (N - 1)));

        in[i].r = mono * w;
        in[i].i = 0.0f;
    }

    kiss_fft(cfg, in.data(), out.data());

    int bestIndex = 1;
    float bestMag = 0.0f;

    // ignore DC + high freq
    for (int i = minBin; i < maxBin; i++)
    {
        float re = out[i].r;
        float im = out[i].i;

        float mag = re * re + im * im;

        if (mag > bestMag)
        {
            bestMag = mag;
            bestIndex = i;
        }
    }

    free(cfg);

    return bestIndex *
           sampleRate /
           (float)N;
}