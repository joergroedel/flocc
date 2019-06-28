#!/usr/bin/python3
#

import json
import sys

sys.argv.pop(0)

inp = sys.argv.pop(0)
out = sys.argv.pop(0)

with open(inp) as fp:
	data = json.loads(fp.read())

with open(out, 'w') as fp:
	json.dump(data, fp, indent=2)
