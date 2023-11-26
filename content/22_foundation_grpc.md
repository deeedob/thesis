## 2.3 gRPC - Remote Procedure Calls

In today's fast-paced software development landscape, there is a growing need
for efficient communication between various software systems. Addressing this
demand, [gRPC](https://grpc.io/), which stands for *g Remote Procedure Calls*,
has risen as an open-source framework specifically tailored for this purpose.
If you want to find out about the current meaning of
[g](https://github.com/grpc/grpc/blob/master/doc/g_stands_for.md) in gRPC,
please consult the official documentation for clearance as it changes between
versions. Its standout features include impressive speed, compatibility with a
wide range of programming languages, and a steadily increasing adoption rate.
These attributes have positioned gRPC as a leading choice for inter-service
communication in contemporary software architecture.

![gRPC supported languages](images/grpc_langs.png){ width=66% }

### 2.3.1 Protobuf

Central to gRPC's efficiency and flexibility is the *Protocol Buffer*, often
abbreviated as protobuf. Developed by Google, protobuf is a serialization
format that efficiently converts structured data into a format optimized for
smooth transmission and reception.

#### The Rationale Behind Protobuf

Traditionally, data interchange between systems utilized formats such as XML or
JSON. While these formats are human-readable and widely accepted, they can be
quite verbose. This increased verbosity can slow down transmission and demand
more storage, leading to potential inefficiencies. In contrast, protobuf offers
a concise binary format, resulting in faster transmission and reduced data
overhead, positioning it as a preferred choice for many
developers[@performanceprotobufIoT].

#### Flexibility in Design

A standout feature of protobuf is its universal approach. Developers can
outline their data structures and services using an *Interface Definition
Language* (IDL). IDLs are used to define data structures and interfaces in a
language-neutral manner, ensuring they can be used across various platforms and
languages. Once defined, this IDL can be compiled to produce libraries that are
compatible with numerous programming languages[@protobufcasestudy], ensuring
coherence even when different system components are developed in diverse
languages.

#### Seamless Evolution

As software services continually evolve, it's essential that changes do not
disrupt existing functionalities. Protobuf's design flexibility allows for such
evolutions without hindering compatibility. This adaptability ensures newer
versions of a service can integrate seamlessly with older versions, ensuring
consistent functionality[@performanceprotobufIoT].

To encapsulate, protobuf's attributes include:

- A compact binary format for quick serialization and deserialization, ideal
  for performance-critical applications.
- Schema evolution capabilities, enabling developers to modify their data
  structures without affecting the integrity of existing serialized data.
- Strongly typed data structures, ensuring data exchanged between services
  adhere strictly to the specified schema, thus reducing runtime errors due to
  data discrepancies.

Consider the following illustrative protobuf definition:

```proto
!include examples/event.proto
```

This definition specifies an event data-type with specific attributes. Central
to processing this definition across different languages is the `protoc`
compiler. One of its standout features is the extensible plugin architecture.
As highlighted in **Chapter 1**, plugins are pivotal in enhancing the
capabilities of software tools. With the aid of [protoc
plugins](https://github.com/protocolbuffers/protobuf/blob/main/docs/third_party.md),
developers not only have the flexibility to generate code suitable for a wide
array of programming languages, but they can also craft their own plugins. An
example is seen in QtGrpc, where a custom plugin will translate the proto file
to c++ classes, which seamlessly integrate into the Qt ecosystem. Additionally,
plugins like [protoc-gen-doc](https://github.com/pseudomuto/protoc-gen-doc)
extend the capability of `protoc` by offering the convenience of producing
documentation directly from inline comments within the proto file.

![protoc extensions](images/protobuf_protoc.png){ width=77% }

For instance, to compile the protobuf file into a Python interface, use:

```bash
protoc --python_out=./build event.proto
```

The code outputted by protobuf may appear abstract as it doesn't directly
provide methods for data access. Instead, it utilizes metaclasses and
descriptors. Think of descriptors as guiding the overall behavior of a class
with its attributes.

To further illustrate, here's an integration with our generated class:

```python
!include examples/event.py
```

Executing this yields:

```bash
Python structure:
----------------
id: CREATED
name: "test"
description: "created event!"

Serialized structure:
--------------------
b'\x08\x01\x12\x04test"\x0ecreated event!'

JSON structure:
--------------
{
  "id": "CREATED",
  "name": "test",
  "description": "created event!"
}
```

This demonstration highlights protobuf's capabilities. It illustrates the
simplicity with which a type can be created, serialized into a compact binary
format, and its contents used by the application. The benefits of protobuf's
efficiency become even more pronounced when contrasted with heftier formats
like JSON or XML.

### 2.3.2 gRPC Core Concepts

#### Channels

in gRPC act as a conduit for client-side communication with a gRPC
service. A channel represents a session which, unlike HTTP/1.1, remains open
for multiple requests and responses.

#### Services

in gRPC define the methods available for remote calls. They're
specified using the protobuf language and serve as API between the server
and its clients.

#### Stubs

are the client-side representation of a gRPC service. They provide
methods corresponding to the service methods defined in the .proto files.

#### Calls and Streams

in gRPC provide the functionality for continuous data exchange between the
client and the server. The four primary RPC types are:

1. Unary Calls: This is the most common type, similar to a regular function call
   where the client sends a single request and gets a single response.
2. Server Streaming: The client sends a single request and receives multiple
   responses. Useful when the server needs to push data continuously after
   processing a client request.
3. Client Streaming: The client sends multiple requests before it awaits the
   server's single response. Useful in scenarios like file uploads where the
   client has a stream of data to send.
4. Bidirectional Streaming: Both client and server send a sequence of messages
   to each other. They can read and write in any order. This is beneficial in
   real-time applications where both sides need to continuously update each
   other.

![gRPC high-level overview](images/grpc_abstract.svg){width=77%}

### 2.3.3 Performance

Traditional HTTP protocols don't support sending multiple requests or receiving
multiple responses concurrently within a single connection. Each request or
response would necessitate a fresh connection.

However, with the advent of HTTP/2, this constraint was addressed. The
introduction of the binary framing layer in HTTP/2 enables such
request/response multiplexing. This ability to handle streaming efficiently is
a significant factor behind gRPC's enhanced performance.

![gRPC streaming [benchmarks](https://grafana-dot-grpc-testing.appspot.com/d/18sWlbd7z/grpc-performance-multi-language-on-gke-atupstream-master?viewPanel=8)](images/grpc_benchmarks.png){#fig:grpcbench}

As depicted in *@fig:grpcbench*, the performance benchmarks highlight the latencies
associated with various gRPC clients when interfaced with a C++ server
implementation. The results reveal a spectrum of latencies, with some as low as
100 microseconds (us) and others peaking around 350 us. Importantly, these
tests measure the Round Trip Time (RTT), representing the duration taken to
send a message and subsequently receive a response.
