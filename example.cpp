#include <fstream>
#include <iostream>
#include <algorithm>
#include <vector>

#include "sf2.hpp"

#define SAMPLE_RATE 16000

enum WAV_FormatTag : uint16_t
{
    WAVE_FORMAT_PCM = 0x0001,
    WAVE_FORMAT_IEEE_FLOAT = 0x0003,
    WAVE_FORMAT_ALAW = 0x0006,
    WAVE_FORMAT_MULAW = 0x0007,
    WAVE_FORMAT_EXTENSIBLE = 0xFFFE
};

struct WAV_Header {
    //RIFF chunk
    uint8_t RIFF_ckID[4] = { 'R', 'I', 'F', 'F' };
    uint32_t RIFF_cksize;
    uint8_t RIFF_WAVEID[4] = { 'W', 'A', 'V', 'E' };
    //fmt subchunk
    uint8_t fmt_ckID[4] = { 'f', 'm', 't', ' ' };
    uint32_t fmt_cksize = 16;
    uint16_t fmt_wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    uint16_t fmt_nChannels = 2;
    uint32_t fmt_nSamplesPerSec = SAMPLE_RATE;
    uint32_t fmt_nAvgBytesPerSec = SAMPLE_RATE*fmt_nChannels*sizeof(float);
    uint16_t fmt_nBlockAlign = fmt_nChannels*sizeof(float);
    uint16_t fmt_wBitsPerSample = 8*sizeof(float);
    //data subchunk
    uint8_t data_ckID[4] = { 'd', 'a', 't', 'a' };
    uint32_t data_cksize;
};

int main(int argc, char *argv[]) {
    static_assert(sizeof(WAV_Header) == 44, "");

    if (argc <= 1)
        std::cerr << "must specify a path to sf2 file" << std::endl;

    std::string sf_path(argv[1]);

    //setup RIFF parser and parse the soundfont
    RIFF::stream stream;
    stream.src = nullptr;
    stream.func_read_ptr = [](void* src, void* dest, size_t size)->size_t
    {
        static_cast<std::ifstream*>(src)->read((char*)dest, size);
        return static_cast<std::ifstream*>(src)->gcount();
    };
    stream.func_skip_ptr = [](void* src, size_t size)->size_t
    {
        static_cast<std::ifstream*>(src)->ignore(size);
        return static_cast<std::ifstream*>(src)->gcount();
    };
    stream.func_getpos_ptr = [](void* src)->size_t
    {
        return static_cast<std::ifstream*>(src)->tellg();
    };
    stream.func_setpos_ptr = [](void* src, size_t pos)
    {
        static_cast<std::ifstream*>(src)->clear();
        static_cast<std::ifstream*>(src)->seekg(pos, static_cast<std::ifstream*>(src)->beg);
    };
    RIFF::RIFF riff;

    auto file = std::make_unique<std::ifstream>(sf_path, std::ios::binary);
    stream.src = file.get();
    riff.parse(stream, false);

    //setup soundfont synthesizer and 1 channel
    SF2::SoundFont2 sf(&riff, &stream);
    SF2::SoundFont2::Channel channel;
    channel.sf = &sf;
    channel.SetPreset(0, 0);

    //play C major chord
    channel.NoteOn(60, 127, SAMPLE_RATE);
    channel.NoteOn(64, 127, SAMPLE_RATE);
    channel.NoteOn(67, 127, SAMPLE_RATE);

    //render 2-channel float data
    uint32_t num_samples = 512*100;
    std::vector<float> output;
    output.resize(num_samples*2);
    channel.Render(output.data(), output.data()+num_samples, num_samples, SAMPLE_RATE);
    //interleave data
    std::vector<float> tmp_output(output.size());
    std::swap(tmp_output, output);
    for(size_t i = 0; i < output.size()/2; ++i)
    {
        output[i*2] = tmp_output[i];
        output[i*2+1] = tmp_output[i+num_samples];
    }

    //write to WAV
    WAV_Header wav;
    size_t data_size = output.size()*sizeof(float);
    wav.RIFF_cksize = data_size + sizeof(wav) - 8;
    wav.data_cksize = data_size + sizeof(wav) - 44;
    std::ofstream out("output.wav", std::ios::binary);
    out.write(reinterpret_cast<const char *>(&wav), sizeof(wav));
    out.write(reinterpret_cast<const char *>(output.data()), data_size);

    return 0;
}