# Chapter 4: Remote Audio Plugin Development

## 4.1 Overview

This section transitions from defining the problem and introducing the
necessary foundation to detail our primary objective: enabling a seamless
native development experience. This is achieved by moving away from rigid
methods, such as the requirement for users to compile Qt in a separate
namespace. Additionally, ensuring stability across all desktop platforms is a
critical requirement.

![Overview of Traditional Plugin Architecture](images/plugins_traditional.png){#fig:plug_tradition}

*@fig:plug_tradition* outlines the conventional architecture of audio plugins
when integrated with graphical user interfaces. In this context, multiple
instances are generated from our DSO, hence enforcing reentrancy. The
traditional approach involves initiating the user interface's event loop within
a distinct thread, typically resulting in at least two threads operating within
the DSO's process. This method is evident in JUCE^[https://juce.com/], a
renowned library for creating agnostic plugins, and is also employed in various
open-source projects.

Starting Qt's event loop within a dedicated thread presents significant
challenges. To address these, we opted to run the plugin's GUI in
an entirely separate process. While this might initially seem
resource-intensive or complex, especially given the intricacies of Inter
Process Communication (IPC), the experimental findings have confirmed the
effectiveness and feasibility of this approach.

With modern computers capable of efficiently handling an additional process,
concerns regarding increased resource usage are mitigated. Operating the
plugin's GUI in a separate process circumvents the re-entrancy issue with Qt
and contributes to the overall stability and security of the implementation.

Isolating the GUI in a separate process strengthens the architecture. This
ensures that GUI-related issues do not affect the plugin's core functionality.
It also increases the plugin's resilience to crashes or glitches in the GUI,
safeguarding the main application's integrity and functionality. This method,
though initially more complex, ultimately enhances reliability and user
experience.

Consider a scenario where both the GUI and the real-time thread coexist within
the same process. In such a case, a crash in the GUI could lead to the failure
of the real-time thread. However, if the GUI operates as a separate process and
encounters a crash, the real-time thread remains unaffected. Implementing IPC
or RPC techniques, while more complex, does not significantly impact user
experience. This is because the data exchanged between the processing and GUI
unit, such as parameter adjustments or note events, is minimal, often just a
few bytes. For sharing larger sets of data like audio, strategies like using
shared memory pools can be effective, ensuring speed comparable to direct
thread communication. However, this approach does introduce complexity in
implementation. It is also worth noting that the operational speed of GUIs does
not need to match that of real-time threads.

Human perception plays a crucial role when interacting with GUIs. The human
eye's ability to perceive smooth and fluid visuals is limited, a concept
encapsulated in the critical flicker fusion rate:

> The critical flicker fusion rate is defined as the rate at which human
> perception cannot distinguish modulated light from a stable field. This rate
> varies with intensity and contrast, with the fastest variation in luminance one
> can detect at 50–90 Hz

Recent research suggests that humans can detect flickering up to 500 Hz
[@flicker]. Standard desktop monitor refresh rates vary from 60 to 240 Hz,
corresponding to *16 to 4.1 ms*. This range sets the minimum communication
speed an application should aim for to ensure a seamless user experience.

To integrate Qt GUIs into the CLAP standard, we will employ the following
techniques:

1. Develop a native gRPC server to manage client connections and handle
   bidirectional event communication.
2. Implement a client-library that supports the server's API.
3. Launch the GUI in a separate process; establish a connection to the server
   upon startup.
4. Remotely control the GUI from the host for actions like showing and hiding.
5. Optionally, enable the client to request specific functionalities from the
   host.

Utilizing these methods in conjunction with
QtGrpc^[https://doc.qt.io/qt-6/qtgrpc-index.html] has proven successful,
delivering a reliable, fast, and native experience on desktop platforms.

## 4.2 Remote Control Interface

In this section, we explore the development of the Remote Control Interface
(RCI hereon) for CLAP. This interface is designed as a thin overlay on top of
the CLAP API, providing a streamlined plugin layer abstraction. Its primary
function is to abstract the interface in a manner that ensures communication is
not only automatically configured but also consistently delivered. Users of
this library are encouraged to engage with the features they wish to tailor to
their needs.

> Note: The techniques and code examples presented in the subsequent sections
> represent the current stage of development and may not reflect the most
> optimized approaches. While several aspects are still under refinement, the
> fundamental concept has already demonstrated its effectiveness.

To ensure our server efficiently manages multiple clients, we will employ
techniques similar to those used in *Q\*Application* objects. These were the
same techniques that initially presented challenges in integration, leading to
the extensive research detailed here. As highlighted in the
[objective](#objective) section, static objects can be problematic but also
have the potential to significantly enhance the architecture and, in this case,
performance. As briefly touched upon in the [clap-debugging](#debugging)
section, the host may implement various plugin hosting modes. These modes
impose additional constraints on our project, as the server must operate
smoothly across all of them. The specific requirements for our server
implementation are as follows:

- In scenarios where all plugins within a CLAP reside in the same process
  address space, we can optimize resource usage by sharing the server across
  multiple clients.
- In contrast, loading every plugin into a separate process, while being the
  safest approach, is also the most resource-intensive. In this case, reusing the
  server is not possible, demanding a separate server for each plugin, which
  leads to underutilized potential.

By avoiding the use of Qt's event system, we gain greater flexibility and
overcome various integration challenges. At this stage, this approach appears
to be the most flexible, performant, and stable solution. The adoption of gRPC
leverages the language-independent features of protobuf's IDL, allowing clients
the freedom to implement the server-provided API in any desired manner. The
server's primary role is to efficiently manage event traffic to and from its
clients. This opens up possibilities such as controlling plugins through
embedded devices or having multiple clients connect to the same plugin, as long
as they support gRPC and have a client implementation for the API.
Additionally, we can statically link all dependencies against our library
target, enhancing portability. This aspect of portability is vital,
particularly in the domain of plugins, which often require extra attention and
care.

![CLAP-RCI Architecture](images/clap-rci_arch.png){ #fig:claprciarch width=77% }

*@fig:claprciarch*, represents the architecture of the developed server
library. At the foundational level, there is a host that is compatible with the
CLAP interface. Positioned just above is the CLAP API, which the host employs
to manage its plugins. The client also interacts with this API, but with the
addition of the CLAP-RCI wrapper library in-between, which seamlessly
integrates all necessary communication mechanisms. Above this layer is the
client implementation of CLAP-RCI. The communication between these two layers
is facilitated through the HTTP/2 protocol of gRPC. This structure enables the
client implementation to support a diverse range of languages while providing
independent and robust interference with the plugin's backend.

### 4.2.1 Server Implementation

When designing IPC mechanisms, there are two primary interaction styles:
synchronous and asynchronous. Synchronous communication is characterized as a
request/response interaction style, where a response to the RPC is awaited
actively. In contrast, asynchronous communication adopts an event-driven
approach, offering the flexibility for customized processing, which can
potentially lead to more efficient hardware utilization.[@ipc_microservices]

At the core of the gRPC C++ asynchronous API is the *Completion Queue*. As
described in the documentation:

```cpp
/// grpcpp/completion_queue.h
/// A completion queue implements a concurrent producer-consumer queue, with
/// two main API-exposed methods: \a Next and \a AsyncNext. These
/// methods are the essential component of the gRPC C++ asynchronous API
```

The gRPC C++ library offers three distinct approaches for server creation:

1. **Synchronous API**: This is the most straightforward method, where event
   handling and synchronization are abstracted away. It simplifies server
   implementation, with the client call waiting for the server's response.
2. **Callback API**: This serves as an abstraction over the asynchronous API,
   offering a more accessible solution to its complexities. This approach was
   introduced in the proposal *L67-cpppcallback-api*^[https://github.com/grpc/proposal/blob/master/L67-cpp-callback-api.md].
3. **Asynchronous API**: The most complex yet highly flexible method for
   creating and managing RPC calls. It allows complete control over the
   threading model but at the cost of a more intricate implementation. For
   example, explicit handling of RPC polling and complex lifetime management are
   required.

Since our application of gRPC deviates from traditional microservice
architectures and operates within a constrained environment, we have opted for
the asynchronous API. This choice allows for a more adaptable approach to our
library's design. The following illustrates the code flow we will employ:

![Lifetime of CLAP-RCI](images/clap-rci_sequence_diag.svg){#fig:claprcilifetime}

*@fig:claprcilifetime*, outlines the server's life-cycle. The process begins
with the creation of the first plugin, triggering the static server controller
to initiate and start the server. When a plugin instance launches its GUI, it
is provided with necessary command line arguments, such as the server's address
and the plugin identifier. The GUI process then attempts to connect to the
server. On the server side, the connection from the GUI client is managed, and
an internal polling callback mechanism is initiated. This mechanism is
responsible for polling and dispatching events to all active clients of every
plugin. It is crucial to note, although not depicted in the diagram, that the
server is capable of managing multiple plugin instances, each potentially
having several clients. The event polling stops when the last client
disconnects and all server-side streams are closed, a strategy implemented to
enhance performance and minimize resource consumption in the absence of
connected clients. Finally, the server is terminated when the last plugin is
unloaded by the host, and the Dynamic Shared Object (DSO) is about to be
unloaded.

To accurately identify or 'register' each plugin instance within the context of
the static server handler, we employ a hashing algorithm known as
MurmurHash^[https://en.wikipedia.org/wiki/MurmurHash]. This step is crucial
for ensuring that each GUI connecting to the server can be correctly mapped to
its corresponding plugin instance. The generated hash value is used both as a
command line argument when launching the GUI and as the key for inserting the
plugin into a map. The following C++ code snippet demonstrates the
implementation of MurmurHash3:

```cpp
// core/global.h

// MurmurHash3
// Since (*this) is already unique in this process, all we really want to do is
// propagate that uniqueness evenly across all the bits, so that we can use
// a subset of the bits while reducing collisions significantly.
[[maybe_unused]] inline std::uint64_t toHash(void *ptr)
{
    uint64_t h = reinterpret_cast<uint64_t>(ptr);
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccd;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53;
    return h ^ (h >> 33);
}
```

To facilitate communication with plugin instances, we have encapsulated all
communication-related logic within the `SharedData` class. It serves as the
hub for communication between the plugin and the server. This class is
responsible for managing the polling mechanism and operating non-blocking,
wait-free queues for event exchange with the plugin. A separate instance of
*SharedData* is created for each plugin. Below is a segment of the public API
for this class:

```cpp
// server/shareddata.h
class SharedData
{
public:
    explicit SharedData(CorePlugin *plugin);

    // Methods for managing core plugins and event streams
    bool addCorePlugin(CorePlugin *plugin);
    bool addStream(ServerEventStream *stream);
    bool removeStream(ServerEventStream *stream);

    // Methods for handling events
    bool blockingVerifyEvent(Event e);
    bool blockingPushClientEvent(Event e);
    void pushClientParam(const ClientParams &ev);

    // Queue accessors
    auto &pluginToClientsQueue() { return mPluginProcessToClientsQueue; }
    auto &pluginMainToClientsQueue() { return mPluginMainToClientsQueue; }
    auto &clientsToPluginQueue() { return mClientsToPluginQueue; }
    // ...
}
```

As a cornerstone of our architecture, the *CorePlugin* class stands at the
forefront, acting as the primary interface for client interactions with this
library:

```cpp
// plugin/coreplugin.cpp
// CorePlugin::CorePlugin(~) {
~~~
    auto hc = ServerCtrl::instance().addPlugin(this);
    dPtr->hashCore = *hc;
    // The shared data is used as a pipeline for this plugin instance
    // to communicate with the server and its clients
    dPtr->sharedData = ServerCtrl::instance().getSharedData(dPtr->hashCore);

    if (ServerCtrl::instance().start())
        SPDLOG_INFO("Server Instance started");
    assert(ServerCtrl::instance().isRunning());
~~~
```

Here we first add the plugin to the static `ServerCtrl`, which registers it
with the hashing algorithm. We then obtain the `SharedData` instance for
further communication and start the server if it hasn't been started yet.

Next, we explore the internal workings of the gRPC server. We employ the
asynchronous API, chosen for its supreme flexibility. Despite the extensive
coding required for the server implementation, this method provides
considerable control and adaptability in handling the threading model.

The code segment below illustrates the construction of the server and the
creation of the completion queues:

```cpp
// server/server.cpp
    // Specify the amount of cqs and threads to use
    cqHandlers.reserve(2);
    threads.reserve(cqHandlers.capacity());

    // Create the server and completion queues; launch server
    { ~~~ } // Condensed for brevity

    // Create handlers to manage the RPCs and distribute them across
    // the completion queues.
    cqHandlers[0]->create<ServerEventStream>();
    cqHandlers[1]->create<ClientEventCallHandler>();
    cqHandlers[1]->create<ClientParamCall>();

    // Distribute completion queues across threads
    threads.emplace_back(&CqEventHandler::run, cqHandlers[0].get());
    threads.emplace_back(&CqEventHandler::run, cqHandlers[1].get());
```

The above code snippet already illustrates the advantages of utilizing the
asynchronous API. By creating multiple completion queues and distributing them
across various threads, we gain complete control over the threading model. The
first completion queue, positioned at `0`, is dedicated to managing all server
communications to clients, while the second queue, at position `1`, handles all
client communications.

Three distinct *rpc-tag* handlers are employed:

1. `ServerEventStream`: Manages the outgoing stream from the plugin instance to
   its clients.
2. `ClientEventCallHandler`: Addresses significant gRPC unary calls originating
   from the client.
3. `ClientParamCall`: Specifically handles gRPC unary calls related to
   parameter adjustments.

This distinction between call types is crucial. Parameter calls from the client
are directly forwarded to the real-time audio thread. In contrast, event calls,
which can be blocking, do not require real-time handling. Instead, they are
used for critical events that necessitate client verification during the GUI
creation process.

The completion queues, as previously noted, function as FIFO (First-In,
First-Out) structures, utilizing a tag system to identify the nature of the
events being processed. Tags, representing specific instructions, are enqueued
and later retrieved when the gRPC framework is ready to handle them. The
following snippet provides a glimpse into this mechanism:

```cpp
// server/cqeventhandler.cpp
void CqEventHandler::run()
{
    state = RUNNING;
    void *rawTag = nullptr;
    bool ok = false;
    // Repeatedly block until the next event is ready in the completion queue.
    while (cq->Next(&rawTag, &ok)) {
        auto *tag = static_cast<EventTag*>(rawTag);
        tag->process(ok);
    }
~~~
```

At its very core, the completion queue is a blocking call that waits for the
the next operation. We start them in a seperate thread inside the constructor
of the server and they will run until we decide to stop them. The `Next`
function returns a generic `void` pointer, which correlates to the tag we have
enqueued at a earlier point in time. We then cast it back to the original type
and call the process function. The `EventTag` is the base class for all other
tags inside a completion queue. It provides a virtual `process` function which
will be handled by either the derived classes or the base class itself. Extra
care has been taken to keep those *tag* implementations slim to avoid potential
bottlenecks. For instance, a segment from the `ServerEventStream`
implementation looks like the following:

```cpp
void ServerEventStream::process(bool ok)
{
    switch (state) {
        case CONNECT: {
            if (!ok)
                return kill();
            // Create a new instance to serve new clients while we're processing this one.
            parent->create<ServerEventStream>();
            // Try to connect the client. The client must provide a valid hash-id
            // of a plugin instance in the metadata to successfully connect.
            if (!connectClient()) {
                state = FINISH;
                stream.Finish({ grpc::StatusCode::UNAUTHENTICATED,
                    "Couldn't authenticate client" }, toTag(this));
                return;
            }
            if (!sharedData->tryStartPolling()) {
                SPDLOG_INFO("Already polling events!");
            }
            state = WRITE;
            SPDLOG_INFO("ServerEventStream new client @ {} connected", toTag(this));
        } break;
~~~
```

This code segment illustrates the state machine within the `ServerEventStream`,
triggered during the connection phase of new clients. The `connectClient()`
function, as seen here, encapsulates the client authentication process.

A client is required to submit a valid hash-ID of a plugin instance for a
successful connection. If this authentication step fails, the stream will be
closed. This scenario also highlights how the tag-based system operates. The
`stream.Finish` call involves the `toTag` function, which converts the `this`
pointer into a `void` pointer using `reinterpret_cast`. With the state now set
to `FINISH`, the subsequent processing of this tag by the completion queue will
result in another call of the `process` function, but this time in a different state.
Once the client connection is established, we initiate the polling process to
facilitate the delivery of events.

![Server Trace](images/clap-rci_server_trace.png){#fig:claprcitrace}

*@fig:claprcitrace* showcases a section of the tracing when the project is
compiled in debug mode. Debugging can be challenging with multiple threads
operating in the completion queues and the potential simultaneous connection of
several plugins and clients. This necessitates a development and debugging
approach that is both structured and effective.

### 4.3.2 Event-System

Handling and distributing events is a critical and complex part of this
library. It's essential to process events quickly while adhering to real-time
constraints, ensuring events are served in a deterministic and predictable
manner. Additionally, managing the lifespan of incoming RPC calls,
synchronizing access to shared resources, and developing a method to correctly
invoke gRPC calls are significant challenges.

To expand on the last point, the gRPC library offers several methods for
interacting with underlying channels, as seen with the `Finish` method for
closing a channel, along with `Read` and `Write` methods. These are the
essential vehicles to receive and transmit messages. However, the complexity
arises with the asynchronous API, where these calls must be manually enqueued
into the completion queues. This restricts the ability to call them directly
from the plugin for client communication. Instead, they must be enqueued with a
tag and await processing by the completion queue before another can be queued.

> Note: Completion Queues can only have one pending operation per tag.

This limitation is manageable when a single plugin instance communicates with
one client. However, in scenarios where the server is designed to support **N**
plugins, each with **M** clients, the method not only proves to be unscalable
but, in fact, it doesn't scale at all. In such scenarios, the system's ability
to scale effectively is significantly hindered.

An effective and efficient method is to centralize event distribution. This
approach involves scanning through all messages queued from the plugin
instances and dispatching them to the clients simultaneously. Such a technique
significantly improves our operational efficiency. On the plugin side, concerns
about synchronization and threading are softened. Plugins are registered with
the server, obtaining a handle to the `SharedData` object. Events are then
pushed into FIFO queues within SharedData, and the server automatically manages
everything thereafter.

Implementing the event polling mechanism is crucial for this system to function
effectively. To avoid creating another independent thread for this purpose, and
to sidestep the complexities associated with thread switching and
synchronization, I chose to utilize the existing completion queues. These
queues are already active and efficiently managed by internal thread pools.
They are also crucial in facilitating communication with client channels. Our
approach requires enqueuing custom operations into the completion queue. This
queue then takes responsibility for managing the lifecycle of these operations
and coordinating their subsequent invocations. Fortunately, the gRPC library
offers a valuable, though often underutilized, feature for this purpose: the
`Alarm` class.

```cpp
  /// grpcpp/alarm.h
  /// Trigger an alarm on completion queue \a cq at a specific time.
  /// When the alarm expires (at \a deadline) or is cancelled (see \a Cancel),
  /// an event with tag \a tag will be added to \a cq. If the alarm has expired,
  /// the event's success bit will be set to true, otherwise false (i.e., upon cancellation).
  //
  // USAGE NOTE: This is often used to inject arbitrary tags into \a cq by
  // setting an immediate deadline. This technique facilitates synchronization
  // of external events with an application's \a grpc::CompletionQueue::Next loop.
  template <typename T>
  void Set(grpc::CompletionQueue* cq, const T& deadline, void* tag)
```

As highlighted in the *usage note*, this feature aligns perfectly with our
requirements. To efficiently handle completion queues, we developed the
`CqEventHandler` class. This class abstracts and manages the queues, storing
all active (pending) tags:

```cpp
    // server/cqeventhandler.h
~~~
    std::map<std::unique_ptr<eventtag>, grpc::alarm> pendingalarmtags;
    std::map<std::uint64_t, std::unique_ptr<eventtag>> pendingtags;
~~~
```

The `pendingtags` map tracks all currently active streams or calls for
communication, while the `pendingalarmtags` map keeps track of all active
alarms. Notably, our foundational `EventTag` class, while serving as a base for
other tag implementations, is also equipped to handle the `process(bool ok)`
callback:

```cpp
// server/tags/eventtag.h
~~~
class EventTag
{
public:
    using FnType = std::function<void(bool)>;
~~~
    virtual void process(bool ok);
    virtual void kill();

protected:
    CqEventHandler *parent = nullptr;
    ClapInterface::AsyncService *service = nullptr;

    std::unique_ptr<FnType> func{};
    Timestamp ts{};
};
~~~
```

Derived classes have the option to override the `process` function, as
exemplified in the `ServerEventStream` implementation. However, by default, it
executes the `func` member. This design allows for the integration of custom
functions into our alarm tag-based subsystem and serves the foundation for the
event polling.

When an alarm tag is processed by the completion queue, it triggers the `process`
function, a standard procedure for all tags. In instances involving alarm tags,
the function's default implementation is called into action:

```cpp
// server/tags/eventtag.cpp
void EventTag::process(bool ok)
{
    (*func)(ok);
    kill();
}
```

In this implementation, the function is executed and the tag is immediately
destroyed. Currently, this works with single-shot alarm tags, presenting an
opportunity for future optimization. The key interface for interacting with
this mechanism is within the `CqEventHandler` class:

```cpp
// server/cqeventhandler.cpp
bool CqEventHandler::enqueueFn(EventTag::FnType &&f, std::uint64_t deferNs /*= 0*/)
{
    auto eventFn = pendingAlarmTags.try_emplace(
        std::make_unique<EventTag>(this, std::move(f)),
        grpc::Alarm()
    );
    { ~~~ }
    if (deferNs == 0) { // Execute immediately.
        eventFn.first->second.Set(
            cq.get(), gpr_now(gpr_clock_type::GPR_CLOCK_REALTIME), toTag(eventFn.first->first.get())
        );
    } else { // Defer the function call.
        const auto tp = gpr_time_from_nanos(deferNs, GPR_TIMESPAN);
        eventFn.first->second.Set(cq.get(), tp, toTag(eventFn.first->first.get()));
    }

    return true;
}
```

The `enqueueFn` function takes a callable `f` and an optional defer time as
parameters. First, it places the EventTag and Alarm instances into the map.
This storage is crucial because destroying a `grpc::Alarm` triggers its
`Cancel` function, which we want to avoid unless the server is shutting down.
The completion queue should autonomously manage the timing of each tag's
invocation, with manual cancellation reserved for server shutdown. Hence, the
function is either deferred or executed immediately based on the provided
parameters.

With this approach, we can seamlessly integrate events from plugin instances into
their respective completion queue handlers. This not only offers a scalable
approach but also ensures reliable interaction with the gRPC interface from
external threads.

Let's delve into the essential mechanism operating behind the scenes,
responsible for distributing events: the polling mechanism. As previously
shown, a successful connection of a client triggers the `tryStartPolling`
function. Here is how this function is defined:

```cpp
bool SharedData::tryStartPolling()
{
    if (!isValid() || pollRunning)
        return false;

    { ~~~ } // Simplified for brevity

    pollRunning = true;
    mServerStreamCq->enqueueFn([this](bool ok){ this->pollCallback(ok); }, mPollFreqNs);
    return true;
}
```

This function returns false if the callback is already running. If not, it
initiates the callback by enqueueing the `pollCallback` function, into the
previously mentioned code segment. The standard polling frequency, `mPollFreqNs`, is
set at *5,000* nanoseconds or *5* microseconds. This means that every 5
microseconds, the function is re-invoked to check for new events.

```cpp
void SharedData::pollCallback(bool ok)
{
    ~~~
    if (streams.empty()) {
        SPDLOG_INFO("No streams connected. Stop polling.");
        return endCallback();
    }
    ~~~
```

To avoid unnecessary polling in the absence of connected clients, the polling
mechanism is designed to stop automatically. When clients disconnect, either by
themself or by the server, the `streams` container will be updated.

```cpp
~~~
    // Processing events from queues
    const auto nProcessEvs = consumeEventToStream(mPluginProcessToClientsQueue);
    const auto nMainEvs = consumeEventToStream(mPluginMainToClientsQueue);

    if (nProcessEvs == 0 && nMainEvs == 0) {
        // No events to dispatch, increase the backoff for the next callback.
        mServerStreamCq->enqueueFn([this](bool ok){ this->pollCallback(ok); }, nextExpBackoff());
        return;
    }
~~~
```

Upon passing the initial checks, the system starts to process the events
generated by each plugin instance. The `consumeEventToStream` function handles
the translation of CLAP-specific events into gRPC messages. We have branched
the event container into two queues: one for real-time processing and another
for generic operations. This isolation is necessary to comply with the strict
real-time requirements. Both queues are preallocated with a fixed size and are
designed to be wait-free, overwriting old messages when reaching capacity. This
approach is a necessary tradeoff, as blocking on the real-time thread cannot be
afforded. If there are no events to be sent, the system schedules the next
callback with a progressively increasing exponential backoff. This technique is
employed to minimize the server's CPU consumption during its hiatus in event
activity, thereby enhancing overall efficiency.

![Exponential Backoff](images/clap-rci_exp_backoff.png){#fig:expbackoff width=66% }

*@fig:expbackoff* illustrates the exponential backoff curve employed to
determine the interval before the next event check. The curve is segmented into
three color-coded sections, each representing different levels of server
activity and corresponding backoff strategies:

1. **High Event Occurrence (Red Zone)**: In this zone, the server immediately
   processes events as they occur, providing the quickest response during
   high-traffic periods.
2. **Cooldown (Orange Zone)**: This section marks a reduced event rate. Without
   new events, the server increases the backoff time, balancing readiness with
   resource saving.
3. **Standby (Green Zone)**: Representing periods where no events are detected.
   Here, the backoff time is increased progressively, placing the server in a
   standby state. The server operates with the least frequency of checks in this
   zone, optimizing the server's resource utilization until a new event triggers a
   return to the red zone.

Any event occurrence immediately resets the backoff to the shortest interval.
This mirrors common scenarios where users are actively interacting with the
system, triggering frequent events that require quick server responses. Once
user interaction drops off and plugins are not in use, the system seamlessly
shifts into **stand-by** mode to optimize efficiency.

![Exponential Backoff Server Trace](images/clap-rci_exp_backoff_trace.png){#fig:expbackofftrace width=88% }

*@fig:expbackofftrace* captures this dynamic behavior. An additional trace
message in the code helps accentuate the exponential backoff's response to
activity. The trace messages indicate that the backoff interval is reset upon
encountering an event and progressively increasing as the server enters a
period of inactivity.

```cpp
~~~
    // If we reached this point, we have events to send.
    bool success = false;
    for (auto stream : streams) { // For all streams/clients
        if (stream->sendEvents(mPluginToClientsData)) // try to dispatch events.
            success = true;
    }

    { ~~~ } // Condensed for clarity

    // Successfully completed a cycle. Schedule the next callback with
    // the standard polling interval and reset the backoff.
    mPluginToClientsData.Clear();
    mCurrExpBackoff = mPollFreqNs;
    mServerStreamCq->enqueueFn([this](bool ok){ this->pollCallback(ok); }, mPollFreqNs);
~~
```

In this final segment, the process of event dispatching is detailed. It goes
through every connected stream (the `ServerEventStream`s), and attempts to
forward the events. Once this process is successfully executed, the next
callback is arranged at the usual polling rate.

### 4.2.3 CLAP API Abstraction

The previous section has already illustrated the server's structure and the
interaction among its core components. This section delves into the integration
of the server with the CLAP API. Given our insights from the
[CLAP](#the_clap_audio_plugin_standard) section, we're aware that the CLAP
standard offers an extension-based interface that allows plugins to add their
unique functionalities. The API abstraction utilizes the
clap-helpers^[https://github.com/free-audio/clap-helpers] toolkit, provided by
the CLAP team. This small library contains a range of utilities designed to ease the
development processes. Among these is the `Plugin` class, which acts as an
intermediary between the C-API and the plugin specific contexts. It functions as a
slim abstraction layer, employing trampoline functions to relay calls to the
individual c++ instances.

The `CorePlugin` class is employed to offer a standard default implementation,
incorporating server-specific features. Users can then tailor this class by
overriding certain functions in its virtual interface, allowing for customized
functionality.

For client GUI interactions, two separate communication channels are
established. The primary channel is non-blocking, designed for interactions
where timing is critical. Here, events are sent using a User Datagram
Protocol (UDP) inspired, *fire and forget* approach. This approach is practical
because it doesn't require immediate confirmation of the client receiving the
message. The events, generated from the host's process callback, reflect
current values. Due to the frequent updates and the rapid pace at which the
process callback operates, waiting for client feedback is not practical.

Nevertheless, certain situations necessitate waiting for client responses. A
notable instance is the implementation of the GUI extension. In this setup, the
GUI operates as an independent process, leading to extra synchronization
complexities. The approach to manage this includes:-

For client GUI communication, two distinct channels are utilized. The first is
a non-blocking channel, crucial for time-sensitive interactions. In this
channel, events are dispatched in a UDP style,
embracing a *fire and forget* method. Since immediate client receipt of the
message is not critical, this approach is adopted. The events, stemming from
the host's process callback, provide a real-time snapshot of their values.
Given their frequent updates and the high frequency of the process callback,
it's impractical to wait for a client's response.

However, there are scenarios where awaiting client feedback is necessary. This
is particularly true for the GUI extension. In this setup, the GUI operates as
an independent process, leading to extra synchronization complexities. The
approach to manage this includes:

1. Providing the hashed ID of the plugin instance and the server address to the
   GUI.
2. Launching the GUI executable using platform-specific APIs.
3. The GUI then connects to the server and verifies the hash ID.
4. For every subsequent GUI event initiated by the host, such as showing or
   hiding the GUI, it becomes essential to wait for the client's response.

> Note: The CLAP specification does not enforce a strict design and provides
> some freedom to the host application's implementation. Some hosts may choose
> to destroy the GUI after hiding it, while some other hosts may choose to
> conceal the UI, keeping it active in the background.

This variation approach requires a versatile and robust GUI creation
mechanism. The public interface for these GUI-related functions is outlined as
follows in:

```cpp
// coreplugin.h
    // #### GUI ####
    bool implementsGui() const noexcept override;
    bool guiIsApiSupported(const char *api, bool isFloating) noexcept override { return isFloating; };
    bool guiCreate(const char *api, bool isFloating) noexcept override;
    bool guiSetTransient(const clap_window *window) noexcept override;
    bool guiShow() noexcept override;
    bool guiHide() noexcept override;
    void guiDestroy() noexcept override;
```

Every extension can be controlled with the `bool implements<ext>()` function.
If a user overrides this function in their subclassed instance to return false,
the corresponding extension is deactivated and subsequently disregarded by the
host application.

The process of creating a GUI demands synchronization with the client. This
step is crucial to confirm the successful creation and validation of the GUI
before letting the host application proceed.

```cpp
bool CorePlugin::guiCreate(const char *api, bool isFloating) noexcept
{
    if (!ServerCtrl::instance().isRunning()) {
        SPDLOG_ERROR("Server is not running");
        return false;
    }

    if (dPtr->guiProc) {
        SPDLOG_ERROR("Gui Proc must be null upon gui creation");
        return false;
    }
    dPtr->guiProc = std::make_unique<ProcessHandle>();
    { ~~~ } // other checks simplified for brevity
```

Upon entering the `guiCreate` function, a series of critical checks are
performed to ensure the GUI's safe operation. Following this, a new
`ProcessHandle` instance is created, serving as an abstraction for
platform-specific process APIs. This requirement arises from the lack of a
standardized 'process' concept in C++. Therefore, this class provides a uniform
interface for interacting with different platform-specific process APIs. For
instance, the `CreateProcess` API from `Kernel32.dll` is used for Windows
environments, while Linux and macOS employ a POSIX-compliant implementation. A
section of the public interface looks like the following:

```cpp
#if defined _WIN32 || defined _WIN64
#include <Windows.h>
using PidType = DWORD;
#elif defined __linux__ || defined __APPLE__
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
using PidType = pid_t;

{ ~~~ } // Only Windows and Unix supported for now

class ProcessHandle
{
public:
    ProcessHandle();
    { ~~~ }
    bool isChildRunning() const;
    std::optional<int> waitForChild();
    { ~~~ }
    bool startChild();
    std::optional<int> terminateChild();
    { ~~~ }
private:
    std::unique_ptr<ProcessHandlePrivate> dPtr;
};
```

This interface facilitates starting, terminating, or waiting for the completion
of a GUI process. It's utilized in the `guiCreate` function, as demonstrated
in the snippet below:

```cpp
// coreplugin.cpp > guiCreate(~) {
~~~
    // Send the hash as argument to identify the plugin to this instance.
    const auto shash = std::to_string(dPtr->hashCore);
    if (!dPtr->guiProc->setArguments({ *addr, shash })) { // Prepare to launch the GUI
        SPDLOG_ERROR("Failed to set args for executable");
        return false;
    }

    if (!dPtr->guiProc->startChild()) {  // Start the GUI
        SPDLOG_ERROR("Failed to execute GUI");
        return false;
    }
~~~
```

As mentioned earlier in the [server implementation](#server-implementation)
section, a hashing algorithm is utilized to identify each plugin instance. This
hash is provided as an argument to the executable, and it is essential that
the client handles this hash and includes it when initiating communication.

```cpp
// coreplugin.cpp > guiCreate(~) {
~~~
    pushToMainQueueBlocking({Event::GuiCreate, ClapEventMainSyncWrapper{}});
    if (!dPtr->sharedData->blockingVerifyEvent(Event::GuiCreate)) {
        SPDLOG_WARN("GUI has failed to verify in time. Killing Gui process.");
        if (!dPtr->guiProc->terminateChild())
            SPDLOG_CRITICAL("GUI proc failed to killChild");
        return false;
    }
    enqueueAuxiliaries();
    return true;
}
```

Here, the `Event::GuiCreate` event is dispatched to the main queue, triggering
a waiting mechanism for the client's response. Should the client fail to
respond in time, the process is terminated. Otherwise the host is allowed
to proceed.

The next step requires the host to provide a window-handle to the plugin
instance. The GUI plugin must incorporate this handle through a process known
as "re-parenting," which embeds the GUI within the context of another
external window:

```cpp
bool CorePlugin::guiSetTransient(const clap_window *window) noexcept
{
    assert(dPtr->sharedData->isPolling());

    { ~~~ }

    pushToMainQueueBlocking( {Event::GuiSetTransient, ClapEventMainSyncWrapper{ wid } });
    if (!dPtr->sharedData->blockingVerifyEvent(Event::GuiSetTransient)) {
        SPDLOG_ERROR("Failed to get a verification from the client");
        return false;
    }
    return true;
}
```

The `guiSetTransient` implementation shows a similar pattern to the `guiCreate`
function, but it's in the non-blocking event handling where we see a more
streamlined approach:

```cpp
void CorePlugin::processEvent(~~~) noexcept
{
    { ~~~ }
    switch (evHdr->type) {

    case CLAP_EVENT_PARAM_VALUE: {
        const auto *evParam = reinterpret_cast<const clap_event_param_value *>(evHdr);
        { ~~~ }
        param->setValue(evParam->value);
        pushToProcessQueue({ Event::Param, ClapEventParamWrapper(evParam) });
    } break;
    { ~~~ }
    case CLAP_EVENT_NOTE_ON: {
        const auto *evNote = reinterpret_cast<const clap_event_note *>(evHdr);
        pushToProcessQueue({ Event::Note, ClapEventNoteWrapper { evNote, CLAP_EVENT_NOTE_ON }});
    } break;
    { ~~~ }
```

Here, the focus is on efficiently enqueueing events as they are received from
the host. Each event is move-constructed into its respective wrapper class.
These specialized wrapper classes are essential in ensuring that the integrity
of the events is maintained, particularly focusing on the real-time requirements
under which they operate in.

```cpp
void CorePlugin::deactivate() noexcept
{
    dPtr->rootModule->deactivate();
    pushToMainQueue({Event::PluginDeactivate, ClapEventMainSyncWrapper{}});
}

bool CorePlugin::startProcessing() noexcept
{
    dPtr->rootModule->startProcessing();
    pushToMainQueue({Event::PluginStartProcessing, ClapEventMainSyncWrapper{}});
    return true;
}
```

This design demonstrates the ease of integration with the server. The
complexity of managing these events, including their timing and processing, is
adeptly handled by the underlying server-library. This allows the plugin
developer to focus on the plugin's functionality without delving into the
intricate details of event management and server communication.

### 4.2.4 Server API

The library discussed thus far lays the groundwork for users to develop their
projects. To fully leverage this library, users need a client that implements
the public API provided by the server. This is crucial for integrating the full
capabilities of gRPC and its language-independent protobuf interface into their
plugin and GUI development workflows. The API is accessible in the
`api/v0/api.proto` file, containing the essential interface for gRPC
communication. This setup mirrors the event structuring approach of CLAP, where
a header identifies the payload type:

```proto
enum Event {
  PluginActivate                 = 0;
  // { ~~~ }
  GuiCreate                      = 26;
  GuiSetTransient                = 27;
  GuiShow                        = 28;
  // { ~~~ }
  Param                          = 31;
  ParamInfo                      = 32;
  // { ~~~ }
  EventFailed                    = 42;
  EventInvalid                   = 43;
}
```

The `Event` enum specifies the type of payload being processed. It is essential
for the host to send the correct event type to ensure proper communication. The
`ServerEvent` message, the fundamental construct for outgoing streams showcases
this:

```proto
// Plugin -> Clients
message ServerEvent {
  Event event = 1;
  oneof payload {
    ClapEventNote note = 2;
    ClapEventParam param = 3;
    ClapEventParamInfo param_info = 4;
    ClapEventMainSync main_sync = 5;
  }
}
```

Currently, this API only supports a subset of the CLAP specification, primarily
note, parameter, and generic events. To further optimize the use the HTTP/2
channel with server streaming, we allow to batch multiple `ServerEvent`
messages into a single message:

```proto
message ServerEvents {
  repeated ServerEvent events = 1;
}
```

Working in tandem with the previously discussed event polling and dispatching
algorithm, which operates at a fixed maximum frequency, this allows for the
aggregation of events. A specific event payload can be seen for the
`ClapEventParam` message:

```proto
message ClapEventParam {
  enum Type {
    Value = 0;
    Modulation = 1;
    GestureBegin = 2;
    GestureEnd = 3;
  }
  Type type = 1;
  uint32 param_id = 2;
  double value = 3;
  double modulation = 4;
}
```

This message type wraps the CLAP specific parameter events. A parameter can have
four different states:

- `Type::Value` signifies a standard parameter update, typically triggered by
  user actions like adjusting a knob or slider in the GUI.
- `Type::Modulation` occurs when a parameter is influenced by external
  modulation sources, such as Low-Frequency Oscillators (LFOs).
- `Type::GestureBegin` and `Type::GestureEnd` are generally sent by the
  plugin's GUI to mark the start and end of manual parameter manipulation. These
  events can be utilized to enhance the audio rendering process.

Exploring the service definition reveals how gRPC generates the actual API,
building on the individual messages previously discussed. The current API is
defined as follows:

```proto
service ClapInterface {
  rpc ServerEventStream(ClientRequest) returns (stream ServerEvents) {}
  rpc ClientEventCall(ClientEvent) returns (None) {}
  rpc ClientParamCall(ClientParams) returns (None) {}
}
```

In this definition, three distinct RPC methods are outlined:

1. `ServerEventStream`: Serves as the primary communication channel from the
   plugin to its clients. Clients must initiate this stream to start receiving
   events.
2. `ClientEventCall` and `ClientParamCall`: Behaves as unary calls from the
   client, meaning they are executed once per message.

Originally, due to QtGrpc supporting only unary calls and server streaming,
these functionalities were adopted. While functional, this setup is not
entirely optimal for the use case at hand. In the future, the service
definition would benefit from supporting bidirectional streams and client-side
streams. Such streams provide a more efficient, sustained communication channel
compared to unary calls, which tend to be slower because they handle messages
on a one-at-a-time basis.

![CLAP RCI mono and multi client](images/clap-remote_mono_multi_client.png){#fig:mono_multi}

*@fig:mono_multi* visualizes the architecture of the CLAP RCI library. On the
left is the *mono-client* architecture, which means only a single client is
connected, or the DSO is running fully sandboxed, which means only a single
client will ever connect. On the right, the multi-client configuration unveils
the library's full capabilities, where numerous clients share the same
completion queues and threads, illustrating a more collaborative and dynamic
environment.

