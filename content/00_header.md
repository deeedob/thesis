---
author: Dennis Oberst
title: Audio Plugin Development with QtGrpc
subtitle: A Journey Into Remote GUIs
date: 19 October 2023

lang: en-US

abstract: |
  The Qt project is a powerful and versatile C++ framework widely used
  for developing cross-platform applications. Additionally, Qt is no stranger
  to the audio-industry, with a strong presence within professional audio
  companies. If we look into the expanding world of audio-plugins however,
  we would steer into the void when looking for Qt user-interfaces. With this
  work I will explain the reasons and problems that led to this state and show
  a new and innovative approach in solving the problems. A user should be able
  to find plugins that fits perfectly their needs to customize their environment.
  However developers often don't have that freedom when it comes to developing
  such plugins. The goal is to bridge the gap and extend the toolset developers
  have in developing audio plugins and their graphical user interface specifically.

  Special thanks to:

  - Dr. Cristián Maureira-Fredes (Qt) for making all of this possible and his ...
  - Alexandre Bique (Bitwig & CLAP) for listening to my idea and providing
    deep insights into CLAP and extended techniques for communication

papersize: a4

fontsize: 14
# mainfont: TitilliumWeb
monofont: "MonoLisa"
bibliography: content/bibliography.bib

geometry: [a4paper, bindingoffset=0mm, inner=30mm, outer=30mm, top=30mm, bottom=30mm]
linestretch: 1.25

color-links: true
link-citations: true
citecolor: blue
linkcolor: blue
urlcolor: blue

toc_depth: 2
toc-title: 'Contents'

classoption: [titlepage, openright, DIV=calc, toc=listof, listof=nochaptergap]
geometry: [a4paper, bindingoffset=0mm, inner=30mm, outer=30mm, top=30mm, bottom=30mm]
figureTitle: Figure
figPrefix: Figure
graphics: yes

---
