## 2.3 The Qt Framework

[Qt](https://www.qt.io/), emerging in the early 90s from Trolltech in Norway,
stands out in the landscape of software development for its robust toolkit that
simplifies the creation of graphical user interfaces (GUIs) that are
platform-agnostic. With Qt, the same codebase can be deployed on multiple
operating systems such as Linux, Windows, macOS, Android or embedded systems
with little need for modification, streamlining the development process
significantly.

After its acquisition by Nokia in 2008, Qt has continued to thrive under the
guidance of The Qt Company, which is responsible for its ongoing development
and maintenance. Qt provides two licensing options: the GNU (L)GPL license,
promoting a community-driven approach, and a commercial license for developers
who prefer to maintain exclusive control over their software.

Qt's versatility extends beyond its C++ core, offering language bindings for
additional programming languages, with Python being a notable example through
[Qt for Python / PySide6](https://doc.qt.io/qtforpython-6/). This extension
facilitates rapid development by allowing the integration of Qt's powerful C++
modules within Python's flexible scripting environment.

### 2.3.1 Core Techniques

Central to Qt is its [Signals &
Slots](https://doc.qt.io/qt-6/signalsandslots.html) mechanism, which can be
thought of as a flexible implementation of the observer pattern. A 'signal'
represents an observable event, while a 'slot' is comparable to an observer that can
react to that event. This design allows for a many-to-many relationship meaning
any signal may be connected to any number of slots. This concept has been
fundamental to Qt since its first release in 1994 and the concept of signals
and slots is so compelling that it has become a part of the computer
science landscape.

Essential for the reactive nature of graphical user interfaces, signals & slots
enables a program to handle user-generated events such as clicks and
selections, as well as system-generated events like incoming data
transmissions. Qt employs the moc (Meta Object Compiler), a tool developed
within the Qt framework, which seamlessly integrates these event notifications
into Qt's event loop.

Let's examine a code example to understand the practicality and simplicity of
signals & slots in Qt:

```cpp
// signals_and_moc.cpp
#include <QCoreApplication>
#include <QRandomGenerator>
#include <QTimer>
#include <QDebug>

// To use signals and slots, we must inherit from QObject.
class MyObject : public QObject
{
    Q_OBJECT  // Tell MOC to target this class
public:
    MyObject(QObject *parent = 0) : QObject(parent) {}
    void setData(qsizetype data)
    {
        if (data == mData) { // No change, don't emit signal
            qDebug() << "Rejected data: " << data;
            return;
        }
        mData = data;
        emit dataChanged(data);
    }
signals:
    void dataChanged(qsizetype data);
private:
    qsizetype mData;
};
```

In this code snippet, we create a class integrated with Qt's [Meta-Object
System](https://doc.qt.io/qt-6/metaobjects.html). To mesh with the Meta-Object
System, three steps are critical:

1. Inherit from `QObject` to gain meta-object capabilities.
2. Use the `Q_OBJECT` macro to enable the **MOC's** code generation.
3. Utilize the `moc` tool to generate the necessary meta-object code,
   facilitating signal and slot functionality.

The `MyObject` class emits the `dataChanged` signal only when new data
is set.

```cpp
// signals_and_moc.cpp
void receiveOnFunction(qsizetype data)
{ qDebug() << "1. Received data on free function: " << data; }

class MyReceiver : public QObject
{
    Q_OBJECT
public slots:
    void receive(qsizetype data)
    { qDebug() << "2. Received data on member function: " << data; }
};

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    MyObject obj;
    MyReceiver receiver;

    QTimer timer; // A timer to periodically change the data
    timer.setInterval(1000);
    QObject::connect(&timer, &QTimer::timeout, [&obj]() {
        obj.setData(QRandomGenerator::global()->bounded(3));
    });
    timer.start();

    // Connect the signal to three different receivers.
    QObject::connect(&obj, &MyObject::dataChanged, &receiveOnFunction);
    QObject::connect(&obj, &MyObject::dataChanged, &receiver, &MyReceiver::receive);
    QObject::connect(&obj, &MyObject::dataChanged, [](qsizetype data) {
        qDebug() << "3. Received data on lambda function: " << data;
    });

    // Start the event loop, which handles all published events.
    return QCoreApplication::exec();
}
```

The given code snippet demonstrates Qt's signal and slot mechanism by setting
up a series of connections between a signal and various slot types using
`QObject::connect`. In a typical compilation scenario for such a Qt application
on a Linux system, you'd run a command like:

```bash
g++ -std=c++17 -I/usr/include/qt6 -I/usr/include/qt6/QtCore -lQt6Core -fPIC -o build/signals_and_moc signals_and_moc.cpp
```

However, this direct approach to compile the file will be met with linker
errors, a sign that something is missing:

```bash
/usr/bin/ld: /tmp/ccbsQpDO.o: in function `main':
signals_and_moc.cpp:(.text+0x2e4): undefined reference to `MyObject::dataChanged(long long)'
/usr/bin/ld: /tmp/ccbsQpDO.o: in function `MyObject::MyObject(QObject*)':
signals_and_moc.cpp:(.text._ZN8MyObjectC2EP7QObject[_ZN8MyObjectC5EP7QObject]+0x26): undefined reference to `vtable for MyObject'
```

These errors indicate that we have a missing implementation for our
`dataChanged` function, which handles the signaling mechanism of our object.
This is where the meta-object-compiler comes into play and provides the needed
(boilerplate) implementation to this function. The moc is integral to Qt's
signal and slot system, and without it, the necessary meta-object code that
facilitates these connections is not generated. To correct the compilation
process, moc must be run on the source files containing the Q_OBJECT macro
before the final compilation step.

```bash
# assuming moc is in $PATH
moc signals_and_moc.cpp > gen.moc
# include the generated file at the end of our source
echo '#include "gen.moc"' >> signals_and_moc.cpp
# re-compile
```

A closer look at the output from moc uncovers the previously missing pieces,
providing the essential implementation for our `dataChanged` signal and
clarifying how moc underpins the signal's integration.

```cpp
// SIGNAL 0
void MyObject::dataChanged(qsizetype _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}
```

After running the corrected compilation and executing the application, the
output could look like this:

```bash
./build/signals_and_moc
1. Received data on free function:  0
2. Received data on member function:  0
3. Received data on lambda function:  0
Rejected data:  0
```

### 2.3.2 Graphics

Within the vast expanse of the Qt universe, developers are presented with two
primary paradigms for GUI crafting: Widgets and QML. Widgets offer the
traditional approach in creating UI elements, making it the go-to for many
classical desktop applications. Their imprint is evident in widely-adopted
applications
like
[Telegram](https://github.com/telegramdesktop/tdesktop/blob/dev/Telegram/SourceFiles/mainwidget.cpp#L255)
and [Google
Earth](https://github.com/google/earthenterprise/blob/893f6b470673e2bad4cacdd8eec5ad1f179b6249/earth_enterprise/src/fusion/fusionui/main.cpp#L28).

Conversely, **QML** (Qt Modeling Language) represents a contemporary,
declarative approach to UI design. It employs a clear, JSON-like syntax, while
utilizing inline JavaScript for imperative operations. Central to its design
philosophy is dynamic object interconnection, leaning heavily on property
bindings. One of its notable strengths is the seamless integration with C++,
ensuring a clean separation between application logic and view, without any
significant performance trade-offs.

Above the foundational QML module resides **QtQuick**, the de facto standard
library for crafting QML applications. While [Qt
QML](https://doc.qt.io/qt-6/qtqml-index.html) lays down the essential
groundwork by offering the QML and JavaScript engines, overseeing the core
mechanics, QtQuick comes equipped with fundamental types imperative for
spawning user interfaces in QML. This library furnishes the visual canvas and
encompasses a suite of types tailored for creating and animating visual
components.

A significant distinction between Widgets and QML lies in their rendering
approach. Widgets, relying on software rendering, primarily lean on the
CPU for graphical undertakings. This sometimes prevents them from harnessing
the full graphical capabilities of a device. QML, however, pivots this paradigm
by capitalizing on the hardware GPU, ensuring a more vibrant and efficient
rendering experience. Its declarative nature streamlines design interpretation
and animation implementation, ultimately enhancing the development velocity.

**Qt6**, the most recent major release in the Qt series, introduced a series of
advancements. A standout among these is the QRHI (Qt Rendering Hardware
Interface). Functioning subtly in the background, QRHI adeptly handles the
complexities associated with graphic hardware. Its primary mission is to
guarantee determined performance consistency across a diverse range of graphic
backends. The introduction of [QRHI](https://doc.qt.io/qt-6/qrhi.html)
underscores Qt's steadfast dedication to strengthen its robust cross-platform
capabilities, aiming to create a unified experience across various graphic
backends.

![Qt Rendering Hardware Interface [RHI](https://doc.qt.io/qt-6/topics-graphics.html)](images/qt_rhi.png){#fig:qrhi}

*@fig:qrhi* presents the layered architecture of the rendering interface. At
its base, it supports native graphic APIs such as OpenGL, Vulkan or Metal.
Positioned just above is the QWindows implementation, which is housed within
the QtGui module of Qt. Notably, QtWidgets occupies a niche between these two
levels. Given that Widgets emerged before the QRhi module, their integration
with QRhi isn't as profound. While certain widgets do offer OpenGL
capabilities, their primary reliance is on the
[QPainter](https://doc.qt.io/qt-6/qpainter.html#drawing) API. Ascending to the
subsequent tier, we are greeted by the Qt Rendering Hardware Interface, which
serves as a crucial bridge, offering an abstraction layer over
hardware-accelerated graphics APIs.

> **Note:** As of this writing, QRHI maintains a limited compatibility
> assurance, implying potential shifts in its API. However, such changes are
> anticipated to be minimal.

Further up the hierarchy, the QtQuick and QtQuick3D modules showcase Qt's
progression in graphics rendering.

In essence, Qt delivers a comprehensive framework for designing user interfaces
that cater to the diverse array of existing platforms. It provides a broad
spectrum of configuration and customization options and even enabling
developers to tap into lower-level abstractions for crafting custom extensions
tailored to specific project requirements—all while preserving *cross-platform
functionality*. Beyond the foundational modules geared toward GUI development,
Qt also offers a rich suite of additional modules. These include resources for
localizing applications with
[qttranslations](https://code.qt.io/cgit/qt/qttranslations.git/), handling
audio and video via
[qtmultimedia](https://code.qt.io/cgit/qt/qtmultimedia.git/), and various
connectivity and networking options. Modules such as
[qtconnectivity](https://code.qt.io/cgit/qt/qtconnectivity.git/),
[qtwebsockets](https://code.qt.io/cgit/qt/qtwebsockets.git/), and the latest
addition, [qtgrpc](https://code.qt.io/cgit/qt/qtgrpc.git/), facilitate the
integration of Qt into an even wider range of systems.

### 2.3.3 QtGrpc and QtProtobuf

The [QtGrpc](https://doc.qt.io/qt-6/qtgrpc-index.html) module, which is in a
technical preview stage as of Qt version 6.6, represents an innovative addition
to Qt's suite. It provides plugins for the `protoc` compiler, which we touched
upon in section **3.2**. These plugins are designed to serialize and
deserialize Protobuf messages into Qt-friendly classes, facilitating a smooth
and integrated experience within the Qt framework. This integration
significantly reduces the need for additional boilerplate code when working
with Protobuf and gRPC.

To enhance our previous `event.proto` definition with Qt features, let's
compile it using the qtprotobufgen plugin as follows:

```bash
# assuming qtprotobufgen is in $PATH
protoc --plugin=protoc-gen-qtprotobuf=qtprotobufgen --qtprotobuf_out=./build event.proto
```

This command will generate the `build/event.qpb.h` and `build/event.qpb.cpp` files.
A review of the generated header demonstrates how the plugin eases the
integration of Protobuf definitions within the Qt framework:

```cpp
// build/event.qpb.h
~~~
class Event : public QProtobufMessage
{
    Q_GADGET
    Q_PROTOBUF_OBJECT
    Q_DECLARE_PROTOBUF_SERIALIZERS(Event)
    Q_PROPERTY(example::TypeGadget::Type id_proto READ id_proto WRITE setId_proto SCRIPTABLE true)
    Q_PROPERTY(QString name READ name WRITE setName SCRIPTABLE true)
    Q_PROPERTY(QString description READ description_p WRITE setDescription_p)
    Q_PROPERTY(bool hasDescription READ hasDescription)

public:
    using QtProtobufFieldEnum = Event_QtProtobufNested::QtProtobufFieldEnum;
    Event();
    ~Event();
~~~
```

In the provided C++ header file, the `Event` class inherits from
`QProtobufMessage` to leverage Qt's meta-object system for seamless
serialization and deserialization of Protocol Buffers. This class definition
enables the automatic conversion of data types defined in .proto files to
Qt-friendly types. Such integration allows developers to use these types with
ease within both the Qt C++ environment and QML, facilitating rapid and
adaptable integration with gRPC services, irrespective of the server's
implementation language.

Furthermore, QtGrpc extends the functionality of Protocol Buffers within the Qt
framework by providing essential classes and tools for gRPC communication. For
example, `QGrpcHttp2Channel` offers an *HTTP/2* channel implementation for server
communication, while `QGrpcCallReply` integrates incoming messages into the Qt
event system. This synergy between QtGrpc and protocol buffers streamlines
client-server interactions by embedding Protocol Buffer serialization within
Qt's event-driven architecture.
