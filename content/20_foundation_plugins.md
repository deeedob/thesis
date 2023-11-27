# Chapter 2: Foundation

## 2.1 Plugins

**Plug**-***ins***, at their essence, serve as dynamic extensions, enhancing
the capabilities of a plugin-loading host. One can perceive them as *on-demand*
features ready for deployment. Their popularity is evident across both the
software and hardware domains. For instance, they can be specialized filters
added to image processing applications like *Adobe Photoshop*, dynamic driver
modules integrated into operating systems such as *GNU/Linux* [@cppn2074], or
system-specific extensions found in frameworks like *Qt*.

![basic plugin architecture](images/plugin-basic.png){#fig:plugarch}

*@fig:plugarch* offers a visual representation of this mechanism. On the left,
we have the host, which is equipped with the capability to accommodate the
plugin interface. Centrally located is the plugin interface itself, serving as
the communication bridge between the host and the plugin. The right side
demonstrates the actual implementation of the plugin. Once the plugin is
successfully loaded by the host, it actively *requests* the necessary
functionalities from the plugin as and when required. This interaction entails
invoking specific functions and subsequently taking actions based on their
results. While plugins can, on occasion, prompt functions from the host, their
primary role is to respond to the host's requests. The foundation of this
interaction is the meticulously designed plugin interface that facilitates this
bilateral communication.

A practical illustration of such an interface is seen in the **CL**ever
**A**udio **P**lugin (CLAP hereon) format. This standard establishes the
communication protocols between a Digital Audio Workstation (DAW hereon) and
its various plugins, be it synthesizers, audio effects, or other elements. A
segment from the C-API reads:

```cpp
   // Call start processing before processing.
   // [audio-thread & active_state & !processing_state]
   bool(CLAP_ABI *start_processing)(const struct clap_plugin *plugin);
```

To unpack this, the function outlines the calling convention for a function
pointer named `start_processing`. This function returns a `bool` and receives a
constant pointer to the struct `clap_plugin` as an argument. The preprocessor
macro `CLAP_ABI` is an implementation detail without significant importance. In
a practical scenario, a plugin can assign a specific function to this pointer.
This allows the plugin to respond to calls made by the host, which controls the
timing of these calls

### 2.1.1 Shared Libraries and Plugins

When discussing plugins written in a compiled language, we typically refer to
them as shared libraries. A shared library, also known as a dynamic library or
DSO^[Dynamic Shared Object], is a reusable object that exports a table of
symbols (e.g. functions, variables, global data). These libraries are loaded
into shared memory once and made accessible to all instances that might utilize
them. This approach ensures efficient memory and resource management. Commonly
used libraries can greatly benefit from this. However, this efficiency can
compromise portability, as these libraries must either be present on the target
platform or packaged with the application.

A canonical example is the *standard C library* (libc). Given its presence in
nearly every application, the efficiency of shared libraries becomes evident.
To demonstrate, we'll examine the shared object dependencies of some standard
applications using the Linux utility **ldd**:

```bash
ldd /usr/bin/git
  linux-vdso.so.1 (0x00007ffcf7b98000)
  libpcre2-8.so.0 => /usr/lib/libpcre2-8.so.0 (0x00007f5c0f286000)
  libz.so.1 => /usr/lib/libz.so.1 (0x00007f5c0f26c000)
  libc.so.6 => /usr/lib/libc.so.6 (0x00007f5c0ec1e000)

ldd /usr/bin/gcc
  linux-vdso.so.1 (0x00007ffef8dfd000)
  libc.so.6 => /usr/lib/libc.so.6 (0x00007fcf68af9000)
```

We can see that both, `git` and `gcc` depend on the standard C library. Shared
libraries can be further categorized into:

- *Dynamically linked libraries* - The library is linked against the
  application after the compilation. The kernel then loads the library, if not
  already in shared memory, automatically upon execution.
- *Dynamically loaded libraries* - The application takes full control by
  loading the DSO manually with libraries like
  [dlopen](https://linux.die.net/man/3/dlopen) or
  [QLibrary](https://doc.qt.io/qt-6/qlibrary.html).


In the context of audio plugins, the approach of *dynamically loaded libraries*
is commonly used for loading plugin instances. Unlike dynamically linked
libraries, where the kernel handles the loading post-compilation, dynamically
loaded libraries give the application the autonomy to load the Dynamic Shared
Object (DSO hereon) manually. Considering the foundational role of shared
libraries in every plugin, it's beneficial to delve deeper into their
intricacies. Let's explore a basic example:

```cpp
!include examples/simplelib.cpp
```

This code defines a minimal shared library. After including the required
standard-headers, we define a compile time directive that is used to signal the
visibillity of the exported symbols. Windows and Unix based system differ here.
On Windows with MSVC the symbols are _not_ exported by default, and require
explicit marking with `__declspec(dllexport)`. On Unix based system we use the
visibillity attribute. You might noticed the absence of `__declspec(dllimport)`
here. We actually don't need it in this example because we're going to load the
library manually, like a plugin.

The function `void lib_hello()` is additionally marked with `extern "C"` to
provide C linkage, which makes this function also available to clients loading
this library from C-code. The function then simply prints the name and the
source location of the current file.

Now lets have a look at the host, which is loading the shared library during
its runtime. The implementation is Unix specific but would follow similar logic
on Windows as well:

```cpp
!include examples/simplehost.cpp
```

For simplicity reasons the error handling has been kept to a minimum. The code
seen above is basically all it takes to *dynamically load libraries*, and is
what plugin-hosts are doing to interact with the plugin interface.

To finalize this example, lets write a minimal build script and run our
`simplehost` executable.

```bash
!include examples/simplelib.sh
```

And finally run our script:

```bash
./simplelib
simplelib: called from simplelib.cpp:18
```

Throughout our discussions, we'll frequently reference the terms *ABI* and
*API*. To ensure clarity, let's demystify these terms. The **API**, an acronym
for **A**pplication **P**rogramming **I**nterface, serves as a blueprint that
dictates how different software components should interact. Essentially, it
acts as an agreement, ensuring that software pieces work harmoniously together.
If we think of software as a puzzle, the API helps ensure that the pieces fit
together. On the flip side, we have the **ABI** or **A**pplication **B**inary
**I**nterface, which corresponds to the actual binary that gets executed.

In software development, as we move from version 1.1 to 1.2, then to 1.3, and
so on, keeping a consistent interface is crucial. This is where ***Binary
Compatibility*** comes into play. It’s essential for ensuring that various
versions of software libraries can work together smoothly. Ideally, software
built with version 1.1 of an interface should seamlessly operate with version
1.3, without recompilation or other adjustments.

Take this example:

```cpp
// Version 1.0 of 'my_library'
typedef uint32_t MyType;
~~~
void my_library_api(MyType type);
```

Here, `MyType` is a 32-bit unsigned integer. But in the next version:

```cpp
// Version 1.1 of 'my_library'
typedef uint64_t MyType;
~~~
void my_library_api(MyType type);
```

`MyType` is updated to a 64-bit integer. This change maintains **Source
Compatibility** (the code still compiles) but breaks **Binary Compatibility**.
Binary compatibility is lost because the binary interface of the library, has
changed due to the increased size of `MyType`.

Maintaining binary compatibility is vital for the stability of systems,
especially when updating shared libraries. It allows users to upgrade software
without disrupting existing functionalities.

### 2.1.2 Audio Plugins

In the domain of audio plugins, two primary components define their structure:
the realtime audio section and the control section.

![basic audio plugin architecture](images/audio-plugin-basic.png){#fig:plugbasic}

*@fig:plugbasic*, shows a typical plugin interface with two distinct colors for
clarity. The green section represents the plugin's interaction with the host
through a low-priority main thread. This part primarily manages tasks like GUI
setup and other control functions. "Low priority" means that it can handle
non-deterministic operations, which may unpredictably take longer to complete.

The red segment signifies that the API is being invoked by a high-priority
realtime thread. These functions are called frequently and demand minimal
latency to promptly respond to the host. They process all incoming events from
the host, including parameter changes and other vital events essential for
audio processing. For instance, a gain plugin; A typical gain modifies the
output volume and possesses a single parameter: the desired gain in decibels to
amplify or diminish the output audio.^[Inspired by: [CppNow 2023, What is Low
Latency C++? (Part 2) - Timur Doumler](https://www.youtube.com/watch?v=5uIsadq-nyk)]

![Realtime overview](images/realtime-overview.png){ width=77% }

A challenge in audio programming stems from the rapidity with which the audio
callback needs a response. The standard structure unfolds as:

1. The host routinely invokes a process function, forwarding audio buffers,
   along with any events related to the audio plugin (such as a user adjusting
   a dial within the host).
2. The plugin must swiftly act on these values and compute its functionality
   based on these input parameters. Given that a GUI is often integrated,
    synchronization with the audio engine's events is vital. Once all events are
   processed, these changes are communicated back to the GUI.

Failure to meet the callback's deadline, perhaps due to extended previous
processing, can lead to audio disruptions and glitches. Such inconsistencies
are detrimental to professional audio and must be diligently avoided.
Therefore, it's essential to prioritize punctuality and precision in all
aspects of audio work[@rtaudio]. Keeping in mind the simple but crucial reminder:

> Don't miss your Deadline!

$$
\frac{\text{AudioBufferSize}}{\text{SamplingFrequency}} = \text{CallbackFrequency} \\
\quad\text{, e.g.}\quad\frac{512}{48000\text{Hz}} = 10.67\text{ms}
$$ {#eq:rt_callback}

The equation above presents the formula for determining the minimum frequency
at which the processing function must supply audio samples to prevent glitches
and drop-outs. For 512 individual sampling points, at a sample rate of 48'000
samples per second, the callback frequency stands at **10.67ms**. Audio
block-size generally fluctuates between 128 - 2048 samples, with sampling rates
ranging from 48'000 - 192'000 Hz. Consequently, our callback algorithm must
operate within approximately **2.9ms - 46.4ms** for optimal
functionality[@audiolatency].

However, this is just the tip of the iceberg. The real challenge lies in
crafting algorithms and data structures that adhere to these specifications.
Drawing a comparison between the constraints of a realtime thread and a normal
thread brings this into sharper focus: In a realtime thread, responses must
meet specific deadlines, ensuring immediate and predictable behavior. In
contrast, a normal thread has more flexibility in its operation, allowing for
variable response times without the demanding need for timely execution.

|  Problems to Real-time  |     Real-time     |  Non-real-time  |
| ----------------------- |    -----------    | --------------- |
|        CPU work         |       **+**       |      **+**      |
|    Context switches     |   **+** (avoid)   |      **+**      |
|      Memory access      | **+** (non-paged) |      **+**      |
|      System calls       |       **x**       |      **+**      |
|       Allocations       |       **x**       |      **+**      |
|      Deallocations      |       **x**       |      **+**      |
|       Exceptions        |       **x**       |      **+**      |
|   Priority Inversion    |       **x**       |      **+**      |

Table: Realtime limitations. ^[Taken from: [ADC 2019, Fabian Renn-Giles & Dave Rowland - Real-time 101](https://www.youtube.com/watch?v=Q0vrQFyAdWI)]

The above table compares realtime to non-realtime requirements. In short we
can't use anything that has non-deterministic behavior. We want to know exactly
how long a specific instruction takes to create an overall system that provides
a deterministic behavior. *System calls* or calls into the operating system
kernel, are one of such non-deterministic behaviors. They also include
allocations and deallocations. None of those mechanisms can be used when we
deal with realtime requirements. This results in careful design decisions that
have to be taken when designing such systems. We have to foresee many aspects
of the architecture and use pre-allocated containers and structures to prevent
a non-deterministic behavior. For example to communicate with the main thread
non-blocking and wait free data structures as FIFO^[First In First Out] queues
or ring-buffers are used.

![Realtime ranking](images/realtime-scala.png){ width=77% #fig:rtranking }

*@fig:rtranking* classifies various realtime systems by their severity (Y-axis) and
the impact on overall design (X-axis). Realtime can be partitioned into three
categories: Soft-Realtime, Firm-Realtime, and Hard-Realtime. High-severity
lapses might have catastrophic consequences. For instance, in medical
monitoring systems or car braking systems, missing a deadline can be fatal,
leading to a literal **dead**line. Audio sits in the middle, where a missed
deadline compromises professional utility but doesn't pose dire threats. Video
game rendering bears even lesser severity, as occasional frame drops don't
render the product ineffectual and are relatively commonplace.

### 2.1.3 Standards and Hosts

Over time, various audio plugin standards have evolved, but only a select few
remain significant today. The table below provides an overview of some of the
most well-recognized standards:

|                                      Standard                                       |       Extended Name       |       Developer       |  File Extension   |      Supported OS      |  Initial Release  |           Licensing           |
|                                   --------------                                    |    -------------------    |      -----------      | ----------------  |    ---------------     | ----------------- |        ---------------        |
|                          [CLAP](https://cleveraudio.org/)                           |    Clever Audio Plugin    |     Bitwig & U-he     |       .clap       | Windows, MacOS & Linux |       2022        |              MIT              |
|    [VST/VST3](https://steinbergmedia.github.io/vst3_dev_portal/pages/index.html)    | Virtual Studio Technology |       Steinberg       | .dll, .vst, .vst3 | Windows, MacOS & Linux |       2017        | GPLv3, Steinberg Dual License |
|                      [AAX](https://apps.avid.com/aax-portal/)                       |   Avid Audio Extension    |   Pro Tools (Avid)    |       .aax        |    Windows & MacOS     |       2011        |       Approved Partner        |
| [AU](https://developer.apple.com/documentation/audiotoolbox/audio_unit_v3_plug-ins) |        Audio Units        |   Apple macOS & iOS   |        .AU        |         MacOS          |  "Cheetah" 2001   |   Custom License Agreement    |

Certain standards cater specifically to particular platforms or programs. For
instance, *Apple's AU* is seamlessly integrated with their core audio
SDK^[Software Development Kit] [@applecoreaudio]. Similarly, *Avid's AAX* is
designed exclusively for plugin compatibility with the Pro
Tools^[https://www.avid.com/pro-tools] DAW. On the other hand, standards like
the *VST3 SDK* are both platform and host independent, and it's currently
among the most popular plugin standards. Additionally, the newly introduced
*CLAP* standard is also gaining traction.

Most commonly, these plugins are hosted within **Digital Audio Workstations**.
These software applications facilitate tasks such as music production, podcast
recording, and creating custom game sound designs. Their usage spans a broad
spectrum, and numerous DAW manufacturers exist:

![plugin hosts](images/plugin-hosts.png)

However, DAWs are not the only plugin hosts. Often, plugin developers include a
*standalone* version that operates independently of other software. A recent
noteworthy development in this domain is the game industry's move towards these
plugins, exemplified by Unreal Engine's planned support for the **CLAP**
standard, as unveiled at Unreal Fest
2022^[https://www.youtube.com/watch?v=q9pFsI9Cq9c&t=621s].

