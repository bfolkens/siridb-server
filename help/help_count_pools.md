count pools
===========

Syntax:

	count pools [where ...]
	
Count pools returns the number of pools in the SiriDB cluster.

Examples:

	# Get number of pools
	count pools 
	
	# Get number of pools with less or equal to 100000 series
	count pools where series <= 100000

Example output:

	{"pools": 2}