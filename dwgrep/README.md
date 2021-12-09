# dwgrep tooling to inspect debuginfo

Tools can be written using [dwgrep](http://pmachata.github.io/dwgrep/)
to analyze the debuginfo to detect issues with the generated
debuginfo.

- lists_parameters - for each function in debuginfo list out name, address, parameters, and parameters in function's abstract DIE
- locationlist_0len_entry - list out variables that have 0-length entry in their location lists
- parameter_diffs - list out each function where the abstract origin and function have different number of formal paramaters
- parameter_nolocation - list out each function formal parameter without location list information
