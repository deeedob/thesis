# Chapter 1: Introduction

Throughout this work, we will discuss plugins and their impact on the usability
of an application. Specifically, we will delve into the development of
graphical user interfaces (GUIs) for audio plugins, examining their influence
on user experiences and their relationship with development experiences. When
considering GUIs^[Graphical User Interfaces] broadly, it's natural to
contemplate their flexibility and stability. The sheer number of operating
systems, graphic backends, and platform-specific details is more than any
single developer could realistically address.

Investing time in learning and potentially mastering a skill naturally leads to
the desire to apply it across various use-cases. Opting for a library that has
*withstood the test of time* enhances stability, yet professionals often seek
continuity, preferring not to re-acquaint themselves with a topic merely because
API^[Application Programming Interface] of our chosen toolkit doesn't support
the targeted platform.

Therefore, Qt, a cross-platform framework for crafting GUIs, comes to mind when
contemplating the development of an audio plugin UI^[User Interface] intended
for widespread platform compatibility. The expertise gained from utilizing the
Qt framework is versatile, suitable for crafting mobile, desktop, or even
embedded applications without relearning syntax or structure. As one of Qt's
mottos aptly states:

> Code once, deploy everywhere.

The significance of this subject becomes evident when browsing the forum
"kvraudio.com", a renowned platform for audio-related discussions.

A brief search of:

> "Qt" "Plugin" :site www.kvraudio.com

uncovers 57'800 results, with 580 from the span between 10/19/2022 and
10/19/2023. While the weight of such figures may be debated, they undeniably
highlight the relevance and potential of Qt as a viable choice for audio plugin
development.

## 1.1 Background

### 1.1.1 Plugins: Shared Libraries

When discussing plugins written in a compiled language, we typically refer to
them as shared libraries. A shared library, also known as a dynamic library or
DSO^[Dynamic Shared Object], is a reusable object that exports a table of
symbols (e.g., functions, variables, global data). These libraries are loaded
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

Shared libraries can be further categorized into:

- *Dynamically linked libraries* - The library is linked against the
  application after the compilation. The kernel then loads the library, if not
  already in shared memory, automatically upon execution.
- *Dynamically loaded libraries* - The application takes full control by
  loading libraries manually with tools like
  [dlopen](https://linux.die.net/man/3/dlopen) or
  [QLibrary](https://doc.qt.io/qt-6/qlibrary.html).

For audio plugins, the latter method is employed to load plugin instances. The
interface defined by a plugin standard can be viewed more as a communication
layer, resonating more with a request-response paradigm than the conventional
utility functions of linked libraries.

Considering the foundational role of shared libraries in every plugin, it's
beneficial to delve deeper into their intricacies. Let's explore a basic
example:

```cpp
!include examples/simplelib.cpp
```

This code defines a minimal shared library. After including the required
standard-headers, we define a compile time directive that is used to signal the
visibillity of the exported symbols. Windows and Unix based system differ here.
On Windows with MSVC the symbols are _not_ exported by default, and require
explicit marking with `__declspec(dllexport)`. On Unix based system we use the
visibillity attribute. Since by default all symbols are exported on these
platforms, rendering this attribute seemingly redundant, it remains
advantageous to maintain clarity. This would also allows us to control the
visibillity in the linking step by simply using the linker flag
`-fvisibility=hidden` to hide all symbols.

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
!include examples/build_and_run.sh
```

And finally run our script:

```bash
./build_and_run
simplelib: called from simplelib.cpp:18
```

###  1.1.2 Plugins: Overview

**Plug**-***ins***, at their essence, serve as dynamic extensions, enhancing
the capabilities of a plugin-loading host. One can perceive them as *on-demand*
functionalities ready for deployment. Their prevalence is evident across both
the software and hardware domains. For instance, they can be specialized
filters added to image processing applications like *Adobe Photoshop*, dynamic
driver modules integrated into operating systems such as *GNU/Linux*
[@cppn2074], or system-specific extensions found in frameworks like *Qt*.

![basic plugin architecture](images/plugin-basic.png)

*Figure 1* offers a visual representation of this mechanism. On the left, we
have the host, which is equipped with the capability to accommodate the plugin
interface. Centrally located is the plugin interface itself, serving as the
communication bridge between the host and the plugin. The right side showcases
the actual implementation of the plugin. Once the shared object is loaded
successfully by the host, it actively *requests* the necessary functionalities
from the plugin as and when required. This interaction entails invoking
specific functions and subsequently taking actions based on the results. While
plugins can, on occasion, prompt functions from the host, their primary role is
to respond to the host's requests. The foundation of this interaction is the
meticulously designed plugin interface that facilitates this bilateral
communication.

A concrete example of such an interface can be gleaned from the **CL**ever
**A**udio **P**lugin (CLAP) format. This standard dictates the communication
protocols between a **D**igital **A**udio **W**orkstation (DAW) and its various
plugins, be it synthesizers, audio effects, or other elements. A segment from
the C-API reads:

```cpp
   // Call start processing before processing.
   // [audio-thread & active_state & !processing_state]
   bool(CLAP_ABI *start_processing)(const struct clap_plugin *plugin);
```

To unpack this, the function outlines the calling convention for a function
pointer named `start_processing`. This pointer returns a `bool` and receives a
constant pointer to the struct `clap_plugin` as an argument. Embedded within this
is the preprocessor macro CLAP_ABI, defined as:

```cpp
#if !defined(CLAP_ABI)
#   if defined _WIN32 || defined __CYGWIN__
#      define CLAP_ABI __cdecl
#   else
#      define CLAP_ABI
#   endif
#endif
```

For Windows and similar platforms, the attribute `__cdecl` is adopted, ensuring
adherence to the C-style function calling convention. On other platforms, this
macro is unassigned. This implementation detail only becomes pertinent for developers
intent on designing theor own plugin interface.

Throughout our discussions, we'll frequently reference the terms *ABI* and
*API*. To ensure clarity, let's demystify these terms. The **API**, an acronym
for **A**pplication **P**rogramming **I**nterface, serves as a blueprint that
dictates how different software components should interact. Essentially, it
acts as an agreement, ensuring that software pieces work harmoniously together.
If we think of software as a puzzle, the API helps ensure that the pieces fit
together.

On the flip side, we have the **ABI** or **A**pplication **B**inary
**I**nterface, which pertains to the actual executable binary. While the API
sets the communication norms, the ABI ensures they're executed accurately.

As software undergoes iterative development—evolving from version 1.1 to 1.2,
and then to 1.3, and so forth—maintaining a consistent interface becomes
paramount. Ideally, software designed using version 1.3 of an interface should
seamlessly operate with version 1.1. This seamless integration without
necessitating recompilation or additional adjustments is known as ***Binary
Compatibility***. It ensures that different versions of software libraries can
coexist without conflict. Notably, while binary compatibility emphasizes the
consistent presence of binary components, it doesn't imply rigidity in the API.

Beyond this, there's the concept of ***Source Compatibility***. This pertains
to preserving the integrity of the API. Achieving this form of compatibility
demands meticulous planning and entails upholding a uniform API, precluding
drastic alterations like function renaming or behavioral modifications.

###  1.1.3 Audio Plugins: Structure and Realtime

In exploring the realm of audio plugins, two primary components emerge in their
structure: a realtime audio section and a controlling section.

![basic audio plugin architecture](images/audio-plugin-basic.png)

*Figure 2* illustrates a structure closely resembling *Figure 1*. The plugin
interface now features two distinct colors to aid differentiation. The green
section pertains to the host's interaction with the plugin through a
low-priority main thread. This primarily handles tasks such as GUI setup and
other control-related functions. "Low priority" implies that it's permissible
to conduct non-deterministic operations which may momentarily halt progress.

The red segment signifies that the API is being invoked by a high-priority
realtime thread. These functions are called frequently and demand minimal
latency to promptly respond to the host. They process all incoming events from
the host, including parameter changes and other vital events essential for
audio processing. When processing audio, the plugin integrates all necessary
alterations and executes the specified function, as demonstrated by a gain
plugin; A typical gain modifies the output volume and possesses a single
parameter: the desired gain in decibels to amplify or diminish the output
audio.

![realtime overview[@lowlatency]](images/realtime-overview.png){ width=77% }

A challenge in audio programming stems from the rapidity with which the audio
callback needs a response. The standard structure unfolds as:

1. The host routinely invokes a process function, forwarding the audio input
   buffer as an argument, along with any events related to the audio samples
   (such as a user adjusting a dial within the host).
2. The plugin must swiftly act on this buffer and relay the modified audio
   buffer back to the host. Given that a GUI is often integrated,
   synchronization with the audio engine's events is vital. Once all audio
   samples and events are processed, these changes are communicated back to the
   GUI.

Failure to meet the callback's deadline, perhaps due to extended previous
processing, can lead to audio disruptions and glitches. Such inconsistencies
are detrimental to professional audio and must be diligently avoided. Hence, a
known saying is:

> Don't miss your Deadline!

![realtime callback frequency](images/realtime-callback.png){ width=66% }

*Figure 4* presents the formula for determining the minimum frequency at which
the processing function must supply audio samples to prevent glitches and
drop-outs. For 512 individual sampling points, at a sample rate of 48,000
samples per second, the callback frequency stands at **10.67ms**. Audio's
block-size generally fluctuates between 128 - 2048 samples, with sampling rates
ranging from 48,000 - 192,000 Hz. Consequently, our callback frequency must
operate within approximately **2.9ms - 46.4ms** for optimal functionality.

This, however, merely scratches the surface. The intricacy lies in devising
algorithms and data structures that align with these specifications. Comparing
the constraints of a realtime thread with a standard thread bereft of such
requirements elucidates this: #TODO:

![realtime limitations[@realtime101]](images/realtime-problems.png)

*Figure 5* compares realtime to non-realtime requirements. In short we can't
use anything that has non-deterministic behavior. We want to know exactly how
long a specific instruction takes to create an overall structure that provides
a deterministic behavior. *System calls* or calls into the operating system
kernel, are one of such non-deterministic behaviors. They also include
allocations and deallocations. None of those mechanisms can be used when we
deal with realtime requirements. This results in careful design decisions that
have to be taken when designing such systems. We have to foresee many aspects
of the architecture and use pre-allocated containers and structures to prevent
a non-deterministic behavior. For example to communicate with the main thread
of the application we use non-blocking and wait free data structures as
FIFO's^[First In First Out] or ring-buffers to complement this.

![realtime ranking[@realtime101]](images/realtime-scala.png)

*Figure 6* classifies various realtime systems by their severity (Y-axis) and
the impact on overall design (X-axis). Realtime can be partitioned into three
categories: Soft-Realtime, Firm-Realtime, and Hard-Realtime. High-severity
lapses might have catastrophic consequences. For instance, in medical
monitoring systems or car braking systems, missing a deadline can be fatal,
leading to a literal **dead**line. Audio sits in the middle, where a missed
deadline compromises professional utility but doesn't pose dire threats. Video
game rendering bears even lesser severity, as occasional frame drops don't
render the product ineffectual and are relatively commonplace.

### 1.1.4 Audio Plugins: Standards and Hosts

Over time, various audio plugin standards have evolved, but only a select few
remain significant today. The table below provides an overview of some of the
most well-recognized standards:

| Standard | Extended Name             | Developer        | File Extension       | Supported OS                       | Initial Release | Licensing                                |
|----------------|---------------------------|-------------------|-----------------------|------------------------------------|-----------------|------------------------------------------|
| [CLAP](https://cleveraudio.org/)     | Clever Audio Plugin       | Bitwig & U-he     | .clap                | Windows, MacOS & Linux            | 2022            | MIT                                      |
| [VST/VST3](https://steinbergmedia.github.io/vst3_dev_portal/pages/index.html) | Virtual Studio Technology | Steinberg         | .dll, .vst, .vst3    | Windows, MacOS & Linux            | 2017            | GPLv3, Steinberg Dual License (Email)  |
| [AAX](https://apps.avid.com/aax-portal/)      | Avid Audio Extension      | Pro Tools (Avid) | .aax                 | Windows & MacOS                   | 2011            | Approved Partner (Email)                 |
| [AU](https://developer.apple.com/documentation/audiotoolbox/audio_unit_v3_plug-ins)       | Audio Units               | Apple macOS & iOS| .AU                  | MacOS                             | "Cheetah" 2001  | Custom License Agreement                 |

Certain standards cater specifically to particular platforms or programs. For
instance, Apple's **AU** is seamlessly integrated with their core audio
SDK^[Software Development Kit] [@applecoreaudio]. Similarly, Avid's **AAX** is
designed exclusively for plugin compatibility with the Pro Tools DAW^[Digital
Audio Workstation]. On the other hand, standards like the **VST3** SDK are both
platform and program independent, and it's currently among the most popular
plugin standards. Additionally, the newly introduced **CLAP** is also gaining
traction.

Most commonly, these plugins are hosted within **Digital Audio Workstations
(DAWs)**. These software applications facilitate tasks such as music
production, podcast recording, and creating custom game sound designs. Their
applicability spans a broad spectrum, and numerous DAW manufacturers exist:

![plugin hosts](images/plugin-hosts.png)

However, DAWs are not the only plugin hosts. Often, plugin developers include a
*standalone* version that operates independently of other software. A recent
noteworthy development in this domain is the game industry's move towards these
plugins, exemplified by Unreal Engine's forthcoming support for the **CLAP**
standard, as unveiled at *Unreal Fest 2022*[@ueaudio].

## 1.2 Problem Statement

A primary challenge in integrating Qt user interfaces within restricted audio
plugin environments hinges on reentrancy. A program or function is considered
re-entrant if it can safely support concurrent invocations, meaning it can be
"re-entered" within the same process. Such behavior is vital as audio plugins
often instantiate multiple times from the loaded shared libraries. To
illustrate this challenge, consider the following example:

```cpp
!include examples/reentrancy.cpp
```

In C++, static objects and variables adhere to the [static storage
duration](https://en.cppreference.com/w/c/language/static_storage_duration):

> The storage for the object is allocated when the program starts and
> deallocated when it concludes. Only a single instance of the object exists.

The crux here is the singular instance of the object per application process.
This necessitates caution when working with static types where reentrancy is
essential. When the above program is executed, the outcome is:

```bash
# Compile and run the program
g++ -o build/reentrancy reentrancy.cpp -Wall -Wextra -pedantic-errors \
;./build/reentrancy

First call:
Non-Reentrant: 0 1 2
Reentrant: 0 1 2

Second call:
Non-Reentrant:
Reentrant: 0 1 2
```

While the initial invocation for both functions is successful, re-entering the
function during the second call yields no output for the non-reentrant
function. This outcome stems from the use of the static specifier in the for
loop counter, causing the variable to not meet the `i < 3` condition during the
second entry. This example serves to emphasize the interplay between *static
storage duration* and function reentrancy.

While static objects facilitate global accessibility and can foster improved
application design, challenges arise. For instance, the QApplication variants
(e.g., QCoreApplication, QGuiApplication), housing the Qt event loop initiated
via `Q*Application::exec()`, are static:

```c++
// qtbase/src/corelib/kernel/qcoreapplication.h
static QCoreApplication *instance() noexcept { return self; }
```

This implies that only one QApplication can exist per process. Challenges arise
if a plugin-loading-host, like [QTractor
does](https://github.com/rncbc/qtractor/blob/0e987e6c41796a4cbe85e499ae890b5989be8b82/src/qtractor.h#L60),
employs a QApplication object or when multiple plugin instances load within a
single process.

Furthermore, event loop's blocking nature means that `Q*Application::exec()`
only returns post-execution. Yet, the plugin standards in discussion
necessitate return capabilities.

Although there are various workarounds, like compiling Qt under a separate
namespace and initiating the event loop in an isolated thread, these often fall
short. Such methods either introduce undue complexity, as seen with namespace
compilation for their users, or induce instability, as with separate thread
initiation.

## 1.3 Objectives

This study endeavors to pave the way for innovative techniques that allow
seamless integration of graphical user interfaces developed with the Qt
framework into an audio plugin standard. Central to this goal is enabling the
initiation of these GUIs directly from a host, while ensuring efficient and
responsive communication between the two entities (host <-> GUI). The overall
user experience is pivotal; hence, the development process of these Qt GUIs
should not only feel native but also be intuitive. This implies that events
triggered by the host should effortlessly weave into Qt's event system.
Additionally, these events should be designed to offer signal & slot
mechanisms, ensuring they are readily available for utilization within various
UI components, streamlining development and promoting a more organic
interaction between components.

## 1.4 Scope and Limitations

The cornerstone of this investigation is the **CLAP** plugin standard.
Recognized for its innovative capabilities and being at the forefront of
current technologies, it stands as the most promising contender for such
integration tasks. The primary ambition of this work revolves around bridging
the gap between the two distinct realms of audio plugins and Qt GUIs. However,
it's imperative to note that this research doesn't aim to deliver a universal,
cross-platform solution. The focus remains steadfast on the Linux platform,
ensuring compatibility and support for both X11 and Wayland protocols. By
confining the research to this scope, the study seeks to delve deeper into the
intricacies and nuances of the integration process, ensuring a robust and
effective methodology.

