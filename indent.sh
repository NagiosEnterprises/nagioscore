#!/bin/sh
ARTISTIC_STYLE_OPTIONS=/dev/null

export ARTISTIC_STYLE_OPTIONS

astyle --style=banner --indent=tab --unpad-paren --pad-oper --suffix=.pre-indent "$@"
