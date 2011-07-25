#!/bin/sh

astyle --style=banner --indent=tab --unpad-paren --pad-oper --suffix=none \
		$(find . -type f -name "*.[ch]")
