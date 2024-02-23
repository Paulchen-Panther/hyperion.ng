#!/usr/bin/env python
import json, sys

retval  = 0
total   = 0
errors  = 0
for filename in sys.argv[1:]:
	with open(filename) as f:
		total += 1
		try:
			data = f.read()
			json.loads(data)
		except ValueError as e:
			print("-- Check json %s ... " % filename + 'invalid ('+str(e)+')')
			retval = 1
			errors += 1

print("-- Checked JSON files: %s | Success: %s | Errors: %s" % (total,(total-errors),errors))

sys.exit(retval)
