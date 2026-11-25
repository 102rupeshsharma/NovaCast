#include "audio/audio_module.h"

namespace nc::audio {

ModuleInfo module_info() {
    return ModuleInfo{
        "audio",
        "Yamaha AICA: ARM7DI sound CPU, 64-channel PCM/ADPCM engine, DSP, 2 MB wave RAM, RTC",
        ModuleStatus::NotImplemented,
        "No sound CPU, channels or mixing yet. No audio output is produced.",
    };
}

} // namespace nc::audio
