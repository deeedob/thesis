# Chapter 5: Conclusions

## 5.1 Event System Performance Analysis

This section focuses on evaluating the performance of the CLAP-RCI event
system. A series of tests were conducted to simulate a high volume of events
and measure the system's efficiency in processing and delivering these events
to the client. The key steps in the benchmarking process were:

1. **Client Initialization**: The client, running as a separate process,
   connects and initiates event polling, marking the event system as
   operational.
2. **Event Generation and Dispatch**: Events are generated and enqueued in the
   `pluginToClientsQueue`. These steps involve:
    - Wrapping events in a specific type for processing.
    - The polling callback aggregates these events into a gRPC streaming message.
    - The message is serialized and transmitted via the HTTP/2 channel.
3. **Event Reception and Processing**: The client receives the serialized
   messages, converts them back into gRPC format, and performs trivial
   computations with the data.

To accurately gauge the time taken for a complete cycle of event processing,
from generation to client reception, timestamped messages were introduced at
the beginning and end of the event stream. The `ServerEvents` message format
was extended with an optional `TimestampMsg` field to facilitate this
measurement:

```proto
message TimestampMsg {
  int64 seconds = 1;
  int64 nanos = 2;
}
~~~
message ServerEvents {
  repeated ServerEvent events = 1;
  optional TimestampMsg timestamp = 2;
}
```

The performance of the CLAP-RCI event system was assessed under various load
conditions. The aim was to examine the system's behavior with different levels
of queue occupancy, ranging from tightly packed message streams in high-load
situations to streams with fewer messages.

To push the system's limits, the polling frequency was set to **0**. This
setting means the system would operate without any imposed delays between
callbacks. This setup allowed for a direct evaluation of the system's
performance under continuous, high-speed operation.

```cpp
// server/shareddata.h
static constexpr uint64_t mPollFreqNs = 0;
```

The benchmark then works with nested loops, where we try to occupy the
event processing in a more or less controlled manner:

```cpp
// tests/bench/bench_clap_rci.cpp
~~~
    child.startChild();
    while (sharedData->nStreams() <= 0) ; // Busy wait

    clockedEvent();
    for (uint64_t i = 0; i < iterations; ++i) {
        for (uint64_t k = 0; k < eventsPerIteration; ++k) {
            while (!sharedData->pluginToClientsQueue().push(ServerEventWrapper(TestEvent))) ;
        }
    }
    clockedEvent();
~~~
```

In this setup:

1. The client is first initiated and the system awaits its successful
   connection.
2. A time-stamped event marks the start of event generation.
3. The loop iterates `i` times, each iteration attempting to enqueue `k` events
   into the queue.
4. The events are wrapped in `ServerEventWrapper` and pushed to
   `pluginToClientsQueue`.

The experiments conducted with this structure indicated a degree of control
over the utilization of the stream size. However, it was observed that it is
not always guaranteed to enqueue exactly `k` events before the event callback
intervenes and clears the queue. This aspect of the benchmark highlights the
dynamic and somewhat unpredictable nature of event handling under different
load scenarios.

In the client-side processing of the benchmark test for the CLAP-RCI event
system, the focus shifts to reading the incoming stream and marking time-stamps
similarly to the server. The client-side code proceeds as follows:

```cpp
~~~
    Stamp tBegin = Timestamp::stamp();
    while (stream->Read(&serverEvents)) {
        bytesWritten += serverEvents.ByteSizeLong();
        messageCount += static_cast<uint64_t>(serverEvents.events_size());
        if (serverEvents.has_timestamp()) {
            stamps.emplace_back(
                serverEvents.timestamp().seconds(), serverEvents.timestamp().nanos()
            );
        }
    }
    Stamp tEnd = Timestamp::stamp();
~~~
```

In this sequence:

1. The client begins by recording the initial timestamp (`tBegin`).
2. It then continuously reads from the stream, counting the number of messages
   received (`messageCount`) and the total bytes (`bytesWritten`).
3. For each message with a timestamp, the client stores these in the
   container (`stamps`).
4. The reading process continues until the host terminates the connection.
5. Finally, the client marks the end timestamp (`tEnd`).

Following the stream reading and data accumulation, the client calculates
several metrics:

```cpp
~~~
    const auto serverRtt = tEnd.delta(stamps[0]);
    cout << "Server RTT: " << serverRtt.toDouble() << " s" << endl;
    cout << "Mbps: " << (static_cast<double>(bytesWritten * 8)) /
                            serverRtt.toDouble() / 1e6 << endl;
~~~
```

The benchmark test can be configured to run with varying values for
`iterations` and `eventsPerIteration`, evaluating the performace
under various load conditions.

In the benchmark results for the CLAP-RCI event system, two charts display the
performance under different loads. Each benchmark invocation ran 1,000
iterations, with each iteration handling 1 to 64 messages. The key findings are
presented in graphs showing average time per message and average throughput.

![Benchmark results: Average time per message](images/benchmark_ns_msg.svg)

The first graph shows the average time per message in nanoseconds. Performance
initially drops to about 330 ns when the queue is underutilized with only one
event. However, as more events are added to the queue, performance improves,
with the best average time around 200 ns achieved at 30 events in the stream.

![Benchmark results: Average throughput](images/benchmark_throughput.svg)

The second graph illustrates the throughput measured in megabits per second
(Mbps). It mirrors the previous graph's trend, peaking at around 45 messages
per stream with a throughput of approximately 1300 Mbps or 1.3 Gbps. Beyond
this point, the throughput stabilizes, suggesting an optimal message count for
efficient processing.

## 5.2 Final Words on Development Experiences

The development of the CLAP-RCI library and its client-side Qt implementation
has proven successful in facilitating efficient communication between the
plugin side and user interfaces. By opting to launch the graphical user
interface as a separate process, many of the challenges previously encountered
were effectively resolved. The example plugins created using the Qt CLAP client
interface demonstrate a seamless native development experience, similar to
crafting a typical graphical user interface. This approach simplifies the
complexity of the already streamlined CLAP interface and provides a robust
default for users, allowing them to customize their interaction with the events
they need to handle.

The decision to separate the problematic (Qt) component from the rest of the
system has proved its worth, enabling the server implementation to leverage the
language-agnostic capabilities provided by gRPC and protobuf. This separation
of the GUI, or more accurately the clients, introduces a novel way to interact
with audio plugins, where visualization responsibilities are entirely deferred
to the client, freeing the server from these duties. Although currently
limited, future development could enable multiple clients to connect to the
same plugin simultaneously. This would allow for remote control of the plugin
from various devices such as smartphones, tablets, and computers, expanding the
range of interactive possibilities.

However, it's important to acknowledge that these libraries are still evolving
and are not yet at an optimal stage of development. The API requires further
refinement to offer a consistent and stable user experience, not even touching
aspects like binary compatibility. Additionally, several features are yet to be
implemented. Despite these areas for improvement, this research has validated
the effectiveness of the methods presented, laying a solid foundation for
future enhancements and investigations in this field.

