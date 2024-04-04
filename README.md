<div align="center">

# Remote Audio Plugin Development

## Integrating Qt and CLAP for Enhanced Development Solutions



> **My thesis submitted to attain the degree of**
> *BACHELORS OF SCIENCE ( B.Sc. )*
> *author: Dennis Oberst*

</div>

This repository contains the latest version of my thesis, all the tools and
images, fonts, reviews, examples and somwhat tracked this period of my life.
Read it [here](./remote_audio_plugin_development.pdf)

In this work, I will highlight the outcomes of this research, which includes two
proof-of-concept implementations:

- A server-side library to remotely interact with the
    [CLAP](https://github.com/free-audio/clap) audio-plugin standard,
    focusing on headless processing and remote GUIs. It uses
    [gRPC](https://grpc.io/) and provides a language agnostic
    [protobuf](https://protobuf.dev/) API for its clients.

📌 [clap-rci](https://github.com/deeedob/clap-rci)

- A client-side library that provides integration with the
    [Qt](https://github.com/qt) framework. It uses the new
    [QtGrpc](https://doc.qt.io/qt-6/qtgrpc-index.html) module and serves
    as the client implementation for *clap-rci*.

📌 [qtcleveraudioplugin](https://code.qt.io/cgit/playground/qtcleveraudioplugin.git/about/)

## MiniClap

A very short ~200 LOC gain plugin which is described
in the thesis. Pull in clap as dependency:

```bash
git submodule update --init
```
and have a look [at it](examples/mini_clap/mini_gain.cpp).

## Building

1. Depends on `pandoc, pandoc-crossref, xelatex`
2. Install additional filters into python venv:
    ```bash
        python -m venv venv
        source venv/bin/activate.<your shell>
        # Install the needed packages
        pip install pandoc-include pandoc-mermaid-filter
    ```
3. [Fancify images](imgages/fancy_imgs.sh)
4. Build:
    ```bash
        make thesis
    ```
