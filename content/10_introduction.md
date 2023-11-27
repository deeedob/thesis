# Chapter 1: Introduction

## 1.1 Motivation

Throughout this work, we will discuss plugins and their impact on the usability
of an application. Specifically, we will focus on the development of graphical
user interfaces (GUIs) for audio plugins, examining their influence on user
experiences and their relationship with development workflow. When considering
GUIs broadly, it's natural to contemplate their flexibility and stability. The
sheer number of operating systems, graphic backends, and platform-specific
details is more than any single developer could realistically address.

Investing time in learning and potentially mastering a skill naturally leads to
the desire to apply it across various use-cases. Opting for a library that has
*withstood the test of time* enhances stability, yet professionals often seek
continuity, preferring not to re-acquaint themselves with a subject solely due
to the limitations of their chosen toolkit for the next project.

Therefore, Qt, a cross-platform framework for crafting GUIs, comes to mind when
considering the development of an audio plugin User Interface (UI hereon)
intended for widespread platform compatibility. The expertise gained from
utilizing the Qt framework is versatile, suitable for crafting mobile, desktop,
or even embedded applications without relearning syntax or structure. As one of
Qt's mottos aptly states:

> Code once, deploy everywhere.

The significance of this subject becomes evident when browsing "kvraudio", a
renowned platform for audio-related discussions.

A brief search of: `"Qt" "Plugin" :site www.kvraudio.com`

uncovers *57'800* results, with *580* from the span between *10/19/2022* and
*10/19/2023*. While the weight of such figures may be debated, they certainly
suggest the relevance and potential of Qt as a feasible option for audio plugin
development.

## 1.2 Objective

A primary challenge in integrating Qt user interfaces within audio plugin
environments centers on reentrancy. A program or function is considered
re-entrant if it can safely support concurrent invocations, meaning it can be
"re-entered" within the same process. Such behavior is vital as audio plugins
often instantiate multiple times from within itself. To illustrate this
challenge, consider the following example:

```cpp
!include examples/reentrancy.cpp
```

In C++, static objects and variables adhere to the [static storage
duration](https://en.cppreference.com/w/c/language/static_storage_duration):

> The storage for the object is allocated when the program starts and
> deallocated when it concludes. Only a single instance of the object exists.

The problem here is the single instance of the object per application process.
This necessitates caution when working with static types where reentrancy is
essential. When the above program is executed, the outcome is:

```bash
# Compile and run the program
g++ -o build/reentrancy reentrancy.cpp -Wall -Wextra -pedantic-errors \
;./build/reentrancy
Reentrant Call 1, Plugin 1 created.
Non-Reentrant Call 1, Plugin 1 created.

Reentrant Call 2, Plugin 2 created.
Non-Reentrant Call 2, ⏎
```

In the initial invocation of both functions, the creation of the `Plugin`
object is successful and behaves as expected. However, during the second call,
the `create_non_reentrant()` function does not produce a new object. This
behavior occurs because of the static specifier in the `create_non_reentrant()`
function. The static `Plugin` object retains its initialized value from the
first call, hence not creating a new instance or outputting any message in the
subsequent invocation. This demonstrates the non-reentrant nature of static
objects, as they maintain their state and do not reset between function calls,
presenting challenges in environments where independent instances are required
for each function call.

Static objects offer global accessibility and can enhance application design by
allowing for the initialization of crucial objects just once, with the
capability to share them throughout the entire codebase. However, this approach
has its trade-offs, especially in terms of integration and operation in
multi-threaded environments.

This issue is particularly evident in the case of `QApplication` variants like
`QCoreApplication` and `QGuiApplication`. These classes, which manage Qt's
event loop through `Q*Application::exec()`, employ the singleton design
pattern[@gof]. The singleton pattern ensures that a class has only one instance and
provides a global point of access to it, as demonstrated in the Qt framework:

```c++
// qtbase/src/corelib/kernel/qcoreapplication.h
static QCoreApplication *instance() noexcept { return self; }
~~~
#define qApp QCoreApplication::instance()
```

This design choice means only one `QApplication` can exist within a process.
Issues arise when a plugin-loading-host, as
QTractor^[https://github.com/rncbc/qtractor/blob/master/src/qtractor.h#L60],
already utilizes a `QApplication` object or when multiple plugin instances
operate within a single process. At first glance, one might assume the ability
to verify the presence of a `QApplication` within the process and then allowing
it to automatically integrate with the event loop of a receiving window. This
approach can be illustrated as follows:

```c++
~~~
    if (!qGuiApp) {
        static int argc = 1;
        static char empty[] = ""; static char *argv[] = { &empty; }
        new QGuiApplication(argc, argv); // We don't call exec!
    }
~~~
// Set our 'QWindow *window' to the received window from the host.
window->setParent(QWindow::fromWinId(WId(hostWindow)));
```

Attempting to reuse the event system of a parent window, while occasionally
effective, is fraught with uncertainties. A primary limitation is that this
approach is not consistently supported across different platforms. For instance
on Linux, where event systems such as **xcb**^[https://xcb.freedesktop.org/]
and **glib**^[https://docs.gtk.org/glib/] are not standardized would render
this method impractical. Additionally, there are risks associated with
connecting to an event loop from an outdated version, which could lead to
compatibility issues and impaired functionality.

Another concern is the management of multiple instances attempting to connect
to the same event loop, raising doubts about the reliability and efficiency.
Consequently, this approach is not a dependable solution for the challenges
faced in integrating Qt user interfaces with audio plugin environments.

This research is focused on developing innovative methods to integrate Qt-based
graphical user interfaces seamlessly into the audio plugin landscape. The main
objective is to provide seamless integration with audio plugin standards,
ensuring effective and responsible communication. A crucial aspect of this
integration is to enhance development experience by providing a stable method
that works across all major operating systems.  An important part of achieving
this is the seamless integration of events generated by the plugin into Qt's
event system. This strategy is designed to offer a development process that is
inherently aligned with Qt's principles, making it more intuitive for
developers and ensuring a robust, platform-independent solution.

The research primarily focuses on the CLAP^[https://cleveraudio.org/] plugin
standard. CLAP is chosen for its innovative capabilities and its relevance in
the context of current technology trends. This standard is viewed as the most
suitable for the intended integration tasks. The usage of CLAP emphasizes the
study's aim to tackle the intricate challenges of integrating Qt's
comprehensive GUI framework with the dynamic field of audio plugins.
