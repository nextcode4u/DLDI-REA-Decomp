# DLDI REA Decomp

Matching decompilation projects for Nintendo DS DLDI drivers. Each driver has its own directory.

- [N-Card NRIO](n-card/) builds all 60 driver routines from C by default. The optional matching build reproduces XuluMenu's `nrio.dldi` byte for byte. Byte-for-byte matching of the C build is in progress. [NAND recovery instructions and hardware findings](n-card/raw-nand/)

- [nrioTool](n-card/nrioTool/) provides on-console N-Card diagnostics, backups, recovery, native format and menu setup, file management, and SRL dumps. The build also supports USB drive mode on a DS or DS Lite with a Slot-2 N-Card writer. [Download nrioTool.nds](n-card/nrioTool/nrioTool.nds?raw=1)

## Contributors

- [Apache Thunder](https://github.com/ApacheThunder) contributed N-Card boot layout and recovery findings, the DS Linker spare-area sample, and hardware testing of the C driver, raw NAND tools, and banner-only writer.

## Developer note

This project was developed with assistance from AI language models. Some source code was generated or refined using AI, then reviewed, modified and integrated by the project author where appropriate.
