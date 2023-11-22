# Chapter 3: Implementing Remote Plugins

## 3.1 Overview

This section transitions from defining the problem and introducing the
necessary tools to detailing our primary objective: facilitating a native
development experience. This is achieved by moving away from rigid methods,
such as the requirement for users to compile Qt in a separate namespace.
Additionally, ensuring a stable user experience across all desktop platforms is
a critical requirement.

![Traditional plugin architecture](images/plugins_traditional.png){#fig:plug_tradition}

*@fig:plug_tradition* outlines traditional audio plugin architectures, when
used with graphical user interfaces. As we know, multiple instances will be
created out of our DSO, hence enforcing reentrancy. The traditional approach is
to start the event loop of the user interface inside a dedicated thread,
resulting in a minimum of 2 threads that 'live' inside the DSO's process. This
technique can be seen in [JUCE](https://juce.com/), a well-known library for
creating plugin-standard agnostic plugins, as well as in various other open
source implementation.

Due to the limitation that it's not possible to start Qts event loop inside a
dedicated thread, we will simply execute the GUI plugin inside a seperate
process. Starting it inside a seperate process could be seen as expensive or
beeing slow and hard to incorporate because of the various issues with
IPC^[Inter Process Communication], but experimentation for this work has shown
that it's equally as good. First of all, modern computers should be able to
handle an extra process with ease and secondly this approach provides the
implementation with a safety state.

As an example, let's look at the case where we have the GUI and realtime thread
within the same process. A crash in the user interface would then also crash
the realtime thread. However when the GUI crashes, launched as seperate
process, the realtime thread wouldn't notice it. The implementation of IPC or
RPC techniques is a bit more delicate and indeed might result in additional
implementation efforts. On the other hand, the performance shouldn't be
affected critically by it. The messages we have to share between audio and gui
thread are very small in size, as some parameter changes or note events;
usually contain just a few bytes in size. When thinking about sharing the audio
data, techniques such as opening shared memory pools could be employed in order
to make it equally as fast. Although, the cost will appear on the difficulties
when implementing it. It's important to note that GUIs don't have to operate at
the same speed of the realtime thread.

Luckily for us, humans perception of fluent and smooth visuals is limited
by the perception of our eyes. The critical flimmer fusion frequency defines
this as follows:

> The critical flicker fusion rate is defined as the rate at which human
> perception cannot distinguish modulated light from a stable field. This rate
> varies with intensity and contrast, with the fastest variation in luminance one
> can detect at 50–90 Hz

However recent studies show that humans can perceive artifacts of up to 500 Hz[@https://www.nature.com/articles/srep07861].
Traditional refresh rates for desktop monitors ranges between 240 - 60 Hz which
would translate to 4.1 - 16 ms. This sets the boundary for the communication speed
we have to minimally target with our host application for a smooth user experience.


We will utilize the following techniques for integrating Qt GUIs into the CLAP standard:

1. Write a native gRPC server that handles connecting clients and handles all events
   to and from the client in a bidirectional fashion.
2. Write a client implementation that implements the server's API
3. Start the GUI as executable in a seperate process on the desktop and connect to the server upon start.
3. Remote control the GUI from the host for things like showing and hiding the GUI.
4. Optionally demand functionality from the host by a client.

using this in combination with QtGrpc's prooved successfull and showed a reliable, fast
and native experience on desktop platforms.

## 3.2 Remote Control Interface

In this section we will discuss the implementation for CLAP's remote control
interface (RCI) that sits on-top of the CLAP API to provide a thin plugin layer
abstraction to handle all communication in the background.

> Note: The code and techniques presented in the following sections are a
> snapshot of the current state of development and may not be optimal. Many
> aspects still remain to be improved although the core idea has been
> proofed successfull already.

To make the server efficiently handle multiple clients, we will actually apply
the same techniques as used in the *Q\*Application* objects that initially
prevented us from easy integration and more generally led to all of this
research. As mentioned in the [objective](#objective), static objects not only
posess problems but can also increase the overall architecture, and in this
case even performance, tremendously. As briefly mentioned in the
[clap-debugging](#debugging) section, the host *can* implement various plug-in
hosting modes. These put another constrain on this project as the server has to
operate fluently in all of those. The constraints for our server implementation
is as followed:

- All plugins that are contained within a CLAP live withing the same process
  address space. This allows us to share the server by re-using its
  capabilities for multiple client and reducing the required resources.
- Every plugin is loaded into a seperate process. While this is the most safe
  approach, it's also the resource heviest. Here we can't reuse the server and
  need to create a seperate one for every plugin which results in unused potential.

By not using Qt's event system, we gain more flexibility and resolve the
various problems in integrating it in a stable way. The approach at this state
seemed the most flexible, performant and stable solution. By using gRPC we
enable the language independent features of protobufs IDL, and clients are free
to implement the provided API by the server in whatever way they like. The
servers responsibillity is solely to pump all events to its clients and
integrate incoming events from them. This would allow for things like
controlling your plugin through a terminal application or embedded devices. It
just needs to support gRPC and a client implementation for the API.
Additionally we can easily link all our dependencies statically against our
library target to provide great portabillity. Having a portable library is
crucial, especially when it comes to plugins which often demand extra care to
be taken.

![CLAP-RCI Architecture](images/clap-rci_arch.png){ #fig:claprciarch }

*@fig:claprciarch* represents the overall architecture of the resulting server
library. At the bottom we have a host that supports the CLAP-interface. Just
above is the CLAP api that the host will use to control the plugin. The client
will also utilize this api but here we additionally include the CLAP-RCI
wrapper library to provide all necessary communication mechanism out of the
box. Then we have a client implementation of *CLAP-RCI*. Both communicate
through the HTTP/2 protocol of gRPC. Then the client implementation can provide
various devices that utilize this implementation.

### 3.2.1 Server Implementation

When designing IPC mechanisms, there are two types of distinct interaction
styles: synchronous and asynchronous. The synchronous communication is regarded
as a request/response interaction style, where we actively wait for the
response of the RPC. In an asynchronous communication style the interaction is
event driven to provide the flexibillity of customized processing which can
lead to better utilization of the underlying hardware.
[@https://ceur-ws.org/Vol-2767/07-QuASoQ-2020.pdf link this somehow]

At the foundation of gRPC C++ asynchronous API is the Completion Queue. To
state the docs:

```cpp
/// grpcpp/completion_queue.h
/// A completion queue implements a concurrent producer-consumer queue, with
/// two main API-exposed methods: \a Next and \a AsyncNext. These
/// methods are the essential component of the gRPC C++ asynchronous API
```
The gRPC C++ library provides three ways for creating a server:

1. **Synchronous API**: The most straight forward way. All event handling and
   synchronization is abstracted away to provide a straightforward approach of
   implementing the server. Client call waits for the server to respond
2. **Callback API**: An abstraction to the asynchronous API to provide a more
   straightforward solution to the complexities of the asynchronous API.
   ^[Introduced in proposal: [L67-cpppcallback-api](https://github.com/grpc/proposal/blob/master/L67-cpp-callback-api.md)]
3. **Asynchronous API**: The most complex but also most flexible way of creating
   and managing RPC calls. Allows for full control over its threading model for the cost
   of a more complex implementation. E.g. RPC polling must be explicitly handled and lifetime
   management is complicated.

Since our way of using gRPC is already quite unique and differs from
traditional microservice architectures, and we're in a constrained environment
the asynchronous API has been chosen to provide a more flexible approach to our
library design. From here the following code flow will be used:

![Lifetime of CLAP-RCI](images/clap-rci_sequence_diag.svg){#fig:claprcilifetime}

*@fig:claprcilifetime* goes through the lifetime cycle of the server. Upon the
first plugin creation the server will be created and started by the static
server controller . When a plugin instance now executes its GUI, we will supply
the needed command line arguments, as the servers address and the plugin
identifier along the way. The GUI process must then try to connect to the
server. The server side then handles the connecting GUI client and starts its
internal polling callback mechanism. With this polling mechanism all events for
every plugin that has active clients will be polled and sent to all of the
clients. It's important to note that even if not displayed in the figure above,
the server handles possibly multiple plugin instances, which again can hold
multiple clients per plugin instance. When the last client has disconnected and
all server-side-streams have been closed, the event polling will be stopped.
This was implemented to increase performance and reduce resource usage when no
client is connected. Finally, when the last plugin got unloaded by the host and
we're about to unload the DSO, we will stop the server.

To identify or 'register' a specific plugin instance to the context of the
static server handler, we use a hashing algorithm known as
[MurmurHash](https://en.wikipedia.org/wiki/MurmurHash). This is needed, because
the GUI that connects has to map to its respective plugin instance. This hash
value will then be supplied as command line argument when executing the GUI and
is also used as the *key* value when inserting the plugin into a map.

```cpp
// core/global.h

// MurmurHash3
// Since (*this) is already unique in this process, all we really want to do is
// propagate that uniqueness evenly across all the bits, so that we can use
// a subset of the bits while reducing collisions significantly.
[[maybe_unused]] inline std::uint64_t toHash(void *ptr)
{
    uint64_t h = reinterpret_cast<uint64_t>(ptr); // NOLINT
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccd;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53;
    return h ^ (h >> 33);
}
```

Furthermore, to provide a means of communication with the plugin instances
we bundle all communication logic inside the class `SharedData`. The
name may not be optimal and will probably be changed to something
like *Pipe* but the responsibillity should be clear. This class contains
the polling mechanism and the non-blocking and wait free queues to exchange
events with the plugin. We create a *SharedData* instance for every plugin.
A section of the public API looks like the following:

```cpp
// server/shareddata.h
class SharedData
{
public:
    explicit SharedData(CorePlugin *plugin);

    bool addCorePlugin(CorePlugin *plugin);
    bool addStream(ServerEventStream *stream);
    bool removeStream(ServerEventStream *stream);

    bool blockingVerifyEvent(Event e);
    bool blockingPushClientEvent(Event e);
    void pushClientParam(const ClientParams &ev);

    auto &pluginToClientsQueue() { return mPluginProcessToClientsQueue; }
    auto &pluginMainToClientsQueue() { return mPluginMainToClientsQueue; }
    auto &clientsToPluginQueue() { return mClientsToPluginQueue; }
~~~
```

A section of the constructor starting the server and setting up all crucial
information looks like the following:

```cpp
// plugin/coreplugin.cpp
~~~
    auto hc = ServerCtrl::instance().addPlugin(this);
    assert(hc);
    dPtr->hashCore = *hc;
    logInfo();
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

Now lets explore the internal gRPC server implementation a bit more. As already
mentioned we use the asynchronous API to provide the most flexible solution.
For the massive amount of code that is required to implement the server, we will
gain a lot of flexibillity and control over the threading model.

> With great power comes great responsibillity.

The following code snippet shows the server constructor and the creation of the
completion queues.

```cpp
// server/server.cpp
    // Specify the amount of cqs and threads to use
    cqHandlers.reserve(2);
    threads.reserve(cqHandlers.capacity());

    // Create the server and completion queues; launch server
    { ~~~ } // Simplified for brevity

    // Create handlers to manage the RPCs and distribute them across
    // the completion queues.
    cqHandlers[0]->create<ServerEventStream>();
    cqHandlers[1]->create<ClientEventCallHandler>();
    cqHandlers[1]->create<ClientParamCall>();

    // Distribute completion queues across threads
    threads.emplace_back(&CqEventHandler::run, cqHandlers[0].get());
    threads.emplace_back(&CqEventHandler::run, cqHandlers[1].get());
```

In the code snippet above we can already see the power we gain by using the
async API. We can create multiple completion queues and distribute them individually
across threads. This allows us to have full control over the threading model.
We will attach the completion queue to handle all server communication towards
the clients at position `0` and handle all client communication at position `1`.

We have three individual *rpc-tag* handler:

1. `ServerEventStream` is the outgoing stream from the plugin instance to its clients.
2. `ClientEventCallHandler` handles important gRPC unary calls from the client.
3. `ClientParamCall` also handles gRPC unary calls but solely for parameter changes.

We differentiate between the two types of calls, because the parameter calls
from the client have to be supplied directly to the realtime audio thread. The
event calls can be blocking and don't have to be handled in the realtime. In
fact, we use them for critical events that need a verification from the client
in the gui creation process.

The completion queues, as already mentioned, are FIFO queues which operate with
a tag system. The tag system is used to identify the type of event that is
being processed. We enqueue tags which correspond to a specific instruction and
receive them at a later point in time when the gRPC system has decided to
process them.

```cpp
void CqEventHandler::run()
{
    state = RUNNING;

    void *rawTag = nullptr;
    bool ok = false;
    // Block until the next event is available in the completion queue.
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
            // Create a new instance to serve new clients
            // while we're processing this one.
            parent->create<ServerEventStream>();
            // Try to connect the client. The client must provide a valid hash-id
            // of a plugin instance in the metadata to successfully connect.
            if (!connectClient()) {
                state = FINISH;
                stream.Finish({ grpc::StatusCode::UNAUTHENTICATED,
                    "Couldn't authenticate client" }, toTag(this));
                return;
            }
            sharedData->tryStartPolling()) {
                SPDLOG_INFO("Already polling events!");
            }
            state = WRITE;
            SPDLOG_INFO("ServerEventStream new client @ {} connected", toTag(this));
        } break;
~~~
```

The code snippet above shows the state machine of the `ServerEventStream`. This
specific section gets executed when new clients connect to the server. Hidden
behind the `connectClient()` function is the authentication process:

A client has to provide a valid hash-id of a plugin instance to successfully
connect. If the authentication fails, the stream will be finished. Here we can
also see how the tag based system works. When we call `stream.Finish` we
provide the tag with the `toTag` function, which simply `reinterpret_cast`s
the `this` pointer to a `void` pointer. Because we changed the state to
`FINISH`, the next time the completion queue processes this tag, it will call
the `process` function again, but in a updated state. Finally, when the client
has successfully connected, we start the polling mechanism to supply the
events.

![Server Trace](images/clap-rci_server_trace.png){#fig:claprcitrace}

*@fig:claprcitrace* showcases a section of the tracing when the project is
compiled in debug mode. Here we simply print some debugging information to
visualize the flow of the server. It can get increasingly difficult to debug
the server, because the completion queues are running in seperate threads, and
multiple plugins with again multiple clients can be connected at the same time. This
ensures to keep the development and debugging experiences sane and robust.

### 3.3.2 Event Handling and Communication

One of the most important and complex aspects of this library is the event
handling. We have to ensure that the events are processed in a timely manner,
that we comply with the realtime constraints, that is, serving the events in a
deterministic and predictable way. We also have to manage the lifetime of the
incoming RPC calls, synchronize access to the shared resources and if that's
not enough we also have to find a technique to correctly invoke gRPC calls.

Let me elaborate the last point a bit more. The gRPC library provides various
methods to communicate with the underlying channels, like we have seen with the
`Finish` method to close a channel, we also have `Read` or `Write` methods.
These methods are the public interface to effectively receive and write
messages. The tricky part is that for the asynchronous API, those calls have to
be enqueued into the completion queues manually. This means that we can't call
them from the plugin directly multiple times to communicate with our clients.
We have to enqueue them with a tag and then wait for the completion queue to
process them to enqueue another one.

> Completion Queues can only have one pending operation per tag.

Now this wouldn't be such an issue if we would've only had a single plugin
instance that communicates with a single client, but since the server supports
**N** plugins with **M** clients attached to it this approach doesn't scale
well, in fact it doesn't scale at all.

Now a good and efficient approach would be to centralize this event
distribution at a common place, to scan through all messages that are enqueued
from the plugin instances and then send them over to the clients, all at once.
This technique would dramatically increases our quality of life. On the plugin
side we don't have to worry about all the synchronization and threading issues.
We register our plugin with the server and retrieve the handle to the
`SharedData` object. Then we can just push all events to the FIFO queues
contained inside the SharedData and everything from there will be automatically
handled by the server.

For this to work, we need to implement the event polling mechanism. Since I
didn't want to introduce another independent thread to handle this mechanism
and also avoid the overhead of thread switching and the tricky synchronization
issues, I decided to re-use the completion queues for this task. The completion
queues are already running and hosting internal thread pools. They additionally
contain the capability to communicate with the clients channels.
We have to enqueue our own custom operation into the completion queue and let it
handle the lifetime and the next invocation for us. Luckily, the gRPC library
provides a somewhat lesser known feature for that, the `Alarm` class.

```cpp
  /// grpcpp/alarm.h
  /// Trigger an alarm instance on completion queue \a cq at the specified time.
  /// Once the alarm expires (at \a deadline) or it's cancelled (see \a Cancel),
  /// an event with tag \a tag will be added to \a cq. If the alarm expired, the
  /// event's success bit will be true, false otherwise (ie, upon cancellation).
  //
  // USAGE NOTE: This is frequently used to inject arbitrary tags into \a cq by
  // setting an immediate deadline. Such usage allows synchronizing an external
  // event with an application's \a grpc::CompletionQueue::Next loop.
  template <typename T>
  void Set(grpc::CompletionQueue* cq, const T& deadline, void* tag)
```

As stated in the *usage note* section, this is exactly what we need. We implemented
the `CqEventHandler` class to abstract and manage the completion queues for us.
Here we store all pending (active) tags:

```cpp
    // server/cqeventhandler.h
~~~
    std::map<std::unique_ptr<eventtag>, grpc::alarm> pendingalarmtags;
    std::map<std::uint64_t, std::unique_ptr<eventtag>> pendingtags;
~~~
```

The `pendintags` map stores all currently active streams or calls for
communication and the `pendinalarmtags` map stores all currently active alarms.
As already stated, our base class for handling tags called `EventTag` is,
despite it beeing a base class for the other tag implementations, also capable
to handle the `process(bool ok)` callback:

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

The process function can be overriden by the derived classes as we have seen
for the `ServerEventStream` implementation, but by default it will call
the `func` member. With this approach we can inject custom functions into
our alarmtag-based-subsystem and is the foundation for the event polling.

When the completion queue processes such an alarm tag it will invoke the
`process` function as for every tag, but here the default implementation will
be called:

```cpp
// server/tags/eventtag.cpp
void EventTag::process(bool ok)
{
    (*func)(ok);
    kill();
}
```

We just invoke the function and then destroy the tag immediately. The current
implementation works with single-shot alarm tags and could be potentially
be a target for future optimizations. The crucial interface to interact
with this mechanism is inside the `CqEventHandler` class:

```cpp
// server/cqeventhandler.cpp
bool CqEventHandler::enqueueFn(EventTag::FnType &&f, std::uint64_t deferNs /*= 0*/)
{
    auto eventFn = pendingAlarmTags.try_emplace(
        std::make_unique<EventTag>(this, std::move(f)),
        grpc::Alarm()
    );

    if (!eventFn.second) {
        SPDLOG_ERROR("Failed to enqueue AlarmTag");
        return false;
    }

    if (deferNs == 0) { // Execute the function immediately.
        eventFn.first->second.Set(
            cq.get(), gpr_now(gpr_clock_type::GPR_CLOCK_REALTIME), toTag(eventFn.first->first.get())
        );
    } else { // Otherwise, defer the function. Note that this is using a syscall
        const auto tp = gpr_time_from_nanos(deferNs, GPR_TIMESPAN);
        eventFn.first->second.Set(cq.get(), tp, toTag(eventFn.first->first.get()));
    }

    return true;
}
```

The `enqueueFn` function takes the callable function `f` as first parameter and
an optional second parameter for the time, the call should be deferred. We
first emplace the EventTag and Alarm objects into the map. We store those tags
because when the desctructor of the `grpc::Alarm` object is beeing invoked, it
would call the `Cancel` function which would in return immediately enqueue it
inside the completion queue which is *not* what we want. The completion queue
should handle the correct invocation for us and we only want to cancel tags
manually when we shutdown the server. Then we either defer the call or execute
it immediately.

With this approach we can safely integrate the events from the plugin instances
to the corresponding completion queue handlers, and furthermore provide a
scallable and realiable solution to interact with the gRPC interface from
external threads.

Now lets explore the core mechanism that runs behind the scenes and is responsible
for the communication with the clients, the polling mechanism. As already seen
at @lst:evstreamprocess, upon successfull connection we invoke the `tryStartPolling`
function. The definition of this function looks like the following:

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

If the callback is already running we return false. Otherwise we will start it
by enqueueing the `pollCallback` function to the function mentioned earlier at
@lst:cqenqueue. The default polling frequency for `mPollFreqNs` is **5'000**
nanoseconds, which translates to **5** microseconds. Which means that every
5 microseconds we will re-enter the function and check for new events.

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

To ensure that we don't poll for events when there are no clients connected, we
automatically stop the polling mechanism. When clients disconnect, either by
themself or by the server, the `streams` container will be updated.


```cpp
~~~
    // Consume events from the queues
    const auto nProcessEvs = consumeEventToStream(mPluginProcessToClientsQueue);
    const auto nMainEvs = consumeEventToStream(mPluginMainToClientsQueue);

    if (nProcessEvs == 0 && nMainEvs == 0) {
        // We have no events to send, so we can just enqueue the next callback with an increased backoff.
        mServerStreamCq->enqueueFn([this](bool ok){ this->pollCallback(ok); }, nextExpBackoff());
        return;
    }
~~~
```

Then, if the initial checks were successfull, we iterate over all plugin instances and
consume their events. The `consumeEventToStream` function will process the CLAP specific
events and translate them into gRPC messages. We've split the event-container into two
queues, one for the realtime thread and one for the main thread. This is due to the
strict realtime constraints that we have to comply with. The queues are preallocated
and have a fixed size. Since they are wait free, they internally override old messages
when the queue is full. This is a tradeoff that we have to take, because we can't block
on the realtime thread. When there are no events to send, we will enqueue the next
callback with an increased exponential backoff. This is to reduce the amount of CPU cycles that
the server consumes when there are no events to send.

![Exponentiall Backoff](images/clap-rci_exp_backoff.png){#fig:expbackoff}

*@fig:expbackoff* shows the graph of the exponential backoff. This is roughly
the visualization of the function that we use to calculate the next backoff. It
is devided into three sections:

1. To the left, in red, is the section involved with high occurences of events.
   This means one or multiple plugins are actively in use. The events are
   processed as fast as possible.
2. In the middle, in orange, means that the previous section has finished
   and is 'cooling down' or we have a medium destribution of events.
3. To the right, in green, means that there are no events to send and we can
   increase the backoff to reduce the CPU cycles that the server consumes.

Any event will reset the backoff to the initial value (in the red section)
again. This aligns mostly with the real-world use case. Typically you would
adjust some parameters or receive some notes from the host. This would result
in a high amount of events and would command the polling callback to operate at
full speed. When the user stops actively using any plugins we go into **stand-by**
mode.

![Exponentiall Backoff Server Trace](images/clap-rci_exp_backoff_trace.png){#fig:expbackofftrace}

*@fig:expbackofftrace* shows this behavior. Here we added a additional trace
message to the previous code section. We've increased the boundaries to
highlight the behavior of the exponential backoff. We can see that on the
moment we receive a event, the backoff is reset to it's default value and then
quickly increases again.

```cpp
~~~
    // If we reached this point, we have events to send.
    bool success = false;
    for (auto stream : streams) { // For all streams/clients
        if (stream->sendEvents(mPluginToClientsData)) // try to pump some events.
            success = true;
    }

    { ~~ } // Simplified for brevity

    // Succefully completed a round. Enqueue the next callback with
    // regular poll-frequency and reset the backoff.
    mPluginToClientsData.Clear();
    mCurrExpBackoff = mPollFreqNs;
    mServerStreamCq->enqueueFn([this](bool ok){ this->pollCallback(ok); }, mPollFreqNs);
    SPDLOG_TRACE("PollCallback has sent: {} Process Events and {} Main Events", nProcessEvs, nMainEvs);
~~
```

This final section shows the actual sending of the events. We iterate over all
connected streams (the `ServerEventStream`s) and try to send the events.
Finally, if everything was successfull, we enqueue the next callback with the
default poll frequency.

### 3.2.3 CLAP API Abstraction

In the previous section, we've already seen how the server resembles and how
the inidividual core components interact with eachother. Additionally with the
knowledge gained from the [CLAP](#the_clap_audio_plugin_standard) section, we
know that the CLAP standard provides an extension based interface for the
plugin, to provide its own custom functionality. We will focus here on the
relevant sections for the communication between the server and the plugin
and avoid the implementation details. The provided API abstraction utilizes the
[clap-helpers](https://github.com/free-audio/clap-helpers), toolkit provided by
the CLAP team. It is a collection of various tools to simplify the development.
One of them is the `Plugin` class, which sits inbetween the C-API and the
context specific plugin instances. It provides a thin abstraction layer utilizing
trampoline functions to forward the calls to the plugin instance.

The `CorePlugin` class is utilized to provide a generic default implementation, with the
server specific functionality included. Users of this class can then override specific
functions of the virtual interface to customize its functionality.

We use two independent communication channels with the client's GUI. One is the
non blocking channel, which is used for time critical interaction. Here we enqueue
the Events in a UDP^[User Datagram Protocol] like fashion with a *fire and forget*
approach. There is no point in waiting for the client to receive the message. The events
received from the host inside the process callback deliver a snapshot of the current
representation of it's value. They are often frequently updated and given the high
frequency the process callback operates in we can't afford to wait for the client response.

There are cases however, where we need to wait for the client response. One of them is
the GUI extension. Since our approach of integrating the GUI is to start it as a seperate
process, we will face additional synchronization issues. The general idea unfolds as:

1. Provide the parameters hashed id of the plugin instance together with the
   server address to the GUI
2. Execute the GUI executable with platform specific APIs
3. The GUI connects to the server and verifies the hash id
4. For every subsequent gui event from the host, as to show or hide the GUI, we
   have to wait for the client response

> Note: The CLAP specification doesn't enforce a strict design and provides
> some freedom to the host application. Some hosts may choose to destroy the
> gui after every hide event of the plugin inside their UI, while some other
> hosts may choose to keep the GUI alive and just hide it.

Special care has therefore been taken to ensure that the GUI creation process can
handle those cases. The public interface for those methods are defined as followed:

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
When an user would return false in an override of his subclassed instance this
extension would be disabled, and thus also be ignored by the host application.
In the GUI creation we have to synchronize ourself with the client in order
to ensure that the GUI creation and verification succeeded.

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

Upon entering the default implementation of the `guiCreate` function we first
perform various sanity checks, to verify that we can safely execute the GUI.
We then create a new `ProcessHandle` object, which is a wrapper around the
platform specific process APIs. We have to provide platform specific
implementations here since the C++ standard doesn't have any notion of a
*process*. There this class provides a unified interface to interact
with the platform specific process APIs. On windows we use the `CreateProcess`
API from the `Kernel32.dll` library. For linux and mac we use a POSIX compliant
implementation. A section of the public interface looks like the following:

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

This interface will then be used to start the GUI executable, terminate
or wait for it to finish. We use it like the following inside the `guiCreate`
function:

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

As briefly mentioned in the [server implementation](#server_implementation)
section earlies we use a hashing algorithm to identify the plugin instance. This hash
will be provided as argument to the executable and *must* be handled by the client and
also provided when starting communication.

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

We then push the `Event::GuiCreate` event to the main queue and wait for the
client to respond. If the client doesn't respond in time we will terminate the
process and signal the failure. Otherwise we can continue the the GUI creation process.
A host would now provide a system-window handle to the plugin instance. GUI plugins
must integrate this window handle by **re-parenting** themselves with this window.
This is a common technique to integrate GUIs into other applications, by providing
the context for the GUI to be created in:

```cpp
bool CorePlugin::guiSetTransient(const clap_window *window) noexcept
{
    assert(dPtr->sharedData->isPolling());
    if (!window) {
       SPDLOG_ERROR("clap_window is null");
       return false;
    }

    { ~~~ }

    pushToMainQueueBlocking( {Event::GuiSetTransient, ClapEventMainSyncWrapper{ wid } });
    if (!dPtr->sharedData->blockingVerifyEvent(Event::GuiSetTransient)) {
        SPDLOG_ERROR("GuiSetTransient failed to get verification from client");
        return false;
    }
    return true;
}
```

A simplified implementation of the `guiSetTransient` shows the same patterns
as the `guiCreate` function. When we look at the non-blocking communications
of the event handling we can see a much simpler implementation:

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

In this simplified example we can see that we only have to enqueue the event as
received from the host. And move-construct it insidethe corresponding wrapper
function. This wrapper classes ensure that the event can be properly serialized
and deserialized by the gRPC library, while also providing a convenient
conversions to and from the CLAP C-API. Some other implementation can therefore
be extremely simple using this approach:

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

These functions only contain two lines of code. The first line calls
the rootModule, which is the "audio processing" module of the plugin
that implements this interface and contains the actual DSP logic. The
next line pushes the event to the queue to be processed by the server.

### 3.2.4 Server API

The resulting library presented up to this point is the foundation for
potential users of it to build their project upon. However, when a user would
want to utilize the full potential of this library he would also need a client
that implements the public API, that the server provides. To harness the full
potential of gRPC and their language independent protobuf interface, users of
this library would need a client implementation of the server's interface to
receive any events and integrate them into their plugin and GUI creation
process. For that, the api is available in the `api/v0/api.proto` file and
includes the crucial interface for the gRPC communication. Similar to how
CLAP is structuring their events, with a header indicating the payload, the
protobuf uses a similar behavior:

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

The `Event` enum indicates the type of event that is currently received and
must be sent by the host in order to guarantee a correct communication. We
can see that for the `ServerEvent` message, which is the foundational message for outgoing streams
from the plugin to theirs clients:

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

Here we utilize the `Event` type to indicate the message type. Then we use
the `oneof` protobuf keyword, since a message can only contain one payload.
The `oneof` specifier can be thought of as a union type or `std::variant` in
modern C++. The `event` field could be thought to be superfluous, since we
could also check the payload type to determine the event type. However, this
approach provides a more robust and faster way to determine the event type,
and additionally provides better flexibility for future extensions. The server
at the moment only supports a subset of the CLAP specification and can only
provide note, parameter, and some information events. In order to optimize
the usage of the HTTP/2 protocol in combination with the server streaming, we
bundle multiple events into a single message:

```proto
message ServerEvents {
  repeated ServerEvent events = 1;
}
```

In combination with the event polling machnisem presented earlier. Which operates
at a fixed maximum frequency, we can bundle multiple events into a single message.
The `repeated` keyword is utilized for that. A specific event payload can be
seen for the `ClapEventParam` message:

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

- `Type::Value` indicates a regular parameter update. This happens when the
   user moves a knob or slider in their GUI or inside the Hosts GUI.
- `Type::Modulation` is received when the parameter is modulated with external
   modifiers as for example LFOs^[Low-frequency oscillators] or other modulation sources.
- `Type::GestureBegin` and `Type::GestureEnd` will usually be sent by the plugins GUI
   implementation to indicate the manual interaction with the parameter. This can then
   be used to improveLFO the audio rendering.

These messages will then be serialized into a binary protobuf message and assure
fast and reliable communication between the server and the clients. Finally, lets
have a look at the defined service. While we've presented all the individual
messages, gRPC uses the service definition to generate the actual API. The current
provided API looks like the following:

```proto
service ClapInterface {
  rpc ServerEventStream(ClientRequest) returns (stream ServerEvents) {}
  rpc ClientEventCall(ClientEvent) returns (None) {}
  rpc ClientParamCall(ClientParams) returns (None) {}
}
```

Currently there are three different RPC calls defined. The `ServerEventStream`
is the main communication channel from the plugin to the clients. When a client
connects, he has to initiate this stream in order to receive any events. The
`ClientEventCall` and `ClientParamCall` are unary calls for the client, which
means that they are invoked once per message. Since QtGrpc initially only provided
unary calls and server streaming, we had to utilize the available
functionality, which is by far not optimal for this usecase. Ideally, in the
future this service definition should support bidirectional streams and client
side streams, since an long-lived communication channel through a stream is
always faster then using unary calls. A client implementation can now implement
this basic service and would then be able to communicate with the server.

