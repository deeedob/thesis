# Remote Audio Plugin Development

## Integrating Qt and CLAP for Enhanced Development Solutions

> My thesis submitted to attain the degree of
>       *BACHELORS OF SCIENCE ( B.Sc. )*
> author: Dennis Oberst

This repository contains the latest version of my thesis, all the tools and
images, fonts, reviews, examples and somwhat tracked this period of my life.
Read it [here](./remote_audio_plugin_development.pdf)

## MiniClap

A very short ~200 LOC gain plugin which is described
in the thesis. `git submodule update --init` and have a look
[at it](examples/mini_clap/mini_gain.cpp).

## Building

1. [Fancify images](imgages/fancy_imgs.sh)
2. Depends on `pandoc, pandoc-crossref, xelatex`
3. Install additional filters into python venv:
    ```bash
        python -m venv venv
        source venv/bin/activate.<your shell>
        # Install the needed packages
        pip install pandoc-include pandoc-mermaid-filter
    ```
3. Build:
    ```bash
        make thesis-pdf
    ```
