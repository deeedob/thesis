# 3 Remote Plugins

## 3.1 Introduction

After the definition of the problem, and the introduction to the tools
that will be used, we focus on this section on describing the main goal,
which is to enable a native development experience, leaving aside
unflexible techniques as enforcing users to compile Qt in a seperate namespace.
Furthermore a stable user experience that works at least on all available desktop platforms,
is a hard requirement.

![Traditional plugin architecture](images/plugins_traditional.png){#fig:plug_tradition}

*@fig:plug_tradition* outlines traditional audio plugin architectures, when
used with graphical user interfaces. As we know, multiple instances will be
created out of our DSO, hence enforcing reentrancy. The traditional approach is
to start the event loop of the user interface inside a dedicated thread,
resulting in a minimum of 2 threads that 'live' inside the DSO's process. This
technique can be seen in [JUCE](https://juce.com/), a well-known
library for creating plugin-standard agnostic plugins, as well as in various
other open source implementation.

Due to the limitation that is not possible to start Qts event loop inside a dedicated thread, we will simply
execute the GUI plugin inside a seperate process. Starting it inside a seperate
process could be seen as expensive or beeing slow and hard to incorporate because
of the various issues with IPC^[Inter Process Communication], but experimentation for this work
has shown that it's equally as good. First of all, modern computers should be able to handle
an extra process with ease and secondly this approach provides the implementation with a safety state.

As an example, let's look at the case where we have GUI and the realtime thread
within the same process. A crash in the user interface would then also crash the
realtime thread. However when the GUI crashes, launched as seperate process, the
realtime thread wouldn't notice it. The implementation of IPC or RPC
techniques is a bit more delicate and indeed might result in additional implementation efforts.
On the other hand, the performance
shouldn't be affected critically by it. The messages we have to share between audio
and gui thread are very small in size, as some parameter changes or note events; 
usually just a few bytes larger. When thinking about sharing the audio data
techniques such as opening shared memory pools, it could be employed in order to
make it equally as fast as it would be within the same thread. Although, the
cost will appear on the difficulties when implementing it. It's important to note
that GUIs don't have to operate at the same realtime thread speed.


## 3.2 Remote Control Interface

## 3.3 QtGrpc Client Implementation
