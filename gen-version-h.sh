#!/bin/sh

cat > .version.h <<EOF
#ifndef __VERSION_H
#define __VERSION_H
#define FLOCC_VERSION	"$1"
#endif
EOF

MOVE="no"
if [ -f "version.h" ]; then
	cmp --silent version.h .version.h || MOVE="yes"
else
	MOVE="yes"
fi

if [ "$MOVE" == "yes" ]; then
	mv .version.h version.h
else
	rm -f .version.h
fi
