# dwgrep tooling to inspect debuginfo

Tools can be written using [dwgrep](http://pmachata.github.io/dwgrep/)
to analyze the debuginfo to detect issues with the generated
debuginfo.

- const_parameters - [WIP] Provide some metrics on number of functions that have parameters replaced by constants
- functstart_diff - compare the start of functions reported by nm and the debug info
- lists_parameters - for each function in debuginfo list out name, address, parameters, and parameters in function's abstract DIE
- locationlist_0len_entry - list out variables that have 0-length entry in their location lists
- loclist_branch - list out location list entries that have control flow operations
- noloclist - list out the function parameters and variables that do not have location lists
- parameter_diffs - list out each function where the abstract origin and function have different number of formal paramaters
- parameter_nolocation - list out each function formal parameter without location list information
