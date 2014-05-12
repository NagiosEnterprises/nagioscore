#!/bin/sh
ARTISTIC_STYLE_OPTIONS=/dev/null

export ARTISTIC_STYLE_OPTIONS

astyle --style=knf --indent=tab --suffix=.pre-indent "$@"
