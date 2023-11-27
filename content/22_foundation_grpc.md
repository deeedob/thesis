## 2.3 gRPC - Remote Procedure Calls

In the domain of software development, effective communication between various
systems is critical. *gRPC*, which stands for **Remote Procedure Calls**, is an
open-source communication framework. The meaning of the "g" in gRPC has varied
over time, and for the latest interpretation, it's recommended to refer to the
official gRPC repository
^[https://github.com/grpc/grpc/blob/master/doc/g_stands_for.md]. gRPC is
notable for its speed and its support for multiple programming languages,
making it a practical choice for facilitating communication between different
software components in modern software architectures.

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
Language* (IDL hereon). IDLs are used to define data structures and interfaces
in a language-neutral manner, ensuring they can be used across various
platforms and languages. Once defined, this IDL can be compiled to produce
libraries that are compatible with numerous programming
languages[@protobufcasestudy], ensuring coherence even when different system
components are developed in diverse languages.

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
As highlighted in the [plugins](#plugins) section, they are pivotal in
extending the capabilities of software tools. With the aid of protoc
plugins^[https://github.com/protocolbuffers/protobuf/blob/main/docs/third_party.md],
developers not only have the flexibility to generate code suitable for a wide
array of programming languages, but they can also craft their own plugins. An
example is seen in QtGrpc, where a custom plugin will translate the proto file
to C++ classes, which seamlessly integrate into Qt's ecosystem.

![protoc extensions](images/protobuf_protoc.png){ #fig:protocplugs width=77% }

For instance, to compile the protobuf file into a Python interface, use:

```bash
protoc --python_out=./build event.proto
```

The code outputted by protobuf may appear abstract as it doesn't directly
provide methods for data access. Instead, it utilizes metaclasses and
descriptors. To further illustrate, here's an integration with our generated
class:

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

JSON structure:
--------------
{
  "id": "CREATED",
  "name": "test",
  "description": "created event!"
}

Serialized structure:
--------------------
b'\x08\x01\x12\x04test"\x0ecreated event!'
```

This demonstration highlights protobuf's capabilities. It illustrates the
simplicity with which a type can be created, serialized into a compact binary
format, and its contents used by the application. The benefits of protobuf's
efficiency become even more pronounced when contrasted with heftier formats
like JSON or XML.

### 2.3.2 gRPC Core Concepts

Built upon the advanced features of
HTTP/2^[https://grpc.io/blog/grpc-on-http2/], gRPC introduces a more dynamic
and efficient way of communication between client and server. This reliance on
HTTP/2 allows for persistent connections and improved performance.

![gRPC high-level overview^[https://grpc.io/docs/what-is-grpc/introduction/]](images/grpc_abstract.svg){width=77%}

#### Channels

in gRPC act as a pipe for communication with a gRPC service. A channel
represents a session which, unlike HTTP/1.1, remains open for multiple requests
and responses.

#### Services

in gRPC define the methods available for remote calls. They're specified using
the protobuf language and serve as API between the server and its clients.

#### Stubs

are the client-side representation of a gRPC service. They provide methods
corresponding to the service methods defined in the `.proto` files.

#### Calls and Streams

in gRPC provide the functionality for data exchange between the client and the
server. The four primary RPC types are:

1. *Unary Calls*: This is the most common type, similar to a regular function call
   where the client sends a single request and gets a single response.
2. *Server Streaming*: The client sends a single request and receives a stream of
   responses. Useful when the server needs to push data continuously after
   processing a client request.
3. *Client Streaming*: The client sends multiple requests before it awaits the
   server's single response. Useful in scenarios like file uploads where the
   client has a stream of data to send.
4. *Bidirectional Streaming*: Both client and server send a sequence of messages
   to each other. They can read and write in any order. This is beneficial in
   real-time applications where both sides need to continuously update each
   other.


