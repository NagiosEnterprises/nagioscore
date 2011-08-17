#!/bin/sh

for f in `find . -type f -name "*.[c]"`; do
	./indent.sh $f
done

