#!/usr/bin/env bash

EXIT_CODE=0

if grep -r $'\r' ./src/; then
	echo "found CRLF lines"
	EXIT_CODE=1
fi

for file in $(find ./src/ -type f); do
	if [[ $(tail -c1 $file | grep $'\n') ]]; then
		echo "missing final newline: $file"
		EXIT_CODE=1
	fi
done

exit "$EXIT_CODE"
