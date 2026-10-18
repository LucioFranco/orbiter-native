#include "Platform/Audio.h"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>
int main(int argc, char **argv) {
    try {
        if (argc != 2) throw std::runtime_error("sound file required");
        orbiter::platform::AudioMixer mixer(orbiter::platform::AudioMixer::Output::Device);
        auto clip = mixer.Load(argv[1]);
        if (!clip || !clip->Play(false,0.2f)) throw std::runtime_error("device probe sound failed to start");
        bool observedPlaying = clip->IsPlaying();
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (clip->IsPlaying() && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (!observedPlaying || clip->IsPlaying()) throw std::runtime_error("audio device did not advance one-shot playback");
        std::cout << "Native audio device opened and consumed the one-shot through its callback\n";
        return 0;
    } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
