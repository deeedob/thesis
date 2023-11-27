.PHONY: thesis-pdf detect-images
.DEFAULT_GOAL := help

help:
	head -2 Makefile

thesis-uni:
	pandoc --template project.latex --defaults=project.yaml --highlight-style tango

thesis:
	pandoc --template project.latex --defaults=modern.yaml --highlight-style tango

detect-images:
	grep -ohr "images/.*" content/
