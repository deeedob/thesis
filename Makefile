.PHONY: thesis-pdf
.DEFAULT_GOAL := help

help:
	head -2 Makefile

thesis-pdf:
	pandoc -F pandoc-crossref --citeproc --defaults=defaults.yaml --include-in-header=thesis.tex

