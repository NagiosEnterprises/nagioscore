#!/bin/sh

cd `dirname $0`

echo '1..2'

make 2>&1 > /dev/null

perl ./test.pl 2>&1 | sed -e 's/#     Failed (TODO) test \(.*\)/#     Failed (TODO) test ()/' > test.pl.out
perlstatus=$?

./test 2>&1 | sed -e 's/#     Failed (TODO) test \(.*\)/#     Failed (TODO) test ()/' > test.c.out
cstatus=$?

diff -u test.pl.out test.c.out

if [ $? -eq 0 ]; then
	echo 'ok 1 - output is identical'
else
	echo 'not ok 1 - output is identical'
fi

if [ $perlstatus -eq $cstatus ]; then
	echo 'ok 2 - status code'
else
	echo 'not ok 2 - status code'
	echo "# perlstatus = $perlstatus"
	echo "#    cstatus = $cstatus"
fi
