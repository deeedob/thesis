---
author: Dennis Oberst
title: Audio Plugin Development with QtGrpc
subtitle: A Journey Into Remote GUIs
date: 19 October 2023

lang: en-US

abstract: |
  The Qt framework is well-established in the domain of cross-platform
  application development, yet its application within audio plugin development
  has remained largely unexplored. This thesis explores the integration of Qt
  in audio plugin development, examining the synergy between Qt's robust GUI
  capabilities and the emerging CLAP (Clever Audio Plugin) standard. CLAP, with
  its emphasis on simplicity, clarity, and robustness, offers a streamlined and
  intuitive API that aligns with Qt's design principles.

  By integrating gRPC (g Remote Procedure Calls), an open-source communication
  protocol, the research highlights a method for enhancing interactions between
  audio plugins and host applications. This integration points to a flexible,
  scalable approach, suitable for modern software design.

  This work presents a novel approach to audio plugin development that
  leverages the combined strengths of Qt, CLAP, and gRPC. The resulting library
  not only provides a consistent and adaptable user experience across various
  operating systems but also simplifies the plugin creation process. This work
  stands as a testament to the untapped potential of Qt in the audio-plugin
  industry, paving the way for advancements in audio plugin creation that
  enhance both user engagement and developer workflow.

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
