#!/bin/sh
ARTISTIC_STYLE_OPTIONS=/dev/null

export ARTISTIC_STYLE_OPTIONS

f=$1

astyle --style=banner --indent=tab --unpad-paren --pad-oper --suffix=none $f
