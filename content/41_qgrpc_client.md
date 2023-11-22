## 3.3 QtGrpc Client Implementation

In this section we will use QtGrpc to provide a client implementation for the
`ClapInterface` service shown in the [previous section](#server_api). We will
also utilize the provided server-library to implement some example audio
plugins. The main goal of this client library, is to ease the development of
audio plugins and provide a simple interface to their users to both creating
the DSP^[Digital Signal Processing] section of the plugin, which is provided by
the server library and the UI^[User Interface] section of the plugin, which is
implemented with this library. Its responsibillity is to handle the event
stream coming from the audio plugin, integrate those events into Qts event loop
and with that provide the well-known signals & slots behavior, so that graphic
components can greatly benefit from that and handle their representation independently.

For now, we have primarily focused on enabling this library to work with Qts
QML and QtQuick modules. Please note that the term *library* is used here
but the actual implementation aligns more with an extended sandbox to
experiment with the implementation.


### Client Library: Core Concepts

In the projects build file, we add the server library as a dependency and
utilize the `api.proto` file to generate the required C++ client code. The current
implementation devides into two parts. We have the `clapinterface` section,
which operates in the background. It receives and sends events to the server.
It provides the core components to integrate those received messages into Qt.
The `clapcontrols` section provides the actual UI components. Those are Qml
components that are pre-configured with the required properties and signals to
handle and visualize the received events. The `src/` sirectory contains the
the following structure:

```bash
qtclapinterface/src (master)
❯ tree -L 2
.                            ├── clapcontrols
├── CMakeLists.txt           │   ├── clapdial.cpp
├── 3rdparty                 │   ├── clapdial.h
│   ├── clap-remote          │   ├── ClapDial.qml
│   └── toml11               │   ├── ClapKeys.qml
├── clapinterface            │   ├── ClapMenuBar.qml
│   ├── CMakeLists.txt       │   ├── ClapMenu.qml
│   ├── qclapinterface.cpp   │   ├── ClapWindow.qml
│   ├── qclapinterface.h     │   ├── CMakeLists.txt
│   ├── qnote.cpp            │   ├── fonts
│   ├── qnote.h              │   ├── images
│   ├── qnotehandler.cpp     │   ├── qtclapstyle.cpp
│   └── qnotehandler.h       │   ├── qtclapstyle.h
```

In the cmake file, we will add server the library as a dependency and generate
the required client code by utilizing QtGrpc's protobuf plugin:

```cmake
# src/CMakeLists.txt
add_subdirectory(3rdparty/clap-remote)
set(proto_file "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/clap-remote/api/v0/api.proto")
add_subdirectory(clapinterface)

~~~

# src/clapinterface/CMakeLists.txt
# Generate the protobuf and grpc files
qt_add_protobuf(ClapMessages QML
    QML_URI "Clap.Messages"
    PROTO_FILES ${proto_file}
)

qt_add_grpc(ClapMessages CLIENT
    PROTO_FILES ${proto_file}
)
```

`qt_add_protobuf` and `qt_add_grpc` are convenient cmake-functions provided by QtGrpc
to do jus that. We additionally indicate that the `protoc` plugin should provide
`QML` support to the generated code, so that we can interact with the protobuf
messages from within QML code. We then specify the *QML module* that can be linked
by client applications for use in their C++ or QML code:

```cmake
add_library(ClapInterface STATIC)
qt_add_qml_module(ClapInterface
    URI "Clap.Interface"
    VERSION ${CMAKE_PROJECT_VERSION}
    CLASS_NAME ClapInterfacePlugin
    PLUGIN_TARGET clapinterfaceplugin
    SOURCES
        "qclapinterface.h"
        "qclapinterface.cpp"
        "qnotehandler.h"
        "qnotehandler.cpp"
        "qnote.h"
        "qnote.cpp"
    OUTPUT_DIRECTORY ${QML_OUTPUT_DIR_INTERFACE}
    IMPORTS Clap.Messages
)
```

This will generate the module `Clap.Interface` which will later
be registered with the QML engine. We specify `ClapInterface` to be
a static library, to reduce the amount of shared-library dependencies.
The following files will be generated when compiling this module:

```bash
❯ tree -L 2 cmake-build/Clap/
Clap/
└── Interface
    ├── ClapInterface_qml_module_dir_map.qrc
    ├── ClapInterface.qmltypes
    ├── libclapinterfaceplugin.a
    └── qmldir
```

Users of this library will link against the `libclapinterfaceplugin.a` library,
and the QML engine will load this directory when the `Clap.Interface` module is
used in the QML code. We will now inspect the `QClapInterface` class.

```cpp
// src/clapinterface/qclapinterface.h
~~~
#include "api.qpb.h"
#include "api_client.grpc.qpb.h"
using namespace api::v0;
~~~
```

First, it includes the generated protobuf and grpc files. Those generated files
are the foundation of every QtGrpc client implementation. They include
a representation of the servers proto api, which are tailored for the use
inside Qts ecosystem.

```cpp
// src/clapinterface/qclapinterface.h
~~~
class QClapInterface : public QObject
{
    Q_OBJECT
    QML_SINGLETON
    QML_NAMED_ELEMENT(ClapInterface)
    QML_UNCREATABLE("QClapInterface is a singleton")

    Q_PROPERTY(PluginState state READ state NOTIFY stateChanged FINAL)
    Q_PROPERTY(bool visible READ visible WRITE setVisible NOTIFY visibleChanged)
    Q_PROPERTY(QWindow* transientParent READ transientParent NOTIFY transientParentChanged FINAL)
~~~
```

We then declare the `QClapInterface`. We mark it as a QML_SINGLETON, which means
there can only be a single instance of this class inside the QML engine. This is because
we will handle all incoming and outgoing messages centrally with this one class. There is
no need for multiple instances. Then we declare some properties with the `Q_PROPERTY` macro.
This enables us to use those properties from within QML code. This class currently only provides
crucial properties, as to hide or show the UI, when requested by the host. Or the window,
in which we have to re-parent ourselves, when requested by the host.

To begin the communication with the server, we need to create a `QGrpcChannel`. This is
handled inside the `connect` function and is currently required to be called by the users
of this library:

```cpp
// src/clapinterface/qclapinterface.cpp
void QClapInterface::connect(const QString &url, const QString &hash)
{
    // Provide the metadata to the server
    QGrpcChannelOptions channelOptions(url);
    metadata = {
        { QByteArray(Metadata::PluginHashId.data()), { hash.toUtf8() } },
    };
    channelOptions.withMetadata(metadata);

    // Create a Http2 channel for the communication
    auto channel = std::make_shared<QGrpcHttp2Channel>(channelOptions);
    client->attachChannel(channel);
    // Start the server side stream
    stream = client->streamServerEventStream(ClientRequest(), {});
    ~~~
}
```

We start the executable by providing the `url` and `hash` as arguments on the
server side. Those arguments should then be used to call this function. We add
the hash-id to the channel, so that the server can identify the plugin instance
the client want to attach to. The `client` type utilizes the generated qtgrpc
code to access the defined service by the server:

```cpp
    // src/clapinterface/qclapinterface.h
    std::unique_ptr<ClapInterface::Client> client = {};
```

By inspecting the generated code, we can see that the `Client` type is
reflecting the defined service by the server:


```cpp
// build/api_client.grpc.qpb.h
class QPB_CLAPMESSAGES_EXPORT Client : public QAbstractGrpcClient
{
    Q_OBJECT
public:
    explicit Client(QObject *parent = nullptr);
    std::shared_ptr<QGrpcServerStream> streamServerEventStream(~~~);

    QGrpcStatus ClientEventCall(~~~);
    Q_INVOKABLE void ClientEventCall(~~~);

    QGrpcStatus ClientParamCall(~~~);
    Q_INVOKABLE void ClientParamCall(~~~);
~~~
```

Here we can find the three defined service calls. We can see that the generated
code is already providing the crucial integration into Qts ecosystem with the
`Q_INVOKABLE` macro. All other generated messages provide some convenience
integration into Qts meta system, so that we can utilize those messages with
ease to integrate the external grpc system, which could be written in any
of the supported gRPC languages. To finish the `connect` function, we need
to connect to the server stream and handle the incoming messages:

```cpp
// src/clapinterface/qclapinterface.cpp
// void QClapInterface::connect(~~~) {
~~~
    QObject::connect(stream.get(), &QGrpcServerStream::errorOccurred, this,
        [](const QGrpcStatus &status) { QGuiApplication::quit(); }
    );

    QObject::connect(stream.get(), &QGrpcServerStream::finished, this,
         []() { QGuiApplication::quit(); }
    );

    QObject::connect(stream.get(), &QGrpcServerStream::messageReceived, this,
        [this]() { processEvents(stream->read<ServerEvents>()); }
    );
    callbackTimer.start();
~~~
```

For now, we simply connect the `errorOccurred` and `finished` signals to Quit
the application. The crucial informations are handled inside the
`processEvents` callback and will distribute the received events accordingly.
Lastly, we start the `callbackTimer`. We utilize a similar appraoch as the one
used in the server library. The callback operates at a fixed frequency, to
bundle all outgoing events into a single message. Since, at the time of
writing, only grpc calls where provided, this was to reduce the amount of unary
calls to the server to increase performance. Ideally, bidirectional streaming
would be used here to reduce the amount of independent RPC calls.

```cpp
// src/clapinterface/qclapinterface.cpp
void QClapInterface::processEvents(const ServerEvents &events)
{
    for (const auto &event : events.events()) {
        switch (event.event()) {

            case EventGadget::PluginActivate: {
                mState = Active;
                emit stateChanged();
            } break;
            { ~~~ }
            case EventGadget::GuiCreate: {
                // Verify successfull creation back to the host.
                auto call = client->ClientEventCall(create(EventGadget::GuiCreate));
                mState = Connected;
                emit stateChanged();
            } break;
            { ~~~ }
            case EventGadget::GuiShow: {
                client->ClientEventCall(create(EventGadget::GuiShow));
                setVisible(true);
            } break;
            { ~~~ }
}
```

The `processEvents` function will be called every time a new message is received.
We iterate over all received events and handle them accordingly in a switch statement.
Since we provided the type of the event as an enum, we can easily switch over those
cases. This switch statement is then responsible to trigger the signals, so that
subscribe components can react to those events.

```cpp
// src/clapinterface/qclapinterface.cpp
            case EventGadget::Note: {
                if (!event.hasNote())
                    return;
                emit noteReceived(event.note());
            } break;

```

The note event for instance, will emit the `noteReceived` signal. All QML
components that utilize this signal will then be notified and can react to this
event and visualize it.


### QML Components

In this section we will take a look at the custom QML components that are
provided by this library. Those components can be used to quickly build an user
interface, that dynamically reacts to the events received from the server and
has the capabilities to return updated values of them. For instance, when a
client uses the `ClapWindow.qml` component, the application window will
automatically react to the `GuiShow`, `GuiHide` and the `GuiSetTransient`
events. It simply wraps the functionality of Qts native `ApplicationWindow` QML
type. A section of its implementation is shown below:

```qml
// src/clapcontrols/ClapWindow.qml

import QtQuick
import QtQuick.Controls.Basic

import Clap.Interface
import Clap.Controls

ApplicationWindow {
    id: ctrl
    ~~~
    Connections {
        target: ClapInterface
        function onVisibleChanged() {
            ctrl.visible = ClapInterface.visible;
        }
        function onTransientParentChanged() {
            ctrl.transientParent = ClapInterface.transientParent;
        }
        function onStateChanged() {
            ctrl.pluginActive = QClapInterface.state == QClapInterface.Active;
        }
    }
    ~~~
}
```

We import the `Clap.Interface` module shown in the previous section, to gain
access to the `ClapInterface` singleton. We then connect to the signals that
it emits. When the hosts requests to show or hide the UI, we will react to
the `onVisibleChanged` signal and update the `visible` property of the window accordingly.
Similarly the other signals will be handled within this component. A potential user
would then import those controls and use them in their application:

```qml
import Clap.Controls

ClapWindow {
    id: root
    title: "My CLAP GUI!"
    width: 640; height: 480
}
```

This would already suffice for a basic plugin, where the window is controlled
by the host application. Another example is the `ClapDial` component. Dials are
frequently used in audio plugins. Most parameters provided by a plugin will be
displayed as a single controllable unit by the host. In order to automate, modulate or simply interact with those
parameters, hosts often visualize them as a dial. The `ClapDial` component aligns
with those requirements and automatically acts upon the incoming events from the host.

Since, the only thing that this component will process are the parameter signals received by the `ClapInterface`
class, a user would be free to implement the visualization of the dial as they wish. A section
of the default implementation is shown below:

```qml
import Clap.Interface
import Clap.Controls

Dial {
    id: ctrl
    Connections {
        target: ClapInterface
        function onParamChanged() {
            ctrl.param = ClapInterface.param(paramId);
            ctrl.value = ctrl.param.value;
            ctrl.modulation = ctrl.param.modulation;
        }
        function onParamInfoChanged() {
            ctrl.paramInfo = ClapInterface.paramInfo(paramId);
            ctrl.value = ctrl.paramInfo.defaultValue;
            ctrl.from = ctrl.paramInfo.minValue;
            ctrl.to = ctrl.paramInfo.maxValue;
        }
~~~
    onValueChanged: {
        if (ctrl.pressed || ctrl.hovered) {
            ClapInterface.enqueueParam(ctrl.paramId, ctrl.value);
        }
    }
~~~
```

Here we also connect this component to the `ClapInterface` singleton and act
upon incoming signals. The `onParamChanged` signal will be emitted when we
receive updates from the host application. We then update our properties. Thanks
to QMLs excellent property system, those changes will be automatically
handled by their respective bindings.

For the `onValueChanged` signal, we check if the dial is currently pressed or
hovered, to verify that the user is currently interacting with the dial from
within the UI. If that is the case, we enqueue the updated value to be
sent to the server-library, which will incorporate those changes into the
audio-processing section of the plugin.

With this approach, we successfully abstracted the communication. Users
of this client library can now focus on handling the visualization, in a
way that is identical to the way they would implement a native Qt application.
Users should ideally not have to worry about the communication with the server,
but can opt-in to handle those events if they wish to do so.

### Example Plugins

In this last section we will showcase the implementation of a simple plugin
that uses the proposed libraries. There would be no better candidate for this
then a **Gain** plugin. We've already seen the implementation of such a plugin
in the [previous section](#creating_a_clap_plugin). Where we only used
the `clap` library to create a minimal implementation. However this
implementation only covered the basics and didn't even provide a UI, which
would be a tremendous amount of work to implement without third-party libraries.
The build file of this plugin is shown below:

```cmake
cmake_minimum_required(VERSION 3.2)

project(QtClapPlugins
    VERSION 0.0.1
    LANGUAGES CXX
)

include(../cmake/autolink_clap.cmake)

set(target ${PROJECT_NAME})
add_library(${target} SHARED
    clap_entry.cpp
    reveal/revealprocessor.h
    reveal/revealprocessor.cpp
    gain/gainprocessor.cpp
    gain/gainprocessor.h
)

target_link_libraries(${target} PRIVATE clap-remote)
create_symlink_target_clap(${target})

# Add GUIs
add_subdirectory(reveal/gui)
add_subdirectory(gain/gui)
```

After defining the `project`, we include the `autolink_clap.cmake` file, which
automatically symlinks the generated CLAP into the correct location and renames
the library to align with the CLAP specification. We then create the
shared-library, which will produce the DSO that will get loaded by the host and
link it against the clap-remote library. `clap-remote` contains the API
abstraction for the CLAP interface, and provides the server implementation. The
`clap_entry.cpp` file contains the boilerplate code for the exported
`clap_entry` symbol, as well as the `clap_plugin_factory` implementation.
The next files contain the DSP implementation of the actual plugin within this suite.
This CLAP has two plugins, the `gain` plugin and the `reveal` plugin.
Both of them sit directly on the plugin layer and directly interact with the
processing callback. Lastly, we add the subdirectories for the GUIs. Those
are independent executables, and can be started without the host application.
However, when they connect to the server, they will start to act upon the
received events. Let's have a look at the `gainprocessor` declaration:

```cpp
#ifndef GAINPROCESSOR_H
#define GAINPROCESSOR_H

#include <plugin/coreplugin.h>

class Gain final : public CorePlugin
{
public:
    Gain(const std::string &pluginPath, const clap_host *host);
    static const clap_plugin_descriptor *descriptor();

private:
    bool init() noexcept override;
    void defineAudioPorts() noexcept;

private:
    uint32_t mChannelCount = 1;
};

#endif // GAINPROCESSOR_H
```

Here we override the `init` and `defineAudioPorts` function and define
a member that tracks our audio channel count. The constructor of this class will
then propagate the required information to the `CorePlugin` base class:

```cpp
Gain::Gain(const std::string &pluginPath, const clap_host *host)
    : CorePlugin(Settings(pluginPath).withPluginDirExecutablePath("Gain"),
        descriptor(), host, std::make_unique<GainModule>(*this))
{}
```

We will first create a `Settings` object, which will be used to define the
path to the plugins executable. When the base class determines that
the path is a valid executable, the GUI feature will be enabled. We additionally
pass the descriptor and the host to the base class and lastly create the
root module of the plugin. The module based appraoch is used to seperate all
processing logic from the plugin layer. However this is subject to change
and hasn't received much attention during the development of this library,
since the focus was to enable a fluent communication first. The `GainModule`
that we create will handle the actual processing of the plugin and also
define the parameters that will be exposed to the host:

```cpp
// The GainModule controls all the processing of the plugin.
class GainModule final : public Module
{
public:
    explicit GainModule(Gain &plugin) : Module(plugin, "GainModule", 0)
    {}

    void init() noexcept override
    {
        mParamGain = addParameter(
            0, "gain",
            CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_IS_MODULATABLE | CLAP_PARAM_REQUIRES_PROCESS,
            std::make_unique<DecibelValueType>(-40.0, 40.0, 0)
        );
    }

    clap_process_status process(const clap_process *process, uint32_t frame) noexcept override
    { // Same as the 'mini_gain' example }

private:
    Parameter *mParamGain = nullptr;
};
```

Here we add the `gain` parameter, which will be exposed to the host. The definition
of the process callback has been omitted for brevity, but it is identical to the
one shown in the [mini_gain](#processing) example. Besides some unimportant
initialization, this is all that is required to create a plugin that can
already be used in a host application. Next, we will take a look at the GUI
implementation which resides in a seperate projet. The projects build
file is shown below:

```cmake
find_package(Qt6 REQUIRED COMPONENTS Core Qml Quick)
qt_standard_project_setup(REQUIRES 6.5)

set(target Gain)
qt_add_executable(${target} main.cpp)

set_target_properties(${target}
    PROPERTIES
        WIN32_EXECUTABLE TRUE
        MACOSX_BUNDLE TRUE
)

qt_add_qml_module(${target}
    URI "GainModule"
    VERSION 1.0
    QML_FILES "Main.qml"
)

target_link_libraries(${target}
    PRIVATE
        Qt6::Quick
        clapinterfaceplugin
        ClapControls
)

create_symlink_target_gui(${target})
```

We first find the required Qt modules and then declare the executable.
This build file looks identical to other Qt applications. The only difference
here is that we link against the `clapinterfaceplugin` and `ClapControls`
libraries to enable the client-capatibilities. The entry point for this
application looks like this:

```cpp
int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QQmlApplicationEngine engine;
    engine.loadFromModule("GainModule", "Main");
    if (engine.rootObjects().isEmpty())
        return -1;
```

The first part looks like a regular Qt application. We create the `QGuiApplication`
object. Since this application is started as a seperate process, we don't need
to jump through any hoops to enable the Qt event loop. It can be started like
any other application.

```cpp
    QCommandLineParser parser;
    parser.process(app);
    const auto args = parser.positionalArguments();
    if (args.length() == 2) {
        auto *interface = engine.singletonInstance<QClapInterface*>("Clap.Interface","ClapInterface");
        if (interface == nullptr) {
            qFatal() << "Unable to find ClapInterface instance";
            return -1;
        }
        interface->connect(args[0], args[1]);
    }
    return QGuiApplication::exec(); // Start the event loop.
```

The second half of `main` looks a bit different. The current implementation
demands to start the `connect` function of the QClapInterface manually.
The arguments that we will receive as `argv` should contain the `url` and
and the `hash` of the plugin. If arguments are provided, we will connect
to the server and start the event loop. The last part of this example is
the `Main.qml` file. Here we utilize the custom components to interact
with the server and host application:

```qml
~~~
import Clap.Controls

ClapWindow {
    id: root
    title: "QGain"
    width: 640; height: 480

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 10

        Text { ~~~ }
        ClapDial {
            id: dial
            paramId: 0
            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
            Layout.preferredHeight: parent.width / 3
            Layout.preferredWidth: parent.width / 3
        }
        Image { ~~~ }
    }
}
```

We first create the `ClapWindow` and then add a `ColumnLayout` to it.
The column then contains a Text, the `ClapDial` and a logo. Notable is
that the id of the dial is set to `0`. This is the id of the parameter
that we initialized in the `GainModule` earlier. Each parameter
needs to have a unique id, so that no ambiguity can occur. This simplified
code example already provides a fully functioning plugin UI that can communicate
with a host application. Finally, we build the plugin-suite and load it inside
a host application. We will use Bitwig Studio for that.

![QGain Plugin Hosted](images/qtclapplugins_gain.png){#fig:qgain}

*@fig:qgain* shows the QGain plugin hosted inside Bitwig Studio.


![QReveal Plugin Hosted](images/qtclapplugins_reveal.png){#fig:qreveal}

*@fig:qreveal* shows the QReveal plugin hosted inside Bitwig Studio.
