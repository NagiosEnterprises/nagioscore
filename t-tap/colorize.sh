#!/bin/sh
# used to colorize the test output

color_off='\033[0m'

black='\033[1;30m'
red='\033[1;31m'
green='\033[1;32m'
yellow='\033[1;33m'
blue='\033[1;34m'
purple='\033[1;35m'
cyan='\033[1;36m'
white='\033[1;37m'

bg_black='\033[40m'
bg_red='\033[41m'

# we want a black background with white text by default
printf "%b" "${bg_black}${white}"

# read stdin
while read line; do
    
    # is this an ok line?
    if echo $line | grep "^ok" >/dev/null 2>&1; then

        newline=$(echo $line | sed 's/^ok//')
        final="${green}    ok${white}${newline}"

    # not ok line?
    elif echo $line | grep "^not ok" >/dev/null 2>&1; then

        newline=$(echo $line | sed 's/^not ok//')
        final="${bg_red}not ok${bg_black}${newline}"

    # comment line?
    elif echo $line | grep "^#" >/dev/null 2>&1; then

        # not actual comment:
        if echo $line | grep "^# Looks like you planned" >/dev/null 2>&1; then

            final="${cyan}${line}${white}"

        elif echo $line | grep "^# Failed test" >/dev/null 2>&1; then

            final="${red}${line}${white}"            

        # actual comment:
        else
            newline=$(echo $line | sed 's/^#//')
            final="${purple}#${newline}${white}"
        fi

    # result line
    elif echo $line | grep "^Result:" >/dev/null 2>&1; then

        newline=$(echo $line | sed 's/^Result://')

        if echo $line | grep -i "fail" >/dev/null 2>&1; then
            final="${blue}Result: ${red}FAIL${white}"
        else
            final="${blue}Result: ${green}PASS${white}"
        fi

    # otherwise just pass it thru
    else
        final="$line"
    fi

    # highlight line numbers
    #if echo $final | grep "line [0-9]*" >/dev/null 2>&1; then
        # DOESNT WORK
        #final=$(echo $final | GREP_COLORS="mt=\"01;33\"" grep "line [0-9]*")
    #fi

    # print out
    printf "%b\n" "$final"

done

# reset term colors
printf "%b\n" "${color_off}"