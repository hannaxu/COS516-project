# COS516-project

Flattening and translation of SpecPriv. Original program is `loopevent_orig.cpp`. Supporting files including constants and many definitions of helper functions used in `loopevent_orig.cpp` are not included in this repo. `isolated_func_analysis.cpp` and `loopevent.cpp` are cleaned and isolated code from the original in order to parse out the control flow.

## Simplified
`constants.h` is largely a condensed version of the original code. There is an added section of customizable constants that were required to make the abstraction of heaps and invariants. The other files are the corresponding abstractions of the identified structures from the original. `loopevent_trans.c` is the file to run through DUET for cost analysis.

## Translation
The code in `translation` is a more faithful translation attempt at SpecPriv. It attempts to preserve the packet and queue structure completely, and maintains the communication decisions as well as packet type choices dependent on what heap the worker is assigned to. Unfortunately, this representation became too difficult to work with and analyze.

