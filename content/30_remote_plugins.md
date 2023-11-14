# 3 Remote Plugins

## 3.1 Introduction

As outlined in the previous sections the problem should be clear, and the tools
that we will use to solve it should be known. Since the goal is to enable
native development experience, unflexible techniques as enforcing users to
compile Qt in a seperate namespace are not applicable. Furthermore we want a
stable user experience that works at least on all available desktop platforms.

![Traditional plugin architecture](images/plugins_traditional.png){#fig:plug_tradition}

*@fig:plug_tradition* outlines traditional audio plugin architectures, when
used with graphical user interfaces. As we know, multiple instances will be
created out of our DSO, hence enforcing reentrancy. The traditional approach is
to start the event loop of the user interface inside a dedicated thread,
resulting in a minimum of 2 threads that 'live' inside the DSO's process. This
technique can be seen in [JUCE](https://juce.com/), which is a well-known
library for creating plugin-standard agnostic plugins, as well as in various
other open source implementation.

Since we can't start Qts event loop inside a dedicated thread, we will simply
execute the plugin GUI inside a seperate process. Starting it inside a seperate
process could be seen as expensive or beeing slow and hard to incorporate because
of the various issues with IPC^[Inter Process Communication], but in my opinion
it's equally as good. First of all modern computers should be able to handle
another process with ease and secondly we will gain safety with this approach.
To elaborate, let's look at the case where we have GUI and the realtime thread
within the same process. A crash in the user interface would then also crash the
realtime thread. However when the GUI crashes launched as seperate process the
realtime thread wouldn't even notice it at all. The implementation of IPC or RPC
techniques is a bit more tricky and indeed result in some work. But the performance
shouldn't be affected critically by it. The messages we have to share between audio
and gui thread are very small in size, as some parameter changes or note events. They
are usually just a few bytes large. When thinking about sharing the audio data
itself techniques such as opening shared memory pools could be employed which would
make it equally as fast as it would be within the same thread. Although all at the
cost of beeing way more 'expensive' to implement. Furthermore GUIs don't have
to operate at the speed of the realtime thread. We humans


## 3.2 Remote Control Interface

## 3.3 QtGrpc Client Implementation
