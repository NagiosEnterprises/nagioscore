#!/bin/sh

astyle --indent=tab --unpad-paren --pad-oper --pad-header --suffix=none \
		--brackets=linux $(find . -type f -name "*.[ch]")
