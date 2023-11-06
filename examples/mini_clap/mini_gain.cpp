// mini_gain.cpp
#include <clap/clap.h>

#include <set>
#include <cmath>
#include <cstring>
#include <format>
#include <cstring>
#include <iostream>

class MiniGain {
public:
    constexpr static const clap_plugin_descriptor Descriptor = {
        .clap_version = CLAP_VERSION,
        .id = "mini.gain",
        .name = "MiniGain",
        .vendor = "Example",
        .version = "1.0.0",
        .description = "A Minimal CLAP plugin",
        .features = (const char*[]){ CLAP_PLUGIN_FEATURE_MIXING, nullptr }
    };

    static clap_plugin* create(const clap_host *host) {
        std::cout << std::format("{} -- Creating instance for host: <{}, v{}, {}>\n",
            Descriptor.name, host->name, host->version, pluginInstances.size()
        );
        auto [plug, success] = pluginInstances.emplace(new MiniGain(host));
        return success ? &(*plug)->mPlugin : nullptr;
    }

    void destroy() {
        pluginInstances.erase(this);
        std::cout << std::format("{} -- Destroying instance. {} plugins left\n",
            Descriptor.name, pluginInstances.size()
        );
    }

    clap_process_status process(const clap_process *process) {
        const auto *inEvents = process->in_events;
        const auto numEvents = process->in_events->size(inEvents);
        uint32_t eventIndex = 0;

        for (uint32_t frame = 0; frame < process->frames_count; ++frame) {
            // Process all events at given @frame, there may be more than one.
            while (eventIndex < numEvents) {
                const auto *eventHeader = inEvents->get(inEvents, eventIndex);
                if (eventHeader->time != frame) // Events must be sorted by time.
                    break;
                switch(eventHeader->type) {
                    case CLAP_EVENT_PARAM_VALUE: {
                        const auto *event = reinterpret_cast<const clap_event_param_value*>(eventHeader);
                        if (event->param_id == 0)       // Since we only interact with the parameter from this
                            setParamGain(event->value); // thread we don't need to synchronize access to it.
                    };
                }
                ++eventIndex;
            }
            // Process audio for given @frame.
            const float gain = std::pow(10.0f, static_cast<float>(paramGain()) / 20.0f);
            const float inputL = process->audio_inputs->data32[0][frame];
            const float inputR = process->audio_inputs->data32[1][frame];
            process->audio_outputs->data32[0][frame] = inputL * gain;
            process->audio_outputs->data32[1][frame] = inputR * gain;
        }
        return CLAP_PROCESS_SLEEP;
    }

    void setParamGain(double value) noexcept {
        mParamGain = value;
    }

    [[nodiscard]] double paramGain() const noexcept {
        return mParamGain;
    }

private:
    explicit MiniGain(const clap_host *host) : mHost(host) { initialize(); }
    static MiniGain *self(const clap_plugin *plugin);
    void initialize() {
        initializePlugin();
        initializeExtAudioPorts();
        initializeExtParams();
    }
    void initializePlugin();
    void initializeExtAudioPorts();
    void initializeExtParams();

private:
    [[maybe_unused]] const clap_host *mHost = nullptr;
    clap_plugin mPlugin = {};
    clap_plugin_audio_ports mExtAudioPorts = {};
    clap_plugin_params mExtParams = {};

    double mParamGain = 0.0;

    inline static std::set<MiniGain*> pluginInstances{};
};

MiniGain* MiniGain::self(const clap_plugin *plugin) {
    // Cast plugin_data back to MiniGain* to retrieve the class instance.
    return static_cast<MiniGain*>(plugin->plugin_data);
}
// A glue layer between our C++ class and the C API.
void MiniGain::initializePlugin() {
    mPlugin.desc = &Descriptor;
    mPlugin.plugin_data = this; // Link this instance with the plugin data.

    mPlugin.destroy = [](const clap_plugin* p) {
        self(p)->destroy();
    };
    mPlugin.process = [](const clap_plugin* p, const clap_process* proc) {
        return self(p)->process(proc);
    };
    mPlugin.get_extension = [](const clap_plugin* p, const char* id) -> const void* {
        if (!strcmp(id, CLAP_EXT_PARAMS))
            return &self(p)->mExtParams;
        else if (!strcmp(id, CLAP_EXT_AUDIO_PORTS))
            return &self(p)->mExtAudioPorts;
        return nullptr;
    };
    // Simplified for brevity.
    mPlugin.init = [](const auto*) { return true; };
    mPlugin.activate = [](const auto*, double, uint32_t, uint32_t) { return true; };
    mPlugin.deactivate = [](const auto*) {};
    mPlugin.start_processing = [](const auto*) { return true; };
    mPlugin.stop_processing = [](const auto*) {};
    mPlugin.reset = [](const auto*) {};
    mPlugin.on_main_thread = [](const auto*) {};
}

void MiniGain::initializeExtAudioPorts() {
    mExtAudioPorts = {
        .count = [](const clap_plugin*, bool) -> uint32_t { return 1; },
        .get = [](const clap_plugin*, uint32_t index, bool isInput, clap_audio_port_info *info) {
            if (index != 0)
                return false;
            info->id = 0;
            std::snprintf(info->name, sizeof(info->name), "%s %s", Descriptor.name, isInput ? "IN" : "OUT");
            info->channel_count = 2;
            info->flags = CLAP_AUDIO_PORT_IS_MAIN;
            info->port_type = CLAP_PORT_STEREO;
            info->in_place_pair = CLAP_INVALID_ID;
            return true;
        }
    };
}

void MiniGain::initializeExtParams() {
    mExtParams = {
        .count = [](const clap_plugin*) {
            return uint32_t(1);
        },
        .get_info = [](const clap_plugin*, uint32_t index, clap_param_info *info) -> bool {
            if (index != 0)
                return false;
            info->id = 0;
            info->flags = CLAP_PARAM_IS_AUTOMATABLE;
            info->cookie = nullptr;
            std::snprintf(info->name, sizeof(info->name), "%s", "Gain");
            std::snprintf(info->module, sizeof(info->module), "%s %s", MiniGain::Descriptor.name, "Module");
            info->min_value = -40.0;
            info->max_value = 40.0;
            info->default_value = 0.0;
            return true;
        },
        .get_value = [](const clap_plugin* p, clap_id id, double *out) -> bool {
            if (id != 0)
                return false;
            *out = self(p)->paramGain();
            return true;
        },
        .value_to_text = [](const clap_plugin*, clap_id id, double value, char* out, uint32_t outSize) -> bool {
            if (id != 0)
                return false;
            std::snprintf(out, outSize, "%g %s", value, " dB");
            return true;
        },
        .text_to_value = [](const clap_plugin*, clap_id id, const char* text, double* out) -> bool {
            if (id != 0)
                return false;
            *out = std::strtod(text, nullptr);
            return true;
        },
        .flush = [](const clap_plugin*, const auto*, const auto*) {
            // noop
        },
    };
}

// The factory is responsible for creating plugin instances.
const clap_plugin_factory pluginFactory = {
    // This CLAP has only one plugin to offer
    .get_plugin_count = [](auto*) -> uint32_t {
        return 1;
    },
    // Return the metadata for 'MiniGain'
    .get_plugin_descriptor = [](auto*, uint32_t idx) -> const auto* {
        return (idx == 0) ? &MiniGain::Descriptor : nullptr;
    },
    // Create a plugin if the IDs match.
    .create_plugin = [](auto*, const clap_host* host, const char* id) -> const auto* {
        if (strcmp(id, MiniGain::Descriptor.id) == 0)
            return MiniGain::create(host);
        return static_cast<clap_plugin*>(nullptr);
    }
};

// 'clap_entry' is the only symbol exported by the plugin.
extern "C" CLAP_EXPORT const clap_plugin_entry clap_entry = {
    .clap_version = CLAP_VERSION,

    // Called after the DSO is loaded and before it is unloaded.
    .init = [](const char* path) -> bool {
        std::cout << std::format("MiniGain -- initialized: {}\n", path);
        return true;
    },
    .deinit = []() -> void {
        std::cout << "MiniGain -- deinitialized\n";
    },

    // Provide a factory for creating 'MiniGain' instances.
    .get_factory = [](const char* factoryId) -> const void* {
        if (strcmp(factoryId, CLAP_PLUGIN_FACTORY_ID) == 0) // sanity check
            return &pluginFactory;
        return nullptr;
    }
};
