## 2.2 The CLAP Audio Plugin Standard

Originating from the collaboration between Berlin-based companies Bitwig and
u-he, CLAP (**CL**ever **A**udio **P**lugin) emerged as a new plugin standard. Born
from developer Alexandre Bique's vision in 2014, it was revitalized in 2021
with a focus on three core concepts: Simplicity, Clarity, and Robustness.

CLAP stands out for its:

- *Consistent API* and *flat architecture*, making plugin creation more
intuitive.
- *Adaptability*, allowing flexible integration of unique features like
  per-voice modulation (MPE^[Midi Polyphonic Expression] on steroids) or
  a host thread-pool extension.
- Open-source ethos, making the standard accessible under the [MIT
  License](https://en.wikipedia.org/wiki/MIT_License).

CLAP establishes a stable and backward compatible Application Binary Interface.
This ABI forms a bridge for communication between digital audio workstations
and audio plugins such as synthesizers and effect units.

### 2.2.1 CLAP basics

The principal advantage of CLAP is its simplicity. In contrast to other plugin
standards, such as VST, which often involve complex structures including deep
inheritance hierarchies that complicate both debugging and understanding the
code, CLAP offers a refreshingly straightforward and flat architecture. With
CLAP, a single exported symbol is the foundation from which the entire plugin
functionality extends: the `clap_plugin_entry` type. This must be made
available by the DSO and is the only exported symbol.

> Note: We will use the term *CLAP* to refer to the DSO that houses the plugin
> implementation of the clap interface. This is a common phrase in the CLAP
> community.

```cpp
// <clap/entry.h>
typedef struct clap_plugin_entry {
   clap_version_t clap_version;
   bool(CLAP_ABI *init)(const char *plugin_path);
   void(CLAP_ABI *deinit)(void);
   const void *(CLAP_ABI *get_factory)(const char *factory_id);
} clap_plugin_entry_t;
```

The `clap_version_t` type specifies the version the plugin is created with.
Following this, there are three function pointers declared:

1. The initialization function `bool init(const char*)` is the first function
   called by the host. It is primarily used for plugin scanning and is designed
   to execute quickly.
2. The de-initialization function `void deinit(void)` is invoked when the DSO
   is unloaded, which typically occurs after the final plugin instance is
   closed.
3. The `const void* get_factory(const char*)` serves as the "constructor" for
   the plugin, tasked with creating new plugin instances. A notable aspect of
   CLAP plugins is their containerized nature, allowing a single DSO to
   encapsulate multiple distinct plugins.

```cpp
typedef struct clap_plugin_factory {
   uint32_t(CLAP_ABI *get_plugin_count)(const struct clap_plugin_factory *factory);

   const clap_plugin_descriptor_t *(CLAP_ABI *get_plugin_descriptor)(
        const struct clap_plugin_factory *factory, uint32_t index);

   const clap_plugin_t *(CLAP_ABI *create_plugin)(
        const struct clap_plugin_factory *factory, const clap_host_t *host,
        const char *plugin_id);

} clap_plugin_factory_t;
```

The plugin factory acts as a hub for creating plugin instances within a given
CLAP object. The structure requires three function pointers:

1. `get_plugin_count(~)` determines the number of distinct plugins available
   within the CLAP.
2. `get_plugin_descriptor(~)` retrieves a description of each plugin, which is
   used by the host to present information about the plugin to users.
3. `create_plugin(~)` is responsible for instantiating the plugin(s).

At the heart of the communication between the host and a plugin is the
`clap_plugin_t` type. It defines the essential functions that a host will invoke
to control the plugin, such as initializing, processing audio, and handling
events. It is through this interface that the plugin exposes its capabilities
and responds to the host, thereby allowing for the dynamic and interactive
processes required for audio manipulation and creation.

![CLAP factory](images/clap_factory.png){width=77%}

### 2.2.2 Creating a CLAP Plugin

To illustrate the ease with which one can get started with CLAP, we will
develop a simple gain plugin, a basic yet foundational tool in audio
processing. A gain plugin's role is to control the output volume by adjusting
the input signal's level in decibels. Accordingly, our plugin will manage the
audio inputs and outputs and feature a single adjustable parameter: gain.

Setting up our project is straightforward. We'll create a directory for our
plugin, obtain the CLAP library, and prepare the initial files:

```bash
mkdir mini_clap && cd mini_clap
git clone https://github.com/free-audio/clap.git
touch mini_gain.cpp
touch CMakeLists.txt
```

Next, we'll configure the build system:

```cmake
cmake_minimum_required(VERSION 3.2)
project(MiniGain LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(target mini_gain)
add_library(${target} SHARED mini_gain.cpp)

# CLAP is a header-only library, so we just need to include it
target_include_directories(${target} PRIVATE clap/include)
```

This CMake snippet sets up the build environment for our *MiniGain* project. We
enable C++20 to use the latest language features and declare a shared library
named `mini_gain` which will be built from mini_gain.cpp.

To meet the CLAP naming conventions, we need to modify the library's output
name:

```cmake
# A CLAP is just a renamed shared library
set_target_properties(${target} PROPERTIES PREFIX "")
set_target_properties(${target} PROPERTIES SUFFIX ".clap")
```

This configuration tells CMake to output our library with the name
`mini_gain.clap`.

While the output is correctly named, it remains within the build directory,
which isn't automatically recognized by CLAP hosts. For practical development,
creating a symlink to the expected location is beneficial. We'll append a
post-build command to do just that:

```cmake
# Default search path for CLAP plugins. See also <clap/entry.h>
if(UNIX)
    if(APPLE)
        set(CLAP_USER_PATH "$ENV{HOME}/Library/Audio/Plug-Ins/CLAP")
    else()
        set(CLAP_USER_PATH "$ENV{HOME}/.clap")
    endif()
elseif(WIN32)
    set(CLAP_USER_PATH "$ENV{LOCALAPPDATA}\\Programs\\Common\\CLAP")
endif()

# Create a symlink post-build to make development easier
add_custom_command(
    TARGET ${target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink
    "$<TARGET_FILE:${target}>"
    "${CLAP_USER_PATH}/$<TARGET_FILE_NAME:${target}>"
)
```

We start by defining the entry point for the plugin. Here's how you might
proceed:

```cpp
// mini_gain.cpp
#include <clap/clap.h>

#include <set>
#include <format>
#include <cstring>
#include <iostream>

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
```

First, we include the `clap.h` header file, which aggregates all components
from the CLAP API. We also import some standard headers for later use and
initialize the fields of the `clap_plugin_entry` type with simple lambda
functions. Our plugin factory is designed to offer a single plugin within this
DSO:

```cpp
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
```

The `host` pointer serves as a means of communication from the plugin
towards the host, requesting necessary services and functionality.

The implementation of our plugin in modern C++ requires a mechanism to interact
with the C-API of the CLAP standard. Since the `clap_plugin_t` struct expects
static function pointers and member functions are not static, we resolve this
mismatch using *trampoline* functions, also known as *glue* routines. These
functions connect the static world of the C-API with the instance-specific
context of our C++ classes:

```cpp
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
        return nullptr; // TODO: Add extensions.
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
```

The code sets up glue routines to redirect calls from the static C API to our
C++ class methods. The `self` function retrieves the class instance from
`plugin_data`, facilitating the call to the relevant member functions. We focus
on `destroy` and `process`, with other functions returning defaults to
streamline our plugin's integration with the host.

Finally we create the `MiniGain` class which encapsulates the functionality of
our plugin:

```cpp
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
        return {}; // TODO: Implement processing logic.
    }

private:
    explicit MiniGain(const clap_host *host) : mHost(host) { initializePlugin(); }
    static MiniGain *self(const clap_plugin *plugin);
    void initializePlugin();

    // private members:
    [[maybe_unused]] const clap_host *mHost = nullptr;
    clap_plugin mPlugin = {};
    inline static std::set<MiniGain*> pluginInstances{};
};
```

The `Descriptor` in the MiniGain class contains essential metadata for the
plugin, including identifiers and versioning. The create function allocates a
plugin instance and stores it inside a static `std::set`. Overall, this
minimalistic plugin structure is achieved in approximately 100 lines of code.

![MiniGain hosted](images/clap_mini_hosted.png)

This implementation of the MiniGain plugin is already sufficient for it to be
recognized and loaded by CLAP-compatible hosts. At this stage, a developer
might wonder about how to debug such a plugin, considering it operates as a
shared library that has to be loaded and called.

### 2.2.3 Debugging

Debugging is a crucial step in software development. When the complexity of our
program increases or even if we just want to verify correct behavior, it is
great to step through the code with debuggers such as
[gdb](https://www.sourceware.org/gdb/) or [lldb](https://lldb.llvm.org/).

When developing shared objects, debugging introduces an additional layer of
complexity. We rely upon a host that has the CLAP standard implemented and
all features supported.

Fortunately, Bitwig offers a streamlined approach to this challenge. Since
their audio engine runs as a standalone executable, we can simply start it with
a debugger and latch onto the breakpoints of our plugin. To set up debugging in
Bitwig, carry out the following steps:

1. Bitwig supports different hosting modes. For debugging purposes, we want the
   plugin to be loaded within the same thread as the audio engine, so we use
   **Within Bitwig**

![Bitwig Hosting](images/bitwig_hosting.png){width=77%}

2. If the audio engine is running, we must first shut it down. Do this by
   right-clicking in the transport bar and choosing **terminate audio engine**.

Now we can execute the audio engine with the debugger of choice. The executable
is located inside Bitwigs installation directory. On my linux machine this
would be:

```bash
gdb /opt/bitwig-studio/bin/BitwigAudioEngine-X64-AVX2
(gdb) run
```

With the debugger running, you'll have access to debugging tools, and you can
monitor the console output for any print statements. For instance, here's what
you might see when creating and then removing three instances of the `MiniGain`
plugin:

```bash
MiniGain -- initialized: /home/wayn/.clap/mini_gain.clap
MiniGain -- Creating instance for host: <Bitwig Studio, v5.0.7, 0>
MiniGain -- Creating instance for host: <Bitwig Studio, v5.0.7, 1>
MiniGain -- Creating instance for host: <Bitwig Studio, v5.0.7, 2>

PluginHost: Destroying plugin with id 3
MiniGain -- Destroying instance. 2 plugins left
PluginHost: Destroying plugin with id 2
MiniGain -- Destroying instance. 1 plugins left
PluginHost: Destroying plugin with id 1
MiniGain -- Destroying instance. 0 plugins left
```

### 2.2.4 Extensions

The MiniGain plugin, in its current state, is not yet operational due to the
absence of needed extensions. While the fundamental framework of the plugin has
been established, the extensions are key to unlocking its full potential. The
CLAP repository’s `ext/` directory is home to a range of extensions, including
*gui* and *state*, among others. To fulfill our plugin's requirements of
managing a parameter and processing an audio input and output, we focus on
incorporating the **audio-ports** and **params** extensions:

```cpp
~~~
    void setParamGain(double value) noexcept { mParamGain = value; }
    [[nodiscard]] double paramGain() const noexcept { return mParamGain; }
private:
    explicit MiniGain(const clap_host *host) : mHost(host) { initialize(); }
    void initialize() {
        initializePlugin();
        initializeExtAudioPorts();
        initializeExtParams();
    }
    void initializePlugin();
    void initializeExtAudioPorts();
    void initializeExtParams();

    clap_plugin_audio_ports mExtAudioPorts = {};
    clap_plugin_params mExtParams = {};

    double mParamGain = 0.0;
~~~
```

We add an initialization function that bundles the setup processes for these
extensions. We maintain the parameter value in a dedicated variable and provide
access through getters and setters. Finally, to utilize these extensions, the
plugin communicates the supported functionalities back to the host:

```cpp
// initializePlugin() {
~~~
        .get_extension = [](const clap_plugin* p, const char* id) -> const void* {
            if (!strcmp(id, CLAP_EXT_PARAMS))
                return &self(p)->mExtParams;
            else if (!strcmp(id, CLAP_EXT_AUDIO_PORTS))
                return &self(p)->mExtAudioPorts;
            return nullptr;
        },
~~~
```

The host determines the plugin's supported extensions by calling this function
with all supported extension IDs. This ensures the host recognizes the plugin's
capabilities. For MiniGain, the next step involves implementing the audio-ports
extension as follows:

```cpp
void MiniGain::initializeExtAudioPorts() {
    mExtAudioPorts = {
        .count = [](const clap_plugin*, bool) -> uint32_t { return 1; },
        .get = [](const clap_plugin*, uint32_t index, bool isInput, clap_audio_port_info *info) {
            if (index != 0)
                return false;
            info->id = 0;
            std::snprintf(info->name, sizeof(info->name), "%s %s", Descriptor.name, isInput ? "IN" : "OUT");
            info->channel_count = 2; // Stereo
            info->flags = CLAP_AUDIO_PORT_IS_MAIN;
            info->port_type = CLAP_PORT_STEREO;
            info->in_place_pair = CLAP_INVALID_ID;
            return true;
        }
    };
}
```

With the `count` function, the plugin notifies the host about the plugin's
audio input and output capabilities . The `get` function further details the
configuration, marking a stereo channel setup. This establishes the essential
framework for the plugin to access audio input and output streams during the
process callback.

The parameter extension in MiniGain outlines the parameters the plugin
possesses and provides metadata about them. The initialization function for the
extension is defined as follows:

```cpp
void MiniGain::initializeExtParams() {
    mExtParams = {
        .count = [](const clap_plugin*) -> uint32_t {
            return 1; // Single parameter for gain
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
```

Here, `count` reveals that the plugin hosts a single gain parameter, and `get_info`
provides crucial details such as the parameter's range and default setting.

```cpp
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
```

The parameter's current value is fetched using `get_value`, while
`value_to_text` and `text_to_value` manage the conversion between numerical
values and user-readable text, appending 'dB' to indicate decibels. With these
definitions, compiling and loading the plugin into a host will now allow us to
interact with the parameter.

![MiniGain hosted](images/clap_minigain_hosted.png){width=33%}

### 2.2.5 Processing

To make the *MiniGain* plugin fully functional, we must finally tackle the
`process` callback, which is the heart of all audio processing and event
handling. This function is responsible for managing audio data and responding
to events, such as parameter changes or MIDI note triggers.

![CLAP parameter event](images/clap_events.png){width=66%}

CLAP has a sophisticated method for coupling events with audio buffers. Events
come with a header and payload; the header's `flags` denote the payload type,
and the payload contains the event data. In our case, it's the
`clap_event_param_value`, carrying information about parameter changes.

The `process` function works with frames, which encapsulate both the audio
samples and any associated event data in a time-ordered fashion. This approach
ensures that the audio processing is accurate and responsive to real-time
control changes.

![CLAP process](images/clap_process.png)

To implement the `process` function in *MiniGain*, we need to write code that
iterates through these frames, applies the gain value to the audio samples, and
handles any parameter change events. It is crucial to ensure that the gain
adjustments occur precisely at the event's timestamp within the audio buffer to
maintain tight synchronization between user actions and audio output.

By integrating this final piece, we'll have a working gain plugin that not only
processes audio but also responds dynamically to user interactions. The code
would look something like this:

```cpp
clap_process_status process(const clap_process *process) {
    const auto *inEvents = process->in_events;
    const auto numEvents = process->in_events->size(inEvents);
    uint32_t eventIndex = 0;

    for (uint32_t frame = 0; frame < process->frames_count; ++frame) {
        // Process all events at given @frame, there may be more than one.
        while (eventIndex < numEvents) {
            const auto *eventHeader = inEvents->get(inEvents, eventIndex);
            if (eventHeader->time != frame) // Consumned all event for @frame
                break;
            switch(eventHeader->type) {
                case CLAP_EVENT_PARAM_VALUE: {
                    const auto *event = reinterpret_cast<const clap_event_param_value*>(eventHeader);
                    // Since we only interact with the parameter from this
                    // thread we don't need to synchronize access to it.
                    if (event->param_id == 0)
                        setParamGain(event->value);
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
```

The specific implementation will involve fetching the gain parameter,
responding to parameter events, and manipulating the audio buffer accordingly.
Once implemented, compiling and running the plugin should yield a controllable
gain effect within the host application.
