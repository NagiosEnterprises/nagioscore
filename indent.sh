#!/bin/sh

f=$1

astyle --style=banner --indent=tab --unpad-paren --pad-oper --suffix=none $f
