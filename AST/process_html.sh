#!/bin/bash

gzip -k -9 ast.html
bin2C ast.html.gz
rm ast.html.gz
mv ast.html.c html.h

