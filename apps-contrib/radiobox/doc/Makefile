#
# Copyright (c) 2013 Instrumentation Technologies
# All Rights Reserved.
#
# $Id: Makefile 1200 2014-02-20 12:52:27Z ales.bardorfer $
#

.PHONY : all pdf clean

PLANTUML_JAR ?= /usr/share/java/plantuml.jar
#PLANTUML_JAR ?= C:/Users/espero/Packages/java/plantuml.jar
PDF_FILENAME := RedPitaya_RadioBox_Development_Manual.pdf

all: pdf

pdf $(PDF_FILENAME):
	@echo "PLANTUML_JAR = $(PLANTUML_JAR)"
	java -jar '$(PLANTUML_JAR)' -forcecairo -o img *.dox
	java -jar '$(PLANTUML_JAR)' -forcecairo -o ./img ../src/*.c
	java -jar '$(PLANTUML_JAR)' -forcecairo -o ./img ../src/*.h
	doxygen
	make -C latex pdf
	cp latex/refman.pdf $(PDF_FILENAME)

clean:
	$(RM) -r img html latex
	$(RM) $(PDF_FILENAME)
