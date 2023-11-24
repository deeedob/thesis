# Chapter 3: Related Work

This section examines existing research and current advancements in the field
of audio plugin development. Given the focused scope of this thesis, attention
is centered on the most relevant aspects, particularly the use of inter-process
communication in the remote control of plugins.

## Clap-Plugins

The project most directly related to this thesis comes from the CLAP team. In
the official repository of the *free-audio* organization, which is the home of
the CLAP standard, there is a project named
[clap-plugins](https://github.com/free-audio/clap-plugins). This project serves
as a hands-on example of how to implement a CLAP plugin. The plugins in this
project use Qt for their GUIs. Alexandre Bique, the project's maintainer and
the lead developer of CLAP, has recognized the challenges of integrating Qt
GUIs with audio plugins and has offered solutions to these challenges. The first
suggestion is to avoid dynamically linking any dependencies to the plugin, to
keep the plugin self-contained. Following this, the project presents two
different strategies for integrating Qt with CLAP:

>   We can use two different strategies:
>
>       local: statically link everything
>       remote: launch the GUI in a separate child process

The *local* method involves building a custom version of Qt in a separate
namespace, which is achieved by using the compile flag
`QT_NAMESPACE=<uniqueId>`. This step is crucial to prevent conflicts when
multiple Qt applications run in the same process and namespace. The *remote*
method involves starting the GUI in a separate process and communicating with
it via inter-process communication techniques.

The initial approach of using a separate namespace for Qt still presents
challenges, particularly on Linux systems. Even with Qt compiled under a unique
namespace, issues arise when multiple instances of the event loop are running
with the [glib event loop](https://docs.gtk.org/glib/main-loop.html), which is
the default on Linux desktops. Loading a plugin in this manner, even with a
static Qt build under a unique namespace, into an environment where Qt is
already running a glib event loop, might result in an error like this:

```bash
GLib-CRITICAL **: 00:25:07.633: g_main_context_push_thread_default: assertion 'acquired_context' failed
```

The definitive explanation for this error remains unclear. The problem might
stem from glib retaining some internal state, or perhaps there's a namespace
mismatch occurring. Whether intentionally or not, the *clap-plugins*
implementation explicitly employs the *xcb* backend:

```cpp
// plugins/gui/local-gui/factory.cc
#ifdef Q_OS_LINUX
   ::setenv("QT_QPA_PLATFORM", "xcb", 1);
#endif
```

This solution bypasses the problem, but at the expense of not using the
default event-system, which is generally more robust and well-tested.
Additionally, this method necessitates manually building Qt in a separate and
unique namespace for each plugin, adding complexity to the development process.

For the *remote* strategy, the implementation utilizes a custom-built IPC
mechanism. On POSIX systems, it employs a [unix domain
sockets](https://de.wikipedia.org/wiki/Unix_Domain_Socket) approach, and on
Windows, it uses [I/O completion ports](https://tinyclouds.org/iocp_links#iocp)
(IOCP). Events initiated by the host are eventually transmitted to the GUI
process:

```cpp
// plugins/gui/remote-gui-proxy.cc

   bool RemoteGuiProxy::show() {
      bool sent = false;
      messages::ShowRequest request;

      _clientFactory.exec(
         [&] { sent = _clientFactory._channel->sendRequestAsync(_clientId, request); });

      return sent;
   }
```

This function is responsible for dispatching the event that triggers the
GUI window's visibillity. The events, initiated by the host application, are received
and processed by the GUI process through a switch statement that handles
all the various events:

```{.cpp .numberLines}
// plugins/gui/gui.cc
~~~
      case messages::kShowRequest: {
         messages::ShowRequest rq;
         messages::ShowResponse rp;
         msg.get(rq);
         if (c)
            rp.succeed = c->show();
         _channel->sendResponseAsync(msg, rp);
         break;
      }
~~~
```

In this code, *line 5* sees the incoming message extracted from the
communication channel and stored in `rp`. If the GUI component is operational,
it triggers the `show()` function, which in turn interfaces with the
[QQuickView](https://doc.qt.io/qt-6/qquickview.html) of the user interface.
This process effectively ensures the display of the GUI window in response to
the host application's event.

In this segment of code, the message is retrieved from the communication
channel on *line 5* and copied into `rp`. If the GUI is available, the `show()`
method is executed. This call links to the
[QQuickView](https://doc.qt.io/qt-6/qquickview.html) of the user interface,
ensuring that the GUI window is displayed in response to the event from the
host application.

## Sushi

Another project employing a technical specification akin to the one detailed in
this research is [Sushi](https://github.com/elk-audio/sushi). Its description
reads:

> Headless plugin host for ELK Audio OS.

Elk Audio OS is a Linux-based operating system specifically developed for
real-time audio applications. Its primary focus is on embedded hardware within
the "Internet of Musical Things" (IoMusT) sector. This OS is available as an
open-source version for the *Raspberry Pi 4 Single Board Computer*, and it is
also commercially offered for additional SOCs^[System-On-Chip] like the
STM32MP1, or Intel x86
platforms.^[https://elk-audio.github.io/elk-docs/html/documents/supported_hw.html]

Sushi serves as a headless *Digital Audio Workstation* that establishes the
fundamental communication with the Elk Audio OS. It functions as [headless
software](https://en.wikipedia.org/wiki/Headless_software), characterized by
its design to operate without a GUI frontend - the 'missing head'.

Sushi is equipped with multiple protocols and technologies for interaction and
control, including MIDI, OSC^[Open Sound Control], and
gRPC^[https://www.elk.audio/articles/controlling-plug-ins-in-elk-part-ii].

> As SUSHI is a headless host, intended for use in an embedded device, it does
> not feature a graphical user interface. In its place is a gRPC interface that
> can be used for controlling all aspects of SUSHI and hosted plugins[@elk_audio_os]

Additionally, Sushi integrates with the VST audio plugin
standard^[https://elk-audio.github.io/elk-docs/html/documents/building_plugins_for_elk.html#vst-2-x-plugins-using-steinberg-sdk].
This allows for the remote control of such plugins through its gRPC interface.
A segment of the protobuf interface can be seen below:

```proto
service NotificationController
{
    rpc SubscribeToTransportChanges (GenericVoidValue) returns (stream TransportUpdate) {}
    rpc SubscribeToParameterUpdates (ParameterNotificationBlocklist) returns (stream ParameterUpdate) {}
    rpc SubscribeToPropertyUpdates (PropertyNotificationBlocklist) returns (stream PropertyValue) {}
    ~~~
}
```

Sushi provides various services to remotely manage the application or to
monitor changes in the SUSHI application. Leveraging gRPC, this interface can
be implemented in various languages supported by gRPC and protobuf. Elk Audio
also offers two ready-made client implementations, one in
C++^[https://github.com/elk-audio/elkcpp] and another in
Python^[https://github.com/elk-audio/elkpy].

