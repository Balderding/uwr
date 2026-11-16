# UW.EXE, routine by routine

Made by tools/checklist.py from re/uw_map.txt, re/routines_status.txt and the addresses the source names. Status: **proved** (identical to UW.EXE by an emulator verifier: re/proved.txt), **unproved** (our source names it, no verifier yet: not counted as done), **ported** (set by hand), **replaced** (graphics, sound, input, interface: our engine's own way), **notneeded** (DOS, memory, hardware, C runtime), **todo**.

**Done: 39.4% of the code** (160135 of 406266 bytes; 893 of 1904 routines); **the game logic proved: 30.2%** (106707 of 352838 bytes); unproved claims: 56 routines, 19132 bytes

| Status | Routines | Bytes |
|---|---|---|
| proved | 374 | 106707 |
| unproved | 56 | 19132 |
| ported | 0 | 0 |
| replaced | 256 | 30923 |
| notneeded | 263 | 22505 |
| todo | 955 | 226999 |

## seg 0000 (11 routines, 1256 bytes, 100% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 003202 (0000:0002) | 113 | 6 |  | notneeded | program start (Borland C0: stack, DGROUP, the C runtime's start) |
| 003273 (0000:0073) | 51 | 0 |  | notneeded | program start (Borland C0: stack, DGROUP, the C runtime's start) |
| 0032a6 (0000:00a6) | 69 | 6 |  | notneeded | program start (Borland C0: stack, DGROUP, the C runtime's start) |
| 0032eb (0000:00eb) | 53 | 10 |  | notneeded | program start (Borland C0: stack, DGROUP, the C runtime's start) |
| 003320 (0000:0120) | 53 | 1 |  | notneeded | program start (Borland C0: stack, DGROUP, the C runtime's start) |
| 003355 (0000:0155) | 54 | 4 |  | notneeded | program start (Borland C0: stack, DGROUP, the C runtime's start) |
| 00338b (0000:018b) | 55 | 4 |  | notneeded | program start (Borland C0: stack, DGROUP, the C runtime's start) |
| 0033c2 (0000:01c2) | 55 | 0 |  | notneeded | program start (Borland C0: stack, DGROUP, the C runtime's start) |
| 0033f9 (0000:01f9) | 48 | 1 |  | notneeded | program start (Borland C0: stack, DGROUP, the C runtime's start) |
| 003429 (0000:0229) | 263 | 8 |  | notneeded | program start (Borland C0: stack, DGROUP, the C runtime's start) |
| 003530 (0000:0330) | 442 | 8 |  | notneeded | program start (Borland C0: stack, DGROUP, the C runtime's start) |

## seg 004e (10 routines, 894 bytes, 100% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 0036ec (004e:000c) | 62 | 1 |  | notneeded | program start helpers |
| 00372a (004e:004a) | 16 | 6 |  | notneeded | program start helpers |
| 00373a (004e:005a) | 171 | 2 |  | notneeded | program start helpers |
| 0037e5 (004e:0105) | 13 | 1 |  | notneeded | program start helpers |
| 0037f2 (004e:0112) | 195 | 1 |  | notneeded | program start helpers |
| 0038b5 (004e:01d5) | 26 | 5 |  | notneeded | program start helpers |
| 0038cf (004e:01ef) | 108 | 1 |  | notneeded | program start helpers |
| 00393b (004e:025b) | 13 | 11 |  | notneeded | program start helpers |
| 003948 (004e:0268) | 105 | 1 |  | notneeded | program start helpers |
| 0039b1 (004e:02d1) | 185 | 0 |  | notneeded | program start helpers |

## seg 0086 (1 routines, 146 bytes, 100% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 003a6a (0086:000a) | 146 | 1 |  | notneeded | program start helper |

## seg 0090 (162 routines, 25965 bytes, 100% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 003b00 (0090:0000) | 8 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 003b08 (0090:0008) | 8 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 003b10 (0090:0010) | 8 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 003b18 (0090:0018) | 16 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 003b28 (0090:0028) | 56 | 1 | io 1 | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 003b60 (0090:0060) | 193 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 003c21 (0090:0121) | 769 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 003f22 (0090:0422) | 24 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 003f3a (0090:043a) | 127 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 003fb9 (0090:04b9) | 24 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 003fd1 (0090:04d1) | 1397 | 1 | int 1 | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 004546 (0090:0a46) | 2 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 004548 (0090:0a48) | 128 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 0045c8 (0090:0ac8) | 36 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 0045ec (0090:0aec) | 32 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 00460c (0090:0b0c) | 81 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 00465d (0090:0b5d) | 143 | 1 | io 4 | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 0046ec (0090:0bec) | 321 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 00482d (0090:0d2d) | 466 | 1 | io 9 | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 0049ff (0090:0eff) | 120 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 004a77 (0090:0f77) | 267 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 004b82 (0090:1082) | 3768 | 1 | io 6 | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 005a3a (0090:1f3a) | 2117 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 00627f (0090:277f) | 1083 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 0066ba (0090:2bba) | 25 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 0066d3 (0090:2bd3) | 39 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 0066fa (0090:2bfa) | 23 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 006711 (0090:2c11) | 23 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 006728 (0090:2c28) | 14 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 006736 (0090:2c36) | 1775 | 1 | io 9 | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 006e25 (0090:3325) | 26 | 1 | io 3 | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 006e3f (0090:333f) | 24 | 1 | io 2 | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 006e57 (0090:3357) | 47 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 006e86 (0090:3386) | 63 | 2 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 006ec5 (0090:33c5) | 15 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 006ed4 (0090:33d4) | 15 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 006ee3 (0090:33e3) | 24 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 006efb (0090:33fb) | 33 | 1 | int 1 | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 006f1c (0090:341c) | 94 | 1 | int 1 io 18 | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 006f7a (0090:347a) | 75 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 006fc5 (0090:34c5) | 319 | 1 | io 11 | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 007104 (0090:3604) | 405 | 1 | io 17 | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 007299 (0090:3799) | 459 | 1 | io 22 | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 007464 (0090:3964) | 24 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 00747c (0090:397c) | 195 | 1 | io 6 | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 00753f (0090:3a3f) | 39 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 007566 (0090:3a66) | 2 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 007568 (0090:3a68) | 30 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 007586 (0090:3a86) | 21 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 00759b (0090:3a9b) | 79 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 0075ea (0090:3aea) | 3 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 0075ed (0090:3aed) | 16 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 0075fd (0090:3afd) | 18 | 2 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 00760f (0090:3b0f) | 83 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 007662 (0090:3b62) | 56 | 3 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 00769a (0090:3b9a) | 7 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 0076a1 (0090:3ba1) | 3 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 0076a4 (0090:3ba4) | 74 | 2 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 0076ee (0090:3bee) | 3 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 0076f1 (0090:3bf1) | 77 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 00773e (0090:3c3e) | 96 | 2 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 00779e (0090:3c9e) | 5 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 0077a3 (0090:3ca3) | 11 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 0077ae (0090:3cae) | 81 | 3 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 0077ff (0090:3cff) | 47 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 00782e (0090:3d2e) | 20 | 2 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 007842 (0090:3d42) | 5 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 007847 (0090:3d47) | 200 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 00790f (0090:3e0f) | 255 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 007a0e (0090:3f0e) | 206 | 2 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 007adc (0090:3fdc) | 98 | 2 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 007b3e (0090:403e) | 94 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 007b9c (0090:409c) | 152 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 007c34 (0090:4134) | 168 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 007cdc (0090:41dc) | 45 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 007d09 (0090:4209) | 50 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 007d3b (0090:423b) | 71 | 2 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 007d82 (0090:4282) | 267 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 007e8d (0090:438d) | 213 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 007f62 (0090:4462) | 80 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 007fb2 (0090:44b2) | 26 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 007fcc (0090:44cc) | 21 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 007fe1 (0090:44e1) | 1263 | 1 | io 106 | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 0084d0 (0090:49d0) | 54 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 008506 (0090:4a06) | 575 | 1 | io 1 | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 008745 (0090:4c45) | 47 | 2 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 008774 (0090:4c74) | 1 | 4 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 008775 (0090:4c75) | 60 | 0 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 0087b1 (0090:4cb1) | 60 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 0087ed (0090:4ced) | 111 | 26 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 00885c (0090:4d5c) | 111 | 0 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 0088cb (0090:4dcb) | 111 | 25 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 00893a (0090:4e3a) | 111 | 0 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 0089a9 (0090:4ea9) | 60 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 0089e5 (0090:4ee5) | 69 | 0 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 008a2a (0090:4f2a) | 60 | 6 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 008a66 (0090:4f66) | 60 | 20 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 008aa2 (0090:4fa2) | 60 | 0 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 008ade (0090:4fde) | 66 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 008b20 (0090:5020) | 66 | 0 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 008b62 (0090:5062) | 66 | 0 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 008ba4 (0090:50a4) | 66 | 0 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 008be6 (0090:50e6) | 71 | 0 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 008c2d (0090:512d) | 71 | 0 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 008c74 (0090:5174) | 63 | 30 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 008cb3 (0090:51b3) | 60 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 008cef (0090:51ef) | 63 | 0 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 008d2e (0090:522e) | 70 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 008d74 (0090:5274) | 70 | 0 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 008dba (0090:52ba) | 90 | 11 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 008e14 (0090:5314) | 60 | 2 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 008e50 (0090:5350) | 60 | 5 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 008e8c (0090:538c) | 71 | 0 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 008ed3 (0090:53d3) | 69 | 0 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 008f18 (0090:5418) | 69 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 008f5d (0090:545d) | 71 | 0 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 008fa4 (0090:54a4) | 71 | 0 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 008feb (0090:54eb) | 74 | 27 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 009035 (0090:5535) | 74 | 0 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 00907f (0090:557f) | 60 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 0090bb (0090:55bb) | 74 | 2 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 009105 (0090:5605) | 74 | 0 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 00914f (0090:564f) | 90 | 0 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 0091a9 (0090:56a9) | 74 | 0 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 0091f3 (0090:56f3) | 63 | 0 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 009232 (0090:5732) | 63 | 0 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 009271 (0090:5771) | 74 | 0 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 0092bb (0090:57bb) | 99 | 17 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 00931e (0090:581e) | 99 | 3 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 009381 (0090:5881) | 95 | 2 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 0093e0 (0090:58e0) | 53 | 0 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 009415 (0090:5915) | 95 | 0 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 009474 (0090:5974) | 77 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 0094c1 (0090:59c1) | 88 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 009519 (0090:5a19) | 25 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 009532 (0090:5a32) | 64 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 009572 (0090:5a72) | 3 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 009575 (0090:5a75) | 3 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 009578 (0090:5a78) | 9 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 009581 (0090:5a81) | 6 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 009587 (0090:5a87) | 6 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 00958d (0090:5a8d) | 3 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 009590 (0090:5a90) | 3 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 009593 (0090:5a93) | 3 | 3 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 009596 (0090:5a96) | 3 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 009599 (0090:5a99) | 3 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 00959c (0090:5a9c) | 3 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 00959f (0090:5a9f) | 3 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 0095a2 (0090:5aa2) | 9 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 0095ab (0090:5aab) | 3 | 2 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 0095ae (0090:5aae) | 3 | 2 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 0095b1 (0090:5ab1) | 3 | 3 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 0095b4 (0090:5ab4) | 3 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 0095b7 (0090:5ab7) | 3 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 0095ba (0090:5aba) | 36 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 0095de (0090:5ade) | 45 | 1 | int 1 | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 00960b (0090:5b0b) | 82 | 4 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 00965d (0090:5b5d) | 8 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 009665 (0090:5b65) | 2027 | 1 | io 40 | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 009e50 (0090:6350) | 198 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 009f16 (0090:6416) | 305 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |
| 00a047 (0090:6547) | 38 | 1 |  | replaced | VGA mode X drawing (retrace 0x3da, plane registers 0x3c4/0x3cf) |

## seg 06e7 (67 routines, 32121 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 00a0d3 (06e7:0063) | 104 | 2 |  | todo |  |
| 00a13b (06e7:00cb) | 194 | 1 |  | todo |  |
| 00a1fd (06e7:018d) | 7 | 1 |  | todo |  |
| 00a204 (06e7:0194) | 712 | 1 | int 1 | todo |  |
| 00a4cc (06e7:045c) | 306 | 1 |  | todo |  |
| 00a5fe (06e7:058e) | 310 | 1 |  | todo |  |
| 00a734 (06e7:06c4) | 308 | 1 |  | todo |  |
| 00a868 (06e7:07f8) | 312 | 1 |  | todo |  |
| 00a9a0 (06e7:0930) | 321 | 1 | int 1 | todo |  |
| 00aae1 (06e7:0a71) | 528 | 1 |  | todo |  |
| 00acf1 (06e7:0c81) | 239 | 1 |  | todo |  |
| 00ade0 (06e7:0d70) | 74 | 1 |  | todo |  |
| 00ae2a (06e7:0dba) | 206 | 2 |  | todo |  |
| 00aef8 (06e7:0e88) | 118 | 1 |  | todo |  |
| 00af6e (06e7:0efe) | 316 | 2 |  | todo |  |
| 00b0aa (06e7:103a) | 31 | 1 |  | todo |  |
| 00b0c9 (06e7:1059) | 250 | 2 |  | todo |  |
| 00b1c3 (06e7:1153) | 427 | 1 |  | todo |  |
| 00b36e (06e7:12fe) | 52 | 1 |  | todo |  |
| 00b3a2 (06e7:1332) | 62 | 1 |  | todo |  |
| 00b3e0 (06e7:1370) | 431 | 2 |  | todo |  |
| 00b58f (06e7:151f) | 652 | 2 |  | todo |  |
| 00b81b (06e7:17ab) | 5924 | 1 |  | todo |  |
| 00cf3f (06e7:2ecf) | 4492 | 1 |  | todo |  |
| 00e0cb (06e7:405b) | 265 | 1 |  | todo |  |
| 00e1d4 (06e7:4164) | 272 | 1 |  | todo |  |
| 00e2e4 (06e7:4274) | 270 | 1 |  | todo |  |
| 00e3f2 (06e7:4382) | 1541 | 1 |  | todo |  |
| 00e9f7 (06e7:4987) | 445 | 1 | int 1 | todo |  |
| 00ebb4 (06e7:4b44) | 818 | 1 |  | todo |  |
| 00eee6 (06e7:4e76) | 458 | 3 |  | todo |  |
| 00f0b0 (06e7:5040) | 41 | 4 |  | todo |  |
| 00f0d9 (06e7:5069) | 45 | 3 |  | todo |  |
| 00f106 (06e7:5096) | 40 | 1 |  | todo |  |
| 00f12e (06e7:50be) | 26 | 2 |  | todo |  |
| 00f148 (06e7:50d8) | 203 | 2 |  | todo |  |
| 00f213 (06e7:51a3) | 108 | 1 |  | todo |  |
| 00f27f (06e7:520f) | 240 | 2 |  | todo |  |
| 00f36f (06e7:52ff) | 117 | 1 |  | todo |  |
| 00f3e4 (06e7:5374) | 117 | 1 |  | todo |  |
| 00f459 (06e7:53e9) | 61 | 2 |  | todo |  |
| 00f496 (06e7:5426) | 299 | 2 |  | todo |  |
| 00f5c1 (06e7:5551) | 61 | 2 |  | todo |  |
| 00f5fe (06e7:558e) | 299 | 2 |  | todo |  |
| 00f729 (06e7:56b9) | 61 | 2 |  | todo |  |
| 00f766 (06e7:56f6) | 74 | 2 |  | todo |  |
| 00f7b0 (06e7:5740) | 225 | 1 |  | todo |  |
| 00f891 (06e7:5821) | 19 | 2 |  | todo |  |
| 00f8a4 (06e7:5834) | 74 | 1 |  | todo |  |
| 00f8ee (06e7:587e) | 124 | 15 |  | todo |  |
| 00f96a (06e7:58fa) | 61 | 1 |  | todo |  |
| 00f9a7 (06e7:5937) | 7 | 1 |  | todo |  |
| 00f9ae (06e7:593e) | 158 | 1 |  | todo |  |
| 00fa4c (06e7:59dc) | 167 | 1 |  | todo |  |
| 00faf3 (06e7:5a83) | 165 | 1 |  | todo |  |
| 00fb98 (06e7:5b28) | 20 | 1 |  | todo |  |
| 00fbac (06e7:5b3c) | 1517 | 1 | io 1 | unproved |  |
| 010199 (06e7:6129) | 290 | 1 |  | todo |  |
| 0102bb (06e7:624b) | 379 | 1 |  | todo |  |
| 010436 (06e7:63c6) | 383 | 1 |  | todo |  |
| 0105b5 (06e7:6545) | 381 | 1 |  | todo |  |
| 010732 (06e7:66c2) | 5191 | 1 | io 4 | todo |  |
| 011b79 (06e7:7b09) | 46 | 1 | int 1 | todo |  |
| 011ba7 (06e7:7b37) | 6 | 1 |  | todo |  |
| 011bad (06e7:7b3d) | 125 | 1 | int 1 | todo |  |
| 011c2a (06e7:7bba) | 209 | 1 | int 5 | todo |  |
| 011cfb (06e7:7c8b) | 337 | 1 | int 2 | todo |  |

## seg 0ec5 (190 routines, 15170 bytes, 100% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 011f63 (0ec5:0113) | 32 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 011f83 (0ec5:0133) | 64 | 2 | int 1 | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 011fc3 (0ec5:0173) | 67 | 1 | int 5 | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 012006 (0ec5:01b6) | 45 | 1 | int 4 | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 012033 (0ec5:01e3) | 67 | 2 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 012076 (0ec5:0226) | 35 | 2 | int 1 | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 012099 (0ec5:0249) | 55 | 3 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 0120d0 (0ec5:0280) | 45 | 0 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 0120fd (0ec5:02ad) | 81 | 2 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 01214e (0ec5:02fe) | 46 | 29 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 01217c (0ec5:032c) | 37 | 8 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 0121a1 (0ec5:0351) | 128 | 2 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 012221 (0ec5:03d1) | 63 | 2 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 012260 (0ec5:0410) | 109 | 2 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 0122cd (0ec5:047d) | 128 | 18 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 01234d (0ec5:04fd) | 82 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 01239f (0ec5:054f) | 134 | 7 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 012425 (0ec5:05d5) | 111 | 3 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 012494 (0ec5:0644) | 56 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 0124cc (0ec5:067c) | 198 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 012592 (0ec5:0742) | 152 | 3 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 01262a (0ec5:07da) | 34 | 2 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 01264c (0ec5:07fc) | 30 | 15 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 01266a (0ec5:081a) | 36 | 0 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 01268e (0ec5:083e) | 42 | 0 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 0126b8 (0ec5:0868) | 236 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 0127a4 (0ec5:0954) | 77 | 15 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 0127f1 (0ec5:09a1) | 102 | 2 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 012857 (0ec5:0a07) | 102 | 8 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 0128bd (0ec5:0a6d) | 67 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 012900 (0ec5:0ab0) | 73 | 2 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 012949 (0ec5:0af9) | 19 | 1 | int 1 | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 01295c (0ec5:0b0c) | 19 | 1 | int 1 | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 01296f (0ec5:0b1f) | 40 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 012997 (0ec5:0b47) | 23 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 0129ae (0ec5:0b5e) | 28 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 0129ca (0ec5:0b7a) | 263 | 3 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 012ad1 (0ec5:0c81) | 21 | 0 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 012ae6 (0ec5:0c96) | 320 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 012c26 (0ec5:0dd6) | 17 | 2 |  | unproved |  |
| 012c37 (0ec5:0de7) | 38 | 91 |  | proved | verify.py rand |
| 012c5d (0ec5:0e0d) | 143 | 3 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 012cec (0ec5:0e9c) | 216 | 2 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 012dc4 (0ec5:0f74) | 31 | 0 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 012de3 (0ec5:0f93) | 10 | 0 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 012ded (0ec5:0f9d) | 30 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 012e0b (0ec5:0fbb) | 29 | 0 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 012e28 (0ec5:0fd8) | 56 | 0 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 012e60 (0ec5:1010) | 77 | 2 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 012ead (0ec5:105d) | 51 | 3 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 012ee0 (0ec5:1090) | 26 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 012efa (0ec5:10aa) | 291 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 01301d (0ec5:11cd) | 464 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 0131ed (0ec5:139d) | 72 | 2 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 013235 (0ec5:13e5) | 57 | 0 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 01326e (0ec5:141e) | 44 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 01329a (0ec5:144a) | 44 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 0132c6 (0ec5:1476) | 319 | 8 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 013405 (0ec5:15b5) | 112 | 5 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 013475 (0ec5:1625) | 13 | 0 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 013482 (0ec5:1632) | 34 | 3 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 0134a4 (0ec5:1654) | 49 | 3 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 0134d5 (0ec5:1685) | 12 | 0 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 0134e1 (0ec5:1691) | 16 | 0 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 0134f1 (0ec5:16a1) | 28 | 1 | int 2 | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 01350d (0ec5:16bd) | 13 | 0 | int 1 | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 01351a (0ec5:16ca) | 27 | 2 | int 1 | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 013535 (0ec5:16e5) | 30 | 2 | int 1 | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 013553 (0ec5:1703) | 16 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 013563 (0ec5:1713) | 60 | 1 | io 8 | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 01359f (0ec5:174f) | 65 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 0135e0 (0ec5:1790) | 198 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 0136a6 (0ec5:1856) | 104 | 2 | int 4 | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 01370e (0ec5:18be) | 51 | 2 | int 4 | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 013741 (0ec5:18f1) | 45 | 2 | int 4 | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 01376e (0ec5:191e) | 65 | 3 | int 3 | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 0137af (0ec5:195f) | 41 | 2 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 0137d8 (0ec5:1988) | 89 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 013831 (0ec5:19e1) | 24 | 2 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 013849 (0ec5:19f9) | 184 | 3 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 013901 (0ec5:1ab1) | 27 | 1 | int 1 | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 01391c (0ec5:1acc) | 94 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 01397a (0ec5:1b2a) | 17 | 0 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 01398b (0ec5:1b3b) | 29 | 1 | int 1 | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 0139a8 (0ec5:1b58) | 32 | 1 | int 1 | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 0139c8 (0ec5:1b78) | 15 | 1 | int 1 | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 0139d7 (0ec5:1b87) | 17 | 1 | int 1 | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 0139e8 (0ec5:1b98) | 40 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 013a10 (0ec5:1bc0) | 156 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 013aac (0ec5:1c5c) | 37 | 0 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 013ad1 (0ec5:1c81) | 99 | 3 | int 1 | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 013b34 (0ec5:1ce4) | 39 | 1 | int 1 | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 013b5b (0ec5:1d0b) | 61 | 23 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 013b98 (0ec5:1d48) | 17 | 2 | int 1 | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 013ba9 (0ec5:1d59) | 41 | 21 | int 1 | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 013bd2 (0ec5:1d82) | 125 | 4 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 013c4f (0ec5:1dff) | 39 | 18 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 013c76 (0ec5:1e26) | 26 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 013c90 (0ec5:1e40) | 37 | 2 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 013cb5 (0ec5:1e65) | 33 | 3 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 013cd6 (0ec5:1e86) | 40 | 2 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 013cfe (0ec5:1eae) | 25 | 15 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 013d17 (0ec5:1ec7) | 22 | 2 | int 1 | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 013d2d (0ec5:1edd) | 34 | 19 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 013d4f (0ec5:1eff) | 25 | 1 | int 1 | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 013d68 (0ec5:1f18) | 18 | 1 | int 1 | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 013d7a (0ec5:1f2a) | 315 | 29 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 013eb5 (0ec5:2065) | 76 | 1 | int 1 | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 013f01 (0ec5:20b1) | 187 | 12 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 013fbc (0ec5:216c) | 26 | 2 | int 1 | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 013fd6 (0ec5:2186) | 31 | 1 | int 1 | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 013ff5 (0ec5:21a5) | 20 | 3 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 014009 (0ec5:21b9) | 117 | 3 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 01407e (0ec5:222e) | 233 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 014167 (0ec5:2317) | 23 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 01417e (0ec5:232e) | 32 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 01419e (0ec5:234e) | 155 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 014239 (0ec5:23e9) | 591 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 014488 (0ec5:2638) | 43 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 0144b3 (0ec5:2663) | 383 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 014632 (0ec5:27e2) | 24 | 2 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 01464a (0ec5:27fa) | 28 | 1 | int 1 | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 014666 (0ec5:2816) | 19 | 1 | int 1 | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 014679 (0ec5:2829) | 19 | 1 | int 1 | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 01468c (0ec5:283c) | 44 | 1 | io 5 | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 0146b8 (0ec5:2868) | 7 | 1 | io 2 | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 0146bf (0ec5:286f) | 155 | 1 | int 2 | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 01475a (0ec5:290a) | 57 | 34 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 014793 (0ec5:2943) | 54 | 5 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 0147c9 (0ec5:2979) | 36 | 46 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 0147ed (0ec5:299d) | 26 | 29 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 014807 (0ec5:29b7) | 44 | 3 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 014833 (0ec5:29e3) | 32 | 3 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 014853 (0ec5:2a03) | 385 | 2 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 0149d4 (0ec5:2b84) | 226 | 3 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 014ab6 (0ec5:2c66) | 73 | 0 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 014aff (0ec5:2caf) | 22 | 5 | int 1 | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 014b15 (0ec5:2cc5) | 7 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 014b1c (0ec5:2ccc) | 9 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 014b25 (0ec5:2cd5) | 8 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 014b2d (0ec5:2cdd) | 26 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 014b47 (0ec5:2cf7) | 13 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 014b54 (0ec5:2d04) | 9 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 014b5d (0ec5:2d0d) | 1175 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 014ff4 (0ec5:31a4) | 120 | 3 | int 3 | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 01506c (0ec5:321c) | 98 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 0150ce (0ec5:327e) | 113 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 01513f (0ec5:32ef) | 41 | 2 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 015168 (0ec5:3318) | 54 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 01519e (0ec5:334e) | 41 | 10 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 0151c7 (0ec5:3377) | 97 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 015228 (0ec5:33d8) | 57 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 015261 (0ec5:3411) | 35 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 015284 (0ec5:3434) | 125 | 13 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 015301 (0ec5:34b1) | 122 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 01537b (0ec5:352b) | 98 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 0153dd (0ec5:358d) | 110 | 0 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 01544b (0ec5:35fb) | 23 | 22 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 015462 (0ec5:3612) | 33 | 2 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 015483 (0ec5:3633) | 3 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 015486 (0ec5:3636) | 7 | 39 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 01548d (0ec5:363d) | 5 | 5 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 015492 (0ec5:3642) | 11 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 01549d (0ec5:364d) | 151 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 015534 (0ec5:36e4) | 33 | 9 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 015555 (0ec5:3705) | 32 | 3 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 015575 (0ec5:3725) | 30 | 7 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 015593 (0ec5:3743) | 96 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 0155f3 (0ec5:37a3) | 43 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 01561e (0ec5:37ce) | 34 | 39 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 015640 (0ec5:37f0) | 29 | 9 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 01565d (0ec5:380d) | 58 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 015697 (0ec5:3847) | 57 | 2 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 0156d0 (0ec5:3880) | 28 | 2 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 0156ec (0ec5:389c) | 35 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 01570f (0ec5:38bf) | 96 | 8 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 01576f (0ec5:391f) | 64 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 0157af (0ec5:395f) | 41 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 0157d8 (0ec5:3988) | 25 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 0157f1 (0ec5:39a1) | 53 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 015826 (0ec5:39d6) | 66 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 015868 (0ec5:3a18) | 2 | 3 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 01586a (0ec5:3a1a) | 81 | 0 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 0158bb (0ec5:3a6b) | 23 | 7 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 0158d2 (0ec5:3a82) | 33 | 2 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 0158f3 (0ec5:3aa3) | 4 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 0158f7 (0ec5:3aa7) | 4 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 0158fb (0ec5:3aab) | 4 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 0158ff (0ec5:3aaf) | 149 | 1 |  | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |
| 015994 (0ec5:3b44) | 273 | 1 | int 1 io 2 | notneeded | Borland C runtime (DOS file calls, malloc, string, long arithmetic, timer) |

## seg 128a (28 routines, 12812 bytes, 100% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 015aa5 (128a:0005) | 351 | 1 |  | proved | proof_pathfind.py build_corpse |
| 015c04 (128a:0164) | 298 | 1 |  | proved | proof_pathfind.py move_me_joe |
| 015d2e (128a:028e) | 217 | 1 |  | proved | proof_pathfind.py init_ai |
| 015e07 (128a:0367) | 202 | 1 |  | proved | proof_pathfind.py get_terrain |
| 015ed1 (128a:0431) | 454 | 0 |  | proved | proof_pathfind.py crit_hndlr_walk |
| 016097 (128a:05f7) | 7 | 0 |  | todo |  |
| 01609e (128a:05fe) | 96 | 0 |  | proved | proof_pathfind.py crit_hndlr_fly |
| 0160fe (128a:065e) | 109 | 0 |  | proved | proof_pathfind.py crit_hndlr_swim |
| 01616b (128a:06cb) | 43 | 2 |  | proved | proof_pathfind.py do_crit_phys |
| 016196 (128a:06f6) | 2400 | 3 |  | proved | proof_pathfind.py hyp_move |
| 016af6 (128a:1056) | 613 | 1 |  | proved | proof_pathfind.py hyp_see |
| 016d5b (128a:12bb) | 1416 | 2 |  | proved | proof_pathfind.py flood_path |
| 0172e3 (128a:1843) | 245 | 1 |  | proved | proof_pathfind.py make_path_from_flood_data |
| 0173d8 (128a:1938) | 665 | 1 |  | proved | proof_pathfind.py beeline |
| 017671 (128a:1bd1) | 1136 | 4 |  | proved | proof_pathfind.py line_of_sight |
| 017ae1 (128a:2041) | 400 | 1 |  | proved | proof_pathfind.py add_to_beeline_path |
| 017c71 (128a:21d1) | 57 | 1 |  | proved | proof_pathfind.py find_free_path |
| 017caa (128a:220a) | 58 | 2 |  | proved | proof_pathfind.py clear_paths |
| 017ce4 (128a:2244) | 391 | 1 |  | proved | proof_pathfind.py store_path |
| 017e6b (128a:23cb) | 221 | 2 |  | proved | proof_pathfind.py set_next_square_on_path |
| 017f48 (128a:24a8) | 92 | 1 |  | proved | proof_pathfind.py close_to_square |
| 017fa4 (128a:2504) | 373 | 1 |  | proved | proof_pathfind.py move_along_path |
| 018119 (128a:2679) | 489 | 1 |  | proved | proof_pathfind.py do_that_jump_kinda_thing |
| 018302 (128a:2862) | 136 | 12 |  | proved | proof_pathfind.py deltatotheta |
| 01838a (128a:28ea) | 183 | 5 |  | proved | proof_pathfind.py set_loc |
| 018441 (128a:29a1) | 1728 | 6 |  | proved | proof_pathfind.py crit_head_for_loc |
| 018b01 (128a:3061) | 194 | 2 |  | proved | proof_pathfind.py adjust_height |
| 018bc3 (128a:3123) | 238 | 1 |  | proved | proof_pathfind.py try_to_open_door |

## seg 15ab (34 routines, 14634 bytes, 100% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 018cb1 (15ab:0001) | 995 | 4 |  | proved | proof_ai.py crit_drunkwalk |
| 019094 (15ab:03e4) | 507 | 1 |  | proved | proof_ai.py seg007_1798_3E4 |
| 01928f (15ab:05df) | 209 | 2 |  | proved | proof_ai.py crit_mill |
| 019360 (15ab:06b0) | 481 | 2 |  | proved | proof_ai.py crit_guard |
| 019541 (15ab:0891) | 622 | 1 |  | proved | proof_ai.py crit_offense |
| 0197af (15ab:0aff) | 862 | 2 |  | proved | proof_ai.py crit_attack |
| 019b0d (15ab:0e5d) | 405 | 1 |  | proved | proof_ai.py crit_offense_find_target |
| 019ca2 (15ab:0ff2) | 173 | 3 |  | proved | proof_ai.py maybe_cast_defensive_spell |
| 019d4f (15ab:109f) | 348 | 2 |  | proved | proof_ai.py crit_magik_attack |
| 019eab (15ab:11fb) | 203 | 2 |  | proved | proof_ai.py crit_missile_attack |
| 019f76 (15ab:12c6) | 308 | 1 |  | proved | proof_ai.py crit_defense |
| 01a0aa (15ab:13fa) | 1059 | 2 |  | proved | proof_ai.py crit_flee |
| 01a4cd (15ab:181d) | 282 | 2 |  | proved | proof_ai.py crit_avoid_player |
| 01a5e7 (15ab:1937) | 447 | 1 |  | proved | proof_ai.py crit_talk |
| 01a7a6 (15ab:1af6) | 254 | 1 |  | proved | proof_ai.py check_out_player |
| 01a8a4 (15ab:1bf4) | 247 | 1 |  | proved | proof_ai.py crit_hover |
| 01a99b (15ab:1ceb) | 559 | 3 |  | proved | proof_ai.py target_found |
| 01abca (15ab:1f1a) | 194 | 3 |  | proved | proof_ai.py look_for_target |
| 01ac8c (15ab:1fdc) | 327 | 1 |  | proved | proof_ai.py constrain_movement |
| 01add3 (15ab:2123) | 414 | 1 |  | proved | proof_ai.py turn_real_fine |
| 01af71 (15ab:22c1) | 149 | 1 |  | proved | proof_ai.py compute_trz_or_try_rather |
| 01b006 (15ab:2356) | 306 | 1 |  | proved | proof_ai.py set_critter_vars |
| 01b138 (15ab:2488) | 1986 | 1 |  | proved | proof_ai.py critter_ai |
| 01b8fa (15ab:2c4a) | 1569 | 1 |  | proved | proof_ai.py critter_mv |
| 01bf1b (15ab:326b) | 280 | 5 |  | proved | proof_ai.py set_up_target |
| 01c033 (15ab:3383) | 135 | 1 |  | proved | proof_ai.py should_i_flee |
| 01c0ba (15ab:340a) | 132 | 1 |  | proved | proof_ai.py acceptable_danger |
| 01c13e (15ab:348e) | 111 | 8 |  | proved | proof_ai.py critter_set_goal |
| 01c1ad (15ab:34fd) | 122 | 2 |  | proved | proof_ai.py critter_discard_goal |
| 01c227 (15ab:3577) | 84 | 1 |  | proved | proof_ai.py go_into_dying_sequence |
| 01c27b (15ab:35cb) | 87 | 1 |  | proved | proof_ai.py crit_die |
| 01c2d2 (15ab:3622) | 515 | 1 |  | proved | proof_ai.py damage_critter |
| 01c4d5 (15ab:3825) | 67 | 1 |  | proved | proof_ai.py timetodo |
| 01c518 (15ab:3868) | 195 | 1 |  | proved | proof_ai.py move_mobile |

## seg 193d (15 routines, 3894 bytes, 100% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 01c5db (193d:000b) | 50 | 1 |  | proved | proof_physics.py water_set |
| 01c60d (193d:003d) | 275 | 5 |  | proved | proof_physics.py parse_player_terr |
| 01c720 (193d:0150) | 198 | 1 |  | proved | proof_physics.py EtherealVoidSpecialEffects_seg008_150 |
| 01c7e6 (193d:0216) | 973 | 1 |  | proved | proof_physics.py simple_fizix |
| 01cbb3 (193d:05e3) | 238 | 1 |  | proved | proof_physics.py set_player_phys_params |
| 01cca1 (193d:06d1) | 572 | 3 |  | proved | proof_physics.py player_setup |
| 01cedd (193d:090d) | 678 | 1 |  | proved | proof_physics.py phys_affect_player |
| 01d183 (193d:0bb3) | 57 | 0 |  | proved | proof_physics.py player_sqhandler |
| 01d1bc (193d:0bec) | 367 | 1 |  | proved | proof_physics.py do_player_input |
| 01d32b (193d:0d5b) | 42 | 1 |  | proved | proof_physics.py phys_bounce_up |
| 01d355 (193d:0d85) | 25 | 1 |  | proved | proof_physics.py fizix_update |
| 01d36e (193d:0d9e) | 56 | 1 |  | proved | proof_physics.py BouncePlayer_seg008_D9E |
| 01d3a6 (193d:0dd6) | 17 | 1 |  | proved | proof_physics.py DoTrapScreenShake_seg008_DD6 |
| 01d3b7 (193d:0de7) | 37 | 1 |  | proved | proof_physics.py QuakeTrap_seg008_DE7 |
| 01d3dc (193d:0e0c) | 309 | 4 |  | proved | proof_physics.py newFPS |

## seg 1a31 (10 routines, 995 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 01d511 (1a31:0001) | 108 | 4 |  | todo |  |
| 01d57d (1a31:006d) | 221 | 1 |  | todo |  |
| 01d65a (1a31:014a) | 82 | 1 |  | todo |  |
| 01d6ac (1a31:019c) | 133 | 1 |  | todo |  |
| 01d731 (1a31:0221) | 68 | 5 |  | todo |  |
| 01d775 (1a31:0265) | 97 | 22 |  | todo |  |
| 01d7d6 (1a31:02c6) | 102 | 0 |  | todo |  |
| 01d83c (1a31:032c) | 33 | 2 |  | todo |  |
| 01d85d (1a31:034d) | 63 | 1 |  | todo |  |
| 01d89c (1a31:038c) | 88 | 5 |  | todo |  |

## seg 1a6f (7 routines, 1139 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 01d8f4 (1a6f:0004) | 86 | 1 |  | todo |  |
| 01d94a (1a6f:005a) | 41 | 1 |  | todo |  |
| 01d973 (1a6f:0083) | 130 | 5 |  | todo |  |
| 01d9f5 (1a6f:0105) | 112 | 4 |  | todo |  |
| 01da65 (1a6f:0175) | 316 | 4 |  | todo |  |
| 01dba1 (1a6f:02b1) | 332 | 2 |  | todo |  |
| 01dced (1a6f:03fd) | 122 | 1 |  | todo |  |

## seg 1ab6 (3 routines, 180 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 01dd67 (1ab6:0007) | 37 | 1 |  | todo |  |
| 01dd8c (1ab6:002c) | 131 | 4 |  | todo |  |
| 01de0f (1ab6:00af) | 12 | 35 |  | todo |  |

## seg 1ac1 (8 routines, 435 bytes, 100% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 01de1b (1ac1:000b) | 155 | 1 | int 5 | notneeded | EMS memory (int 67h) |
| 01deb6 (1ac1:00a6) | 21 | 2 | int 1 | notneeded | EMS memory (int 67h) |
| 01decb (1ac1:00bb) | 84 | 6 | int 1 | notneeded | EMS memory (int 67h) |
| 01df1f (1ac1:010f) | 29 | 10 | int 1 | notneeded | EMS memory (int 67h) |
| 01df3c (1ac1:012c) | 21 | 1 | int 1 | notneeded | EMS memory (int 67h) |
| 01df51 (1ac1:0141) | 29 | 1 | int 1 | notneeded | EMS memory (int 67h) |
| 01df6e (1ac1:015e) | 83 | 2 | int 1 | notneeded | EMS memory (int 67h) |
| 01dfc1 (1ac1:01b1) | 13 | 1 | int 1 | notneeded | EMS memory (int 67h) |

## seg 1adc (40 routines, 4029 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 01dfce (1adc:000e) | 103 | 1 |  | todo |  |
| 01e035 (1adc:0075) | 76 | 3 |  | todo |  |
| 01e081 (1adc:00c1) | 22 | 59 |  | todo |  |
| 01e097 (1adc:00d7) | 33 | 62 |  | todo |  |
| 01e0b8 (1adc:00f8) | 29 | 1 |  | todo |  |
| 01e0d5 (1adc:0115) | 37 | 1 |  | todo |  |
| 01e0fa (1adc:013a) | 282 | 1 |  | todo |  |
| 01e214 (1adc:0254) | 101 | 1 |  | todo |  |
| 01e279 (1adc:02b9) | 21 | 10 |  | todo |  |
| 01e28e (1adc:02ce) | 42 | 1 |  | todo |  |
| 01e2b8 (1adc:02f8) | 18 | 2 |  | todo |  |
| 01e2ca (1adc:030a) | 14 | 1 |  | todo |  |
| 01e2d8 (1adc:0318) | 45 | 1 |  | todo |  |
| 01e305 (1adc:0345) | 34 | 5 |  | todo |  |
| 01e327 (1adc:0367) | 125 | 30 |  | todo |  |
| 01e3a4 (1adc:03e4) | 137 | 4 |  | todo |  |
| 01e42d (1adc:046d) | 29 | 3 |  | todo |  |
| 01e44a (1adc:048a) | 25 | 6 |  | todo |  |
| 01e463 (1adc:04a3) | 76 | 1 |  | todo |  |
| 01e4af (1adc:04ef) | 124 | 1 |  | todo |  |
| 01e52b (1adc:056b) | 153 | 4 |  | todo |  |
| 01e5c4 (1adc:0604) | 77 | 2 |  | todo |  |
| 01e611 (1adc:0651) | 13 | 15 |  | todo |  |
| 01e61e (1adc:065e) | 13 | 2 |  | todo |  |
| 01e62b (1adc:066b) | 113 | 2 |  | todo |  |
| 01e69c (1adc:06dc) | 94 | 1 |  | todo |  |
| 01e6fa (1adc:073a) | 50 | 16 |  | todo |  |
| 01e72c (1adc:076c) | 74 | 32 |  | todo |  |
| 01e776 (1adc:07b6) | 76 | 2 |  | todo |  |
| 01e7c2 (1adc:0802) | 102 | 4 |  | todo |  |
| 01e828 (1adc:0868) | 212 | 5 |  | todo |  |
| 01e8fc (1adc:093c) | 620 | 4 |  | todo |  |
| 01eb68 (1adc:0ba8) | 64 | 2 |  | todo |  |
| 01eba8 (1adc:0be8) | 71 | 2 |  | todo |  |
| 01ebef (1adc:0c2f) | 195 | 1 |  | todo |  |
| 01ecb2 (1adc:0cf2) | 100 | 2 |  | todo |  |
| 01ed16 (1adc:0d56) | 104 | 1 |  | todo |  |
| 01ed7e (1adc:0dbe) | 107 | 1 |  | todo |  |
| 01ede9 (1adc:0e29) | 356 | 2 |  | todo |  |
| 01ef4d (1adc:0f8d) | 62 | 4 |  | todo |  |

## seg 1bd8 (48 routines, 7522 bytes, 32% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 01ef8b (1bd8:000b) | 18 | 0 |  | todo |  |
| 01ef9d (1bd8:001d) | 68 | 1 |  | todo |  |
| 01efe1 (1bd8:0061) | 437 | 1 |  | todo |  |
| 01f196 (1bd8:0216) | 216 | 1 |  | todo |  |
| 01f26e (1bd8:02ee) | 211 | 1 |  | unproved |  |
| 01f341 (1bd8:03c1) | 91 | 4 |  | unproved |  |
| 01f39c (1bd8:041c) | 343 | 6 |  | proved | songs_verify.py load_new_music |
| 01f4f3 (1bd8:0573) | 154 | 2 |  | todo |  |
| 01f58d (1bd8:060d) | 85 | 1 |  | proved | songs_verify.py play_music |
| 01f5e2 (1bd8:0662) | 121 | 0 |  | todo |  |
| 01f65b (1bd8:06db) | 93 | 1 |  | todo |  |
| 01f6b8 (1bd8:0738) | 8 | 3 |  | unproved |  |
| 01f6c0 (1bd8:0740) | 20 | 2 |  | todo |  |
| 01f6d4 (1bd8:0754) | 20 | 2 |  | unproved |  |
| 01f6e8 (1bd8:0768) | 119 | 3 |  | proved | songs_verify.py turn_music |
| 01f75f (1bd8:07df) | 29 | 0 |  | proved | songs_verify.py toggle |
| 01f77c (1bd8:07fc) | 44 | 2 |  | todo |  |
| 01f7a8 (1bd8:0828) | 35 | 0 |  | todo |  |
| 01f7cb (1bd8:084b) | 37 | 1 |  | todo |  |
| 01f7f0 (1bd8:0870) | 62 | 0 |  | todo |  |
| 01f82e (1bd8:08ae) | 621 | 10 |  | proved | sfx_verify.py play_effect |
| 01fa9b (1bd8:0b1b) | 129 | 12 |  | proved | sfx_verify.py play_effect_here |
| 01fb1c (1bd8:0b9c) | 97 | 5 |  | todo |  |
| 01fb7d (1bd8:0bfd) | 127 | 1 |  | proved | sfx_verify.py kill_effect |
| 01fbfc (1bd8:0c7c) | 153 | 3 |  | proved | sfx_verify.py stop all effects |
| 01fc95 (1bd8:0d15) | 249 | 0 |  | proved | sfx_verify.py effects timer |
| 01fd8e (1bd8:0e0e) | 267 | 1 |  | unproved |  |
| 01fe99 (1bd8:0f19) | 80 | 1 |  | unproved |  |
| 01fee9 (1bd8:0f69) | 556 | 2 |  | unproved |  |
| 020115 (1bd8:1195) | 814 | 1 |  | todo | 250: You play the instrument.  (Use 0-9 to play, or ESC to r / 251: You put the instrument |
| 020443 (1bd8:14c3) | 139 | 1 |  | todo | 136: An object appears in the air and falls into your hand.. / |
| 0204ce (1bd8:154e) | 27 | 1 |  | todo |  |
| 0204e9 (1bd8:1569) | 81 | 1 |  | todo |  |
| 02053a (1bd8:15ba) | 11 | 9 |  | proved | songs_verify.py set_new_music |
| 020545 (1bd8:15c5) | 23 | 6 |  | proved | songs_verify.py wandering pick |
| 02055c (1bd8:15dc) | 49 | 7 |  | proved | songs_verify.py loop_music_maybe |
| 02058d (1bd8:160d) | 436 | 4 |  | proved | songs_verify.py change_music_maybe |
| 020741 (1bd8:17c1) | 44 | 2 |  | unproved |  |
| 02076d (1bd8:17ed) | 77 | 2 |  | todo |  |
| 0207ba (1bd8:183a) | 227 | 1 |  | todo |  |
| 02089d (1bd8:191d) | 8 | 1 |  | todo |  |
| 0208a5 (1bd8:1925) | 92 | 1 |  | unproved |  |
| 020901 (1bd8:1981) | 448 | 1 |  | todo |  |
| 020ac1 (1bd8:1b41) | 311 | 2 |  | todo |  |
| 020bf8 (1bd8:1c78) | 78 | 2 |  | todo |  |
| 020c46 (1bd8:1cc6) | 24 | 1 |  | todo |  |
| 020c5e (1bd8:1cde) | 47 | 1 |  | todo |  |
| 020c8d (1bd8:1d0d) | 96 | 1 |  | todo |  |

## seg 1dae (15 routines, 948 bytes, 100% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 020cee (1dae:000e) | 218 | 3 | io 4 | replaced | VGA palette and registers |
| 020dc8 (1dae:00e8) | 16 | 2 | int 1 | replaced | VGA palette and registers |
| 020dd8 (1dae:00f8) | 22 | 2 |  | replaced | VGA palette and registers |
| 020dee (1dae:010e) | 22 | 14 |  | replaced | VGA palette and registers |
| 020e04 (1dae:0124) | 32 | 2 |  | replaced | VGA palette and registers |
| 020e24 (1dae:0144) | 53 | 30 |  | replaced | VGA palette and registers |
| 020e59 (1dae:0179) | 40 | 3 |  | replaced | VGA palette and registers |
| 020e81 (1dae:01a1) | 91 | 1 |  | replaced | VGA palette and registers |
| 020edc (1dae:01fc) | 41 | 8 |  | replaced | VGA palette and registers |
| 020f05 (1dae:0225) | 53 | 5 | io 3 | replaced | VGA palette and registers |
| 020f3a (1dae:025a) | 77 | 3 | io 2 | replaced | VGA palette and registers |
| 020f87 (1dae:02a7) | 126 | 2 | io 5 | replaced | VGA palette and registers |
| 021005 (1dae:0325) | 65 | 2 | io 4 | replaced | VGA palette and registers |
| 021046 (1dae:0366) | 85 | 2 | io 2 | replaced | VGA palette and registers |
| 02109b (1dae:03bb) | 7 | 0 |  | replaced | VGA palette and registers |

## seg 1df0 (12 routines, 5157 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 021101 (1df0:0001) | 53 | 1 |  | todo |  |
| 021136 (1df0:0036) | 26 | 2 |  | todo |  |
| 021150 (1df0:0050) | 117 | 3 |  | todo |  |
| 0211c5 (1df0:00c5) | 638 | 1 |  | todo |  |
| 021443 (1df0:0343) | 483 | 1 |  | todo |  |
| 021626 (1df0:0526) | 628 | 2 |  | todo |  |
| 02189a (1df0:079a) | 230 | 1 |  | todo |  |
| 021980 (1df0:0880) | 228 | 0 |  | todo |  |
| 021a64 (1df0:0964) | 230 | 1 |  | todo |  |
| 021b4a (1df0:0a4a) | 465 | 1 |  | todo |  |
| 021d1b (1df0:0c1b) | 417 | 2 |  | todo |  |
| 021ebc (1df0:0dbc) | 1642 | 1 |  | todo |  |

## seg 1f32 (1 routines, 109 bytes, 100% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 022526 (1f32:0006) | 109 | 0 | io 1 | replaced | VGA register |

## seg 1f3a (81 routines, 4224 bytes, 94% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 0225a0 (1f3a:0000) | 23 | 1 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 0225b7 (1f3a:0017) | 78 | 1 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 022605 (1f3a:0065) | 60 | 1 | int 2 | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 022641 (1f3a:00a1) | 72 | 1 | int 2 | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 022689 (1f3a:00e9) | 41 | 1 | int 4 | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 0226b2 (1f3a:0112) | 9 | 1 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 0226bb (1f3a:011b) | 53 | 1 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 0226f0 (1f3a:0150) | 36 | 1 | int 2 | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 022714 (1f3a:0174) | 21 | 1 | int 1 | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 022729 (1f3a:0189) | 71 | 1 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 022770 (1f3a:01d0) | 88 | 2 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 0227c8 (1f3a:0228) | 14 | 1 | int 2 | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 0227d6 (1f3a:0236) | 14 | 2 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 0227e4 (1f3a:0244) | 64 | 1 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 022824 (1f3a:0284) | 50 | 1 | int 1 | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 022856 (1f3a:02b6) | 7 | 1 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 02285d (1f3a:02bd) | 47 | 1 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 02288c (1f3a:02ec) | 30 | 2 | io 1 | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 0228aa (1f3a:030a) | 107 | 2 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 022915 (1f3a:0375) | 11 | 1 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 022920 (1f3a:0380) | 15 | 1 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 02292f (1f3a:038f) | 20 | 1 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 022943 (1f3a:03a3) | 90 | 4 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 02299d (1f3a:03fd) | 87 | 2 | io 3 | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 0229f4 (1f3a:0454) | 23 | 1 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 022a0b (1f3a:046b) | 61 | 1 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 022a48 (1f3a:04a8) | 116 | 2 | int 1 | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 022abc (1f3a:051c) | 56 | 2 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 022af4 (1f3a:0554) | 16 | 1 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 022b04 (1f3a:0564) | 21 | 1 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 022b19 (1f3a:0579) | 31 | 2 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 022b38 (1f3a:0598) | 48 | 2 | int 2 | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 022b68 (1f3a:05c8) | 33 | 2 | int 1 | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 022b89 (1f3a:05e9) | 183 | 3 | int 1 io 13 | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 022c40 (1f3a:06a0) | 6 | 1 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 022c46 (1f3a:06a6) | 34 | 1 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 022c68 (1f3a:06c8) | 31 | 1 | int 1 | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 022c87 (1f3a:06e7) | 17 | 1 | int 1 | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 022c98 (1f3a:06f8) | 11 | 1 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 022ca3 (1f3a:0703) | 45 | 2 | int 1 | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 022cd0 (1f3a:0730) | 14 | 1 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 022cde (1f3a:073e) | 23 | 1 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 022cf5 (1f3a:0755) | 60 | 1 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 022d31 (1f3a:0791) | 60 | 1 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 022d6d (1f3a:07cd) | 60 | 0 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 022da9 (1f3a:0809) | 60 | 0 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 022de5 (1f3a:0845) | 60 | 0 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 022e21 (1f3a:0881) | 60 | 0 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 022e5d (1f3a:08bd) | 60 | 0 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 022e99 (1f3a:08f9) | 60 | 0 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 022ed5 (1f3a:0935) | 60 | 0 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 022f11 (1f3a:0971) | 60 | 2 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 022f4d (1f3a:09ad) | 67 | 2 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 022f90 (1f3a:09f0) | 64 | 1 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 022fd0 (1f3a:0a30) | 4 | 4 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 022fd4 (1f3a:0a34) | 4 | 5 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 022fd8 (1f3a:0a38) | 49 | 2 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 023009 (1f3a:0a69) | 15 | 1 |  | unproved |  |
| 023018 (1f3a:0a78) | 256 | 2 |  | unproved |  |
| 023118 (1f3a:0b78) | 42 | 1 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 023142 (1f3a:0ba2) | 50 | 1 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 023174 (1f3a:0bd4) | 44 | 1 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 0231a0 (1f3a:0c00) | 16 | 1 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 0231b0 (1f3a:0c10) | 36 | 1 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 0231d4 (1f3a:0c34) | 74 | 1 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 02321e (1f3a:0c7e) | 6 | 2 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 023224 (1f3a:0c84) | 51 | 4 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 023257 (1f3a:0cb7) | 51 | 0 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 02328a (1f3a:0cea) | 51 | 0 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 0232bd (1f3a:0d1d) | 93 | 1 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 02331a (1f3a:0d7a) | 160 | 3 | int 1 | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 0233ba (1f3a:0e1a) | 73 | 1 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 023403 (1f3a:0e63) | 75 | 2 |  | proved | sfx_verify.py sin/cos |
| 02344e (1f3a:0eae) | 77 | 4 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 02349b (1f3a:0efb) | 68 | 2 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 0234df (1f3a:0f3f) | 68 | 7 |  | proved | sfx_verify.py isqrt |
| 023523 (1f3a:0f83) | 90 | 3 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 02357d (1f3a:0fdd) | 39 | 1 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 0235a4 (1f3a:1004) | 39 | 1 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 0235cb (1f3a:102b) | 38 | 1 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |
| 0235f1 (1f3a:1051) | 47 | 0 |  | replaced | keyboard (int 16h, ports 60h/64h), BIOS video (int 10h), mouse (int 33h), timer |

## seg 2042 (77 routines, 2299 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 0239da (2042:03ba) | 56 | 2 |  | todo |  |
| 023a12 (2042:03f2) | 239 | 2 | io 1 | todo |  |
| 023b01 (2042:04e1) | 50 | 1 |  | todo |  |
| 023b33 (2042:0513) | 17 | 1 |  | todo |  |
| 023b44 (2042:0524) | 46 | 1 | int 2 | todo |  |
| 023b72 (2042:0552) | 10 | 1 |  | todo |  |
| 023b7c (2042:055c) | 28 | 1 | int 1 | todo |  |
| 023b98 (2042:0578) | 10 | 1 |  | todo |  |
| 023ba2 (2042:0582) | 30 | 2 | io 3 | todo |  |
| 023bc0 (2042:05a0) | 11 | 1 |  | todo |  |
| 023bcb (2042:05ab) | 42 | 1 |  | todo |  |
| 023bf5 (2042:05d5) | 75 | 1 |  | todo |  |
| 023c40 (2042:0620) | 139 | 1 |  | todo |  |
| 023ccb (2042:06ab) | 10 | 1 |  | todo |  |
| 023cd5 (2042:06b5) | 58 | 1 |  | todo |  |
| 023d0f (2042:06ef) | 10 | 1 |  | todo |  |
| 023d19 (2042:06f9) | 90 | 1 |  | todo |  |
| 023d73 (2042:0753) | 11 | 1 |  | todo |  |
| 023d7e (2042:075e) | 95 | 3 |  | todo |  |
| 023ddd (2042:07bd) | 10 | 0 |  | todo |  |
| 023de7 (2042:07c7) | 10 | 0 |  | todo |  |
| 023df1 (2042:07d1) | 17 | 0 |  | todo |  |
| 023e02 (2042:07e2) | 29 | 0 |  | todo |  |
| 023e1f (2042:07ff) | 11 | 1 |  | todo |  |
| 023e2a (2042:080a) | 36 | 4 |  | todo |  |
| 023e4e (2042:082e) | 21 | 0 |  | todo |  |
| 023e63 (2042:0843) | 11 | 1 |  | todo |  |
| 023e6e (2042:084e) | 20 | 1 |  | todo |  |
| 023e82 (2042:0862) | 10 | 1 |  | todo |  |
| 023e8c (2042:086c) | 29 | 5 |  | todo |  |
| 023ea9 (2042:0889) | 31 | 1 |  | todo |  |
| 023ec8 (2042:08a8) | 10 | 1 |  | todo |  |
| 023ed2 (2042:08b2) | 29 | 2 |  | todo |  |
| 023eef (2042:08cf) | 31 | 1 |  | todo |  |
| 023f0e (2042:08ee) | 10 | 1 |  | todo |  |
| 023f18 (2042:08f8) | 74 | 3 |  | todo |  |
| 023f62 (2042:0942) | 11 | 1 |  | todo |  |
| 023f6d (2042:094d) | 15 | 3 |  | todo |  |
| 023f7c (2042:095c) | 10 | 0 |  | todo |  |
| 023f86 (2042:0966) | 29 | 0 |  | todo |  |
| 023fa3 (2042:0983) | 11 | 1 |  | todo |  |
| 023fae (2042:098e) | 49 | 0 |  | todo |  |
| 023fdf (2042:09bf) | 21 | 1 |  | todo |  |
| 023ff4 (2042:09d4) | 10 | 1 |  | todo |  |
| 023ffe (2042:09de) | 127 | 2 |  | todo |  |
| 02407d (2042:0a5d) | 11 | 1 |  | todo |  |
| 024088 (2042:0a68) | 35 | 0 |  | todo |  |
| 0240ab (2042:0a8b) | 11 | 1 |  | todo |  |
| 0240b6 (2042:0a96) | 4 | 4 |  | todo |  |
| 0240ba (2042:0a9a) | 10 | 0 |  | todo |  |
| 0240c4 (2042:0aa4) | 24 | 0 |  | todo |  |
| 0240dc (2042:0abc) | 6 | 2 |  | todo |  |
| 0240e2 (2042:0ac2) | 85 | 2 |  | todo |  |
| 024137 (2042:0b17) | 76 | 0 |  | todo |  |
| 024183 (2042:0b63) | 11 | 1 |  | todo |  |
| 02418e (2042:0b6e) | 51 | 1 |  | todo |  |
| 0241c1 (2042:0ba1) | 6 | 1 |  | todo |  |
| 0241c7 (2042:0ba7) | 6 | 2 |  | todo |  |
| 0241cd (2042:0bad) | 18 | 2 |  | todo |  |
| 0241df (2042:0bbf) | 6 | 1 |  | todo |  |
| 0241e5 (2042:0bc5) | 42 | 1 |  | todo |  |
| 02420f (2042:0bef) | 6 | 1 |  | todo |  |
| 024215 (2042:0bf5) | 6 | 1 |  | todo |  |
| 02421b (2042:0bfb) | 6 | 1 |  | todo |  |
| 024221 (2042:0c01) | 6 | 1 |  | todo |  |
| 024227 (2042:0c07) | 6 | 1 |  | todo |  |
| 02422d (2042:0c0d) | 6 | 1 |  | todo |  |
| 024233 (2042:0c13) | 18 | 1 |  | todo |  |
| 024245 (2042:0c25) | 6 | 2 |  | todo |  |
| 02424b (2042:0c2b) | 6 | 3 |  | todo |  |
| 024251 (2042:0c31) | 12 | 3 |  | todo |  |
| 02425d (2042:0c3d) | 18 | 2 |  | todo |  |
| 02426f (2042:0c4f) | 6 | 3 |  | todo |  |
| 024275 (2042:0c55) | 42 | 1 |  | todo |  |
| 02429f (2042:0c7f) | 30 | 5 |  | todo |  |
| 0242bd (2042:0c9d) | 12 | 2 |  | todo |  |
| 0242c9 (2042:0ca9) | 12 | 4 |  | todo |  |

## seg 210d (2 routines, 325 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 0242d5 (210d:0005) | 162 | 3 |  | todo |  |
| 024377 (210d:00a7) | 163 | 1 |  | todo |  |

## seg 2121 (20 routines, 6051 bytes, 100% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 02441a (2121:000a) | 121 | 2 |  | proved | proof_combat.py pickloc |
| 024493 (2121:0083) | 591 | 1 |  | proved | proof_combat.py set_hitobj |
| 0246e2 (2121:02d2) | 404 | 1 |  | proved | proof_combat.py find_wall_coll |
| 024876 (2121:0466) | 595 | 1 |  | proved | proof_combat.py resolve_attack |
| 024ac9 (2121:06b9) | 492 | 1 |  | proved | proof_combat.py frp_check |
| 024cb5 (2121:08a5) | 900 | 2 |  | proved | proof_combat.py do_damage |
| 025039 (2121:0c29) | 287 | 1 |  | proved | proof_combat.py do_miss |
| 025158 (2121:0d48) | 84 | 1 |  | proved | proof_combat.py compute_hitangle |
| 0251ac (2121:0d9c) | 220 | 2 |  | proved | proof_combat.py do_attack |
| 025288 (2121:0e78) | 158 | 2 |  | proved | proof_combat.py check_ammo |
| 025326 (2121:0f16) | 224 | 1 |  | proved | proof_combat.py GetPlayerWeapon |
| 025406 (2121:0ff6) | 288 | 1 |  | proved | proof_combat.py DoPlayerWeapon |
| 025526 (2121:1116) | 60 | 1 |  | proved | proof_combat.py missile_finish |
| 025562 (2121:1152) | 49 | 2 |  | proved | proof_combat.py fin_attack |
| 025593 (2121:1183) | 55 | 4 |  | proved | proof_combat.py clear_fight_state |
| 0255ca (2121:11ba) | 773 | 2 |  | proved | proof_combat.py player_attack |
| 0258cf (2121:14bf) | 287 | 1 |  | proved | proof_combat.py missile_thwack |
| 0259ee (2121:15de) | 279 | 1 |  | proved | proof_combat.py critter_attack |
| 025b05 (2121:16f5) | 48 | 0 |  | proved | proof_combat.py seg022_230E_16F5 |
| 025b35 (2121:1725) | 136 | 1 |  | proved | verify.py player_killed_a (killexp, 1500 cases same) |

## seg 229b (6 routines, 1346 bytes, 7% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 025bbd (229b:000d) | 142 | 1 |  | todo |  |
| 025c4b (229b:009b) | 59 | 1 |  | todo |  |
| 025c86 (229b:00d6) | 553 | 1 |  | todo |  |
| 025eaf (229b:02ff) | 91 | 7 |  | proved | verify.py resistance |
| 025f0a (229b:035a) | 141 | 15 |  | unproved |  |
| 025f97 (229b:03e7) | 360 | 1 |  | todo |  |

## seg 22ef (36 routines, 5742 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 0260ff (22ef:000f) | 35 | 1 |  | todo |  |
| 026122 (22ef:0032) | 221 | 0 |  | unproved | 69: It has been an uncountable number of days since you ente / |
| 0261ff (22ef:010f) | 352 | 0 |  | todo |  |
| 02635f (22ef:026f) | 162 | 1 |  | todo |  |
| 026401 (22ef:0311) | 60 | 1 |  | todo |  |
| 02643d (22ef:034d) | 68 | 1 |  | todo |  |
| 026481 (22ef:0391) | 19 | 1 |  | todo |  |
| 026494 (22ef:03a4) | 99 | 2 |  | todo |  |
| 0264f7 (22ef:0407) | 255 | 1 |  | unproved |  |
| 0265f6 (22ef:0506) | 37 | 1 |  | todo |  |
| 02661b (22ef:052b) | 306 | 4 |  | todo |  |
| 02674d (22ef:065d) | 91 | 1 |  | todo |  |
| 0267a8 (22ef:06b8) | 530 | 3 |  | todo |  |
| 0269ba (22ef:08ca) | 262 | 1 |  | todo |  |
| 026ac0 (22ef:09d0) | 126 | 2 |  | todo |  |
| 026b3e (22ef:0a4e) | 80 | 1 |  | todo |  |
| 026b8e (22ef:0a9e) | 590 | 1 |  | todo | 95: That is too heavy for you to pick up.\n / 96: You cannot pick that up.\n / |
| 026ddc (22ef:0cec) | 30 | 1 |  | todo |  |
| 026dfa (22ef:0d0a) | 22 | 3 |  | todo |  |
| 026e10 (22ef:0d20) | 287 | 0 |  | todo |  |
| 026f2f (22ef:0e3f) | 122 | 2 |  | todo | 185: You are unable to use that from here.\n / |
| 026fa9 (22ef:0eb9) | 70 | 0 |  | unproved |  |
| 026fef (22ef:0eff) | 412 | 0 |  | unproved | 94: You cannot reach that.\n / |
| 02718b (22ef:109b) | 277 | 1 |  | todo |  |
| 0272a0 (22ef:11b0) | 14 | 0 |  | todo |  |
| 0272ae (22ef:11be) | 65 | 0 |  | todo |  |
| 0272ef (22ef:11ff) | 286 | 1 |  | todo |  |
| 02740d (22ef:131d) | 44 | 0 |  | todo |  |
| 027439 (22ef:1349) | 70 | 4 |  | todo |  |
| 02747f (22ef:138f) | 70 | 3 |  | todo |  |
| 0274c5 (22ef:13d5) | 348 | 0 |  | unproved |  |
| 027621 (22ef:1531) | 143 | 1 |  | unproved |  |
| 0276b0 (22ef:15c0) | 72 | 5 |  | todo |  |
| 0276f8 (22ef:1608) | 31 | 2 |  | todo |  |
| 027717 (22ef:1627) | 57 | 1 |  | todo |  |
| 027750 (22ef:1660) | 29 | 1 |  | todo |  |

## seg 2456 (8 routines, 3367 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 02776d (2456:000d) | 146 | 3 |  | todo |  |
| 0277ff (2456:009f) | 451 | 1 |  | todo | 254: You need more space to fire that weapon.\n / |
| 0279c2 (2456:0262) | 79 | 1 |  | todo |  |
| 027a11 (2456:02b1) | 164 | 1 |  | todo |  |
| 027ab5 (2456:0355) | 999 | 1 |  | todo | 253: There is no space to drop that.\n / |
| 027e9c (2456:073c) | 85 | 1 |  | todo |  |
| 027ef1 (2456:0791) | 960 | 5 |  | todo |  |
| 0282b1 (2456:0b51) | 483 | 1 |  | todo |  |

## seg 2529 (14 routines, 5303 bytes, 100% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 028494 (2529:0004) | 417 | 2 |  | proved | collide_verify.py GetHgt (inside TerrainCheck: its every result compared) |
| 028635 (2529:01a5) | 104 | 1 |  | proved | collide_verify.py GetSlopeHgt |
| 02869d (2529:020d) | 183 | 1 |  | proved | collide_verify.py SolvePnt (inside TerrainCheck) |
| 028754 (2529:02c4) | 181 | 1 |  | proved | collide_verify.py SolveCenter (inside TerrainCheck) |
| 028809 (2529:0379) | 1149 | 9 |  | proved | collide_verify.py TerrainCheck (17596 checks on 9 levels same) |
| 028c86 (2529:07f6) | 618 | 1 |  | proved | collide_verify.py ComputeHeading |
| 028ef0 (2529:0a60) | 406 | 1 |  | proved | collide_verify.py obj_coll_check (inside ObjectCheck: every record compared) |
| 029086 (2529:0bf6) | 612 | 8 |  | proved | collide_verify.py ObjectCheck (13500 checks around real objects on 9 levels same) |
| 0292ea (2529:0e5a) | 101 | 1 |  | proved | collide_verify.py oCswap (inside process_objlist) |
| 02934f (2529:0ebf) | 329 | 7 |  | proved | collide_verify.py process_objlist |
| 029498 (2529:1008) | 547 | 11 |  | proved | collide_verify.py can_place (13500 same: ok, nvokHgt, nvokTerr) |
| 0296bb (2529:122b) | 186 | 7 |  | proved | proof_collide.py put_at |
| 029775 (2529:12e5) | 90 | 6 |  | proved | proof_collide.py near_mob_put_at |
| 0297cf (2529:133f) | 380 | 2 |  | proved | proof_collide.py drop_around_place |

## seg 2674 (31 routines, 4673 bytes, 97% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 02994b (2674:000b) | 268 | 2 |  | proved | proof_objects.py Map_ObjFix |
| 029a57 (2674:0117) | 143 | 3 |  | proved | proof_objects.py Obj_Check |
| 029ae6 (2674:01a6) | 128 | 1 |  | proved | proof_objects.py chkTenacious |
| 029b66 (2674:0226) | 195 | 6 |  | proved | proof_objects.py Obj_Elem_Fate |
| 029c29 (2674:02e9) | 64 | 1 |  | proved | proof_objects.py Obj_Fate |
| 029c69 (2674:0329) | 285 | 2 |  | proved | proof_objects.py Obj_GarbageCollect |
| 029d86 (2674:0446) | 151 | 12 |  | proved | proof_objects.py Obj_Alloc |
| 029e1d (2674:04dd) | 132 | 35 |  | proved | proof_objects.py Obj_Free |
| 029ea1 (2674:0561) | 110 | 38 |  | proved | proof_objects.py Obj_Add |
| 029f0f (2674:05cf) | 138 | 9 |  | proved | proof_objects.py Obj_AddEnd |
| 029f99 (2674:0659) | 132 | 35 |  | proved | proof_objects.py Obj_Rem |
| 02a01d (2674:06dd) | 149 | 15 |  | proved | proof_objects.py Obj_Punt |
| 02a0b2 (2674:0772) | 191 | 8 |  | proved | proof_objects.py Obj_FreeChain |
| 02a171 (2674:0831) | 92 | 7 |  | proved | proof_objects.py Obj_FreeLinkChain |
| 02a1cd (2674:088d) | 105 | 82 |  | proved | proof_objects.py Obj_PtrTMem |
| 02a236 (2674:08f6) | 74 | 66 |  | proved | proof_objects.py Obj_MemTPtr |
| 02a280 (2674:0940) | 65 | 52 |  | proved | proof_objects.py Obj_IntTMem |
| 02a2c1 (2674:0981) | 209 | 6 |  | proved | proof_objects.py Obj_Find |
| 02a392 (2674:0a52) | 30 | 19 |  | proved | proof_objects.py IsMobElem |
| 02a3b0 (2674:0a70) | 19 | 1 |  | proved | proof_objects.py active_critter |
| 02a3c3 (2674:0a83) | 76 | 1 |  | proved | proof_objects.py free_critter |
| 02a40f (2674:0acf) | 103 | 0 |  | todo |  |
| 02a476 (2674:0b36) | 256 | 18 |  | proved | proof_objects.py Obj_InList |
| 02a576 (2674:0c36) | 105 | 2 |  | proved | proof_objects.py HasOrIsObj |
| 02a5df (2674:0c9f) | 165 | 3 |  | proved | proof_objects.py Obj_FindInMap |
| 02a684 (2674:0d44) | 233 | 2 |  | proved | proof_objects.py flog_list |
| 02a76d (2674:0e2d) | 204 | 2 |  | proved | proof_objects.py count_list |
| 02a839 (2674:0ef9) | 53 | 1 |  | proved | proof_objects.py seg027_2861_EF9 |
| 02a86e (2674:0f2e) | 41 | 0 |  | todo |  |
| 02a897 (2674:0f57) | 397 | 2 |  | proved | proof_objects.py Obj_ListOkay |
| 02aa24 (2674:10e4) | 360 | 1 |  | proved | proof_objects.py seg027_2861_10E4 |

## seg 2798 (5 routines, 1671 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 02ab8c (2798:000c) | 305 | 2 |  | todo |  |
| 02acbd (2798:013d) | 633 | 1 |  | unproved |  |
| 02af36 (2798:03b6) | 248 | 2 |  | unproved |  |
| 02b02e (2798:04ae) | 219 | 1 |  | unproved |  |
| 02b109 (2798:0589) | 266 | 2 |  | unproved |  |

## seg 2801 (10 routines, 5000 bytes, 100% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 02b213 (2801:0003) | 170 | 1 |  | proved | proof_objphys.py bounce_obj |
| 02b2bd (2801:00ad) | 198 | 1 |  | proved | proof_objphys.py missile_newhit |
| 02b383 (2801:0173) | 601 | 2 |  | proved | proof_objphys.py do_objhit |
| 02b5dc (2801:03cc) | 590 | 3 |  | proved | proof_objphys.py get_phys_data |
| 02b82a (2801:061a) | 1029 | 3 |  | proved | proof_objphys.py set_phys_data |
| 02bc2f (2801:0a1f) | 283 | 2 |  | proved | proof_objphys.py static_to_mob |
| 02bd4a (2801:0b3a) | 309 | 2 |  | proved | proof_objphys.py mob_init |
| 02be7f (2801:0c6f) | 924 | 2 |  | proved | proof_objphys.py mob_to_static
 278: A rending sound fills the air.\n / |
| 02c21b (2801:100b) | 66 | 1 |  | proved | proof_objphys.py update_hack_vecs |
| 02c25d (2801:104d) | 830 | 9 |  | proved | proof_objphys.py obj_deal |

## seg 2939 (20 routines, 6162 bytes, 100% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 02c59b (2939:000b) | 134 | 2 |  | proved | proof_motion.py do_physics |
| 02c621 (2939:0091) | 123 | 1 |  | proved | proof_motion.py space_to_pos |
| 02c69c (2939:010c) | 772 | 3 |  | proved | proof_motion.py set_targz |
| 02c9a0 (2939:0410) | 888 | 2 |  | proved | proof_motion.py space_to_motion |
| 02cd18 (2939:0788) | 53 | 3 |  | proved | proof_motion.py recalc_vecs |
| 02cd4d (2939:07bd) | 65 | 2 |  | proved | proof_motion.py stop_me |
| 02cd8e (2939:07fe) | 166 | 1 |  | proved | proof_motion.py back_to_space |
| 02ce34 (2939:08a4) | 409 | 2 |  | proved | proof_motion.py flat_move |
| 02cfcd (2939:0a3d) | 10 | 0 |  | todo |  |
| 02cfd7 (2939:0a47) | 408 | 1 |  | proved | proof_motion.py rehead |
| 02d16f (2939:0bdf) | 106 | 1 |  | proved | proof_motion.py do_2dbounce |
| 02d1d9 (2939:0c49) | 74 | 1 |  | proved | proof_motion.py bounce_that_guy |
| 02d223 (2939:0c93) | 844 | 1 |  | proved | proof_motion.py do_zbounce |
| 02d56f (2939:0fdf) | 384 | 1 |  | proved | proof_motion.py full_move |
| 02d6ef (2939:115f) | 147 | 3 |  | proved | proof_motion.py grid_move |
| 02d782 (2939:11f2) | 103 | 3 |  | proved | proof_motion.py set_resterr |
| 02d7e9 (2939:1259) | 999 | 2 |  | proved | proof_motion.py get_pcoll |
| 02dbd0 (2939:1640) | 204 | 1 |  | proved | proof_motion.py check_positions |
| 02dc9c (2939:170c) | 233 | 1 |  | proved | proof_motion.py IsaDoor |
| 02dd85 (2939:17f5) | 40 | 3 |  | proved | proof_motion.py CollObject |

## seg 2aba (21 routines, 4648 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 02ddad (2aba:000d) | 140 | 1 |  | todo |  |
| 02de39 (2aba:0099) | 240 | 1 |  | todo |  |
| 02df29 (2aba:0189) | 10 | 0 |  | todo |  |
| 02df33 (2aba:0193) | 105 | 0 |  | todo |  |
| 02df9c (2aba:01fc) | 74 | 1 |  | todo |  |
| 02dfe6 (2aba:0246) | 19 | 3 |  | todo |  |
| 02dff9 (2aba:0259) | 77 | 1 |  | todo |  |
| 02e046 (2aba:02a6) | 83 | 1 |  | todo |  |
| 02e099 (2aba:02f9) | 88 | 6 |  | todo |  |
| 02e0f1 (2aba:0351) | 69 | 2 |  | todo |  |
| 02e136 (2aba:0396) | 277 | 2 |  | todo |  |
| 02e24b (2aba:04ab) | 265 | 2 |  | todo |  |
| 02e354 (2aba:05b4) | 203 | 1 |  | todo |  |
| 02e41f (2aba:067f) | 38 | 4 |  | todo |  |
| 02e445 (2aba:06a5) | 38 | 3 |  | todo |  |
| 02e46b (2aba:06cb) | 1062 | 3 |  | todo |  |
| 02e891 (2aba:0af1) | 424 | 1 |  | todo |  |
| 02ea39 (2aba:0c99) | 887 | 1 |  | todo |  |
| 02edb0 (2aba:1010) | 334 | 1 |  | todo |  |
| 02eefe (2aba:115e) | 188 | 1 |  | todo |  |
| 02efba (2aba:121a) | 27 | 2 |  | todo |  |

## seg 2bdd (3 routines, 4692 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 02efd5 (2bdd:0005) | 1426 | 1 |  | todo |  |
| 02f567 (2bdd:0597) | 1727 | 2 |  | todo |  |
| 02fc26 (2bdd:0c56) | 1539 | 1 |  | todo |  |

## seg 2d02 (10 routines, 2455 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 030229 (2d02:0009) | 228 | 1 |  | todo |  |
| 03030d (2d02:00ed) | 86 | 1 |  | todo |  |
| 030363 (2d02:0143) | 27 | 1 |  | todo |  |
| 03037e (2d02:015e) | 213 | 2 |  | todo |  |
| 030453 (2d02:0233) | 78 | 1 |  | todo |  |
| 0304a1 (2d02:0281) | 168 | 1 |  | todo |  |
| 030549 (2d02:0329) | 164 | 1 |  | todo |  |
| 0305ed (2d02:03cd) | 57 | 1 |  | todo |  |
| 030626 (2d02:0406) | 53 | 1 |  | todo |  |
| 03065b (2d02:043b) | 1381 | 2 |  | todo |  |

## seg 2d9c (13 routines, 3753 bytes, 94% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 030bc0 (2d9c:0000) | 78 | 1 |  | proved | proof_playmove.py player_mous_move |
| 030c0e (2d9c:004e) | 396 | 1 |  | proved | proof_playmove.py parse_playin |
| 030d9a (2d9c:01da) | 346 | 2 |  | proved | proof_playmove.py do_player_keyboard |
| 030ef4 (2d9c:0334) | 210 | 0 |  | todo |  |
| 030fc6 (2d9c:0406) | 274 | 0 |  | proved | proof_playmove.py check_physics |
| 0310d8 (2d9c:0518) | 172 | 2 |  | proved | proof_playmove.py move_physics |
| 031184 (2d9c:05c4) | 64 | 1 |  | proved | proof_playmove.py finish_player |
| 0311c4 (2d9c:0604) | 271 | 2 |  | proved | proof_playmove.py move_player |
| 0312d3 (2d9c:0713) | 389 | 1 |  | proved | proof_playmove.py make_noise |
| 031458 (2d9c:0898) | 165 | 1 |  | proved | proof_playmove.py set_sound |
| 0314fd (2d9c:093d) | 557 | 2 |  | proved | proof_playmove.py parse_effect |
| 03172a (2d9c:0b6a) | 47 | 5 |  | proved | proof_playmove.py set_effect |
| 031759 (2d9c:0b99) | 784 | 1 |  | proved | proof_playmove.py get_eye |

## seg 2e86 (2 routines, 325 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 031a69 (2e86:0009) | 53 | 62 |  | todo |  |
| 031a9e (2e86:003e) | 272 | 21 |  | todo |  |

## seg 2e9a (33 routines, 9710 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 031bae (2e9a:000e) | 165 | 3 |  | todo |  |
| 031c53 (2e9a:00b3) | 84 | 2 |  | todo |  |
| 031ca7 (2e9a:0107) | 103 | 1 |  | todo |  |
| 031d0e (2e9a:016e) | 591 | 2 |  | todo |  |
| 031f5d (2e9a:03bd) | 24 | 1 |  | todo |  |
| 031f75 (2e9a:03d5) | 10 | 1 |  | todo |  |
| 031f7f (2e9a:03df) | 454 | 17 |  | todo |  |
| 032145 (2e9a:05a5) | 327 | 0 |  | todo |  |
| 03228c (2e9a:06ec) | 820 | 0 |  | todo |  |
| 0325c0 (2e9a:0a20) | 1632 | 0 | io 1 | todo |  |
| 032c20 (2e9a:1080) | 141 | 0 |  | todo |  |
| 032cad (2e9a:110d) | 166 | 0 |  | todo |  |
| 032d53 (2e9a:11b3) | 84 | 0 |  | todo |  |
| 032da7 (2e9a:1207) | 191 | 0 |  | todo |  |
| 032e66 (2e9a:12c6) | 88 | 0 |  | todo |  |
| 032ebe (2e9a:131e) | 41 | 0 |  | todo |  |
| 032ee7 (2e9a:1347) | 37 | 1 |  | todo |  |
| 032f0c (2e9a:136c) | 348 | 1 |  | unproved |  |
| 033068 (2e9a:14c8) | 97 | 1 |  | todo |  |
| 0330c9 (2e9a:1529) | 412 | 0 |  | unproved |  |
| 033265 (2e9a:16c5) | 173 | 3 |  | todo |  |
| 033312 (2e9a:1772) | 169 | 3 |  | todo |  |
| 0333bb (2e9a:181b) | 182 | 1 |  | todo |  |
| 033471 (2e9a:18d1) | 340 | 1 |  | todo |  |
| 0335c5 (2e9a:1a25) | 123 | 2 |  | todo |  |
| 033640 (2e9a:1aa0) | 53 | 2 |  | todo |  |
| 033675 (2e9a:1ad5) | 1638 | 1 |  | todo |  |
| 033cdb (2e9a:213b) | 468 | 1 |  | todo |  |
| 033eaf (2e9a:230f) | 334 | 1 |  | todo |  |
| 033ffd (2e9a:245d) | 349 | 2 |  | unproved |  |
| 03415a (2e9a:25ba) | 21 | 6 |  | todo |  |
| 03416f (2e9a:25cf) | 20 | 1 |  | todo |  |
| 034183 (2e9a:25e3) | 25 | 1 |  | todo |  |

## seg 30f9 (3 routines, 520 bytes, 100% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 03419c (30f9:000c) | 60 | 16 |  | proved | verify.py skill check |
| 0341d8 (30f9:0048) | 369 | 5 |  | proved | verify.py player_get_exp (getexp, 1500 cases same) |
| 034349 (30f9:01b9) | 91 | 5 |  | replaced | the |

## seg 311a (32 routines, 6367 bytes, 100% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 0343a4 (311a:0004) | 35 | 4 |  | proved | proof_spells.py anti_magic_p |
| 0343c7 (311a:0027) | 81 | 2 |  | proved | proof_spells.py cast |
| 034418 (311a:0078) | 574 | 3 | io 2 | proved | proof_spells.py do_spell
 228: Your vision distorts and you feel light headed.\n / |
| 034656 (311a:02b6) | 108 | 4 |  | proved | proof_spells.py restore_mana |
| 0346c2 (311a:0322) | 152 | 3 |  | proved | proof_spells.py restore_hp |
| 03475a (311a:03ba) | 114 | 2 |  | proved | proof_spells.py get_hp_back |
| 0347cc (311a:042c) | 71 | 1 |  | proved | proof_spells.py healing |
| 034813 (311a:0473) | 123 | 4 |  | proved | proof_spells.py backfire |
| 03488e (311a:04ee) | 118 | 2 |  | proved | proof_spells.py release_missile
 255: There is not enough room to release that spell.\n / |
| 034904 (311a:0564) | 95 | 2 |  | proved | proof_spells.py build_new_obj |
| 034963 (311a:05c3) | 171 | 0 |  | proved | proof_spells.py sp_true_sight |
| 034a0e (311a:066e) | 141 | 0 |  | proved | proof_spells.py sp_sheet_light |
| 034a9b (311a:06fb) | 239 | 0 |  | proved | proof_spells.py sp_meteor |
| 034b8a (311a:07ea) | 74 | 0 |  | proved | proof_spells.py sp_ward_undead |
| 034bd4 (311a:0834) | 85 | 0 |  | proved | proof_spells.py sp_poison |
| 034c29 (311a:0889) | 111 | 3 |  | proved | proof_spells.py hit_critter_goal |
| 034c98 (311a:08f8) | 123 | 0 |  | proved | proof_spells.py sp_charm |
| 034d13 (311a:0973) | 28 | 0 |  | proved | proof_spells.py sp_confusion |
| 034d2f (311a:098f) | 28 | 0 |  | proved | proof_spells.py sp_fear |
| 034d4b (311a:09ab) | 28 | 0 |  | proved | proof_spells.py sp_hold |
| 034d67 (311a:09c7) | 688 | 2 |  | proved | proof_spells.py process_area |
| 035017 (311a:0c77) | 215 | 6 |  | proved | proof_spells.py gronk_area |
| 0350ee (311a:0d4e) | 108 | 4 |  | proved | proof_spells.py gronk_whoami |
| 03515a (311a:0dba) | 70 | 1 |  | proved | proof_spells.py nail_area |
| 0351a0 (311a:0e00) | 69 | 1 |  | proved | proof_spells.py nail_1area |
| 0351e5 (311a:0e45) | 972 | 1 |  | proved | proof_spells.py creat_spell |
| 0355b1 (311a:1211) | 236 | 0 |  | proved | proof_spells.py tremor_area |
| 03569d (311a:12fd) | 81 | 1 |  | proved | proof_spells.py print_monster |
| 0356ee (311a:134e) | 530 | 2 |  | proved | proof_spells.py mdetect
 62: You detect no monster activity.\n / |
| 035900 (311a:1560) | 229 | 0 |  | proved | proof_spells.py obj_spells
 271: The spell has no discernable effect.\n / |
| 0359e5 (311a:1645) | 485 | 1 |  | proved | proof_spells.py xt_spells
 273: The moonstone is not available.\n / |
| 035bca (311a:182a) | 185 | 3 |  | proved | proof_spells.py damage_square |

## seg 32a8 (19 routines, 2399 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 035c83 (32a8:0003) | 128 | 1 |  | todo |  |
| 035d03 (32a8:0083) | 10 | 1 |  | todo |  |
| 035d0d (32a8:008d) | 137 | 54 |  | todo |  |
| 035d96 (32a8:0116) | 260 | 7 |  | todo |  |
| 035e9a (32a8:021a) | 123 | 1 |  | todo |  |
| 035f15 (32a8:0295) | 145 | 1 |  | todo |  |
| 035fa6 (32a8:0326) | 184 | 17 |  | todo |  |
| 03605e (32a8:03de) | 182 | 3 |  | todo |  |
| 036114 (32a8:0494) | 41 | 61 |  | todo |  |
| 03613d (32a8:04bd) | 122 | 2 |  | todo |  |
| 0361b7 (32a8:0537) | 173 | 1 |  | todo |  |
| 036264 (32a8:05e4) | 32 | 1 |  | todo |  |
| 036284 (32a8:0604) | 384 | 1 |  | todo |  |
| 036404 (32a8:0784) | 49 | 1 |  | todo |  |
| 036435 (32a8:07b5) | 98 | 1 |  | todo |  |
| 036497 (32a8:0817) | 67 | 2 |  | todo |  |
| 0364da (32a8:085a) | 67 | 2 |  | todo |  |
| 03651d (32a8:089d) | 113 | 1 |  | todo |  |
| 03658e (32a8:090e) | 84 | 14 |  | todo |  |

## seg 333e (39 routines, 10224 bytes, 100% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 0365e2 (333e:0002) | 850 | 7 | io 2 | proved | proof_useitems.py UseObj
 9: As you reach for the tree, it withers away, revealing a s / |
| 036934 (333e:0354) | 228 | 9 |  | proved | proof_useitems.py using_punt |
| 036a18 (333e:0438) | 96 | 4 |  | proved | proof_useitems.py place_new |
| 036a78 (333e:0498) | 133 | 0 |  | proved | proof_useitems.py UseLockpickOn
 121: You succeed in picking the lock.\n / |
| 036afd (333e:051d) | 88 | 0 |  | proved | proof_useitems.py UseKeyOn |
| 036b55 (333e:0575) | 69 | 1 |  | proved | proof_useitems.py UseKey |
| 036b9a (333e:05ba) | 168 | 6 |  | proved | proof_useitems.py UseThing |
| 036c42 (333e:0662) | 151 | 0 |  | proved | proof_useitems.py SpikeDoor_seg040_662
 128: You can only spike closed doors.\n / 129: The door is now spiked closed.\n / |
| 036cd9 (333e:06f9) | 32 | 1 |  | proved | proof_useitems.py SpikeDoor_seg040_352B_6F9 |
| 036cf9 (333e:0719) | 361 | 0 |  | proved | proof_useitems.py UseBonesOn
 132: It seems to have no effect.\n / 134: You thoughtfully give the bones a final resting |
| 036e62 (333e:0882) | 77 | 0 |  | proved | proof_useitems.py UsePoleOn
 157: Using the pole you trigger the switch.\n / 158: The pole cannot be used on that.\n / |
| 036eaf (333e:08cf) | 74 | 0 |  | proved | proof_useitems.py UseAnvilOn |
| 036ef9 (333e:0919) | 209 | 1 |  | proved | proof_useitems.py UseUtil |
| 036fca (333e:09ea) | 40 | 0 |  | proved | proof_useitems.py seg040_352B_9EA |
| 036ff2 (333e:0a12) | 237 | 1 |  | proved | proof_useitems.py TybalsOrb_seg040_A12
 132: It seems to have no effect.\n / 133: The orb is destroyed!\n / |
| 0370df (333e:0aff) | 133 | 0 |  | proved | proof_useitems.py seg040_352B_AFF
 132: It seems to have no effect.\n / |
| 037164 (333e:0b84) | 258 | 1 |  | proved | proof_useitems.py UseUnique |
| 037266 (333e:0c86) | 411 | 1 |  | proved | proof_useitems.py UseLight
 123: Lights may only be used if equipped.\n / 124: That light is already used up.\n / 246: |
| 037401 (333e:0e21) | 237 | 1 |  | proved | proof_useitems.py UseWand
 125: With a loud <SNAP!>, the wand cracks.\n / |
| 0374ee (333e:0f0e) | 1168 | 4 |  | proved | proof_useitems.py UseFood
 119: You can only use those individually.  Take one from thi / 126: You are too full to ea |
| 03797e (333e:139e) | 482 | 0 |  | proved | proof_useitems.py UseRockHammerOn
 132: It seems to have no effect.\n / 135: The rock breaks into smaller pieces.\n / |
| 037b60 (333e:1580) | 324 | 0 |  | proved | proof_useitems.py UseOilOn
 177: You cannot use oil on that.\n / 181: Dousing a cloth with oil and applying it to the |
| 037ca4 (333e:16c4) | 740 | 1 |  | proved | proof_useitems.py UseMagic
 274: The cauldron is empty.\n / |
| 037f88 (333e:19a8) | 397 | 1 |  | proved | proof_useitems.py UseBook |
| 038115 (333e:1b35) | 518 | 1 |  | proved | proof_useitems.py UseRect |
| 03831b (333e:1d3b) | 457 | 6 |  | proved | proof_useitems.py checkLock |
| 0384e4 (333e:1f04) | 242 | 4 |  | proved | proof_useitems.py checkSpell |
| 0385d6 (333e:1ff6) | 215 | 11 |  | proved | proof_useitems.py checkTrap |
| 0386ad (333e:20cd) | 97 | 2 |  | proved | proof_useitems.py moveDoor |
| 03870e (333e:212e) | 184 | 3 |  | proved | proof_useitems.py changeDoor |
| 0387c6 (333e:21e6) | 279 | 6 |  | proved | proof_useitems.py OpenDoor |
| 0388dd (333e:22fd) | 186 | 6 |  | proved | proof_useitems.py CloseDoor |
| 038997 (333e:23b7) | 53 | 2 |  | proved | proof_useitems.py ToggleDoor |
| 0389cc (333e:23ec) | 168 | 2 |  | proved | proof_useitems.py DumpTheBag |
| 038a74 (333e:2494) | 183 | 3 |  | proved | proof_useitems.py UseCont |
| 038b2b (333e:254b) | 49 | 1 |  | proved | proof_useitems.py BlastFunction |
| 038b5c (333e:257c) | 372 | 6 |  | proved | proof_useitems.py decode_obj_spell |
| 038cd0 (333e:26f0) | 51 | 1 |  | proved | proof_useitems.py remove_spell |
| 038d03 (333e:2723) | 207 | 1 |  | proved | proof_useitems.py useNSpellCharges |

## seg 35bd (4 routines, 355 bytes, 20% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 038dd2 (35bd:0002) | 75 | 2 |  | todo |  |
| 038e1d (35bd:004d) | 155 | 9 |  | unproved |  |
| 038eb8 (35bd:00e8) | 54 | 1 |  | todo |  |
| 038eee (35bd:011e) | 71 | 14 |  | proved | verify.py dice |

## seg 35d3 (8 routines, 769 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 038f35 (35d3:0005) | 224 | 1 |  | todo |  |
| 039015 (35d3:00e5) | 51 | 2 |  | todo |  |
| 039048 (35d3:0118) | 120 | 1 |  | todo |  |
| 0390c0 (35d3:0190) | 11 | 1 |  | todo |  |
| 0390cb (35d3:019b) | 38 | 11 |  | todo |  |
| 0390f1 (35d3:01c1) | 98 | 7 |  | todo |  |
| 039153 (35d3:0223) | 83 | 27 |  | todo |  |
| 0391a6 (35d3:0276) | 144 | 7 |  | todo |  |

## seg 3603 (22 routines, 4158 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 039236 (3603:0006) | 41 | 1 |  | todo |  |
| 03925f (3603:002f) | 31 | 3 |  | todo |  |
| 03927e (3603:004e) | 22 | 14 |  | todo |  |
| 039294 (3603:0064) | 22 | 5 |  | todo |  |
| 0392aa (3603:007a) | 22 | 6 |  | todo |  |
| 0392c0 (3603:0090) | 142 | 5 |  | todo |  |
| 03934e (3603:011e) | 69 | 3 |  | todo |  |
| 039393 (3603:0163) | 103 | 2 |  | todo |  |
| 0393fa (3603:01ca) | 141 | 2 |  | todo |  |
| 039487 (3603:0257) | 147 | 1 |  | todo |  |
| 03951a (3603:02ea) | 304 | 56 |  | todo |  |
| 03964a (3603:041a) | 82 | 1 |  | todo |  |
| 03969c (3603:046c) | 83 | 1 |  | todo |  |
| 0396ef (3603:04bf) | 490 | 2 |  | todo |  |
| 0398d9 (3603:06a9) | 248 | 1 |  | todo |  |
| 0399d1 (3603:07a1) | 82 | 1 |  | todo |  |
| 039a23 (3603:07f3) | 248 | 0 |  | todo |  |
| 039b1b (3603:08eb) | 144 | 12 |  | todo |  |
| 039bab (3603:097b) | 118 | 1 |  | todo |  |
| 039c21 (3603:09f1) | 109 | 3 |  | todo |  |
| 039c8e (3603:0a5e) | 1267 | 4 |  | todo |  |
| 03a181 (3603:0f51) | 243 | 2 |  | todo |  |

## seg 3707 (18 routines, 3975 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 03a274 (3707:0004) | 122 | 1 |  | todo |  |
| 03a2ee (3707:007e) | 107 | 1 |  | todo |  |
| 03a359 (3707:00e9) | 649 | 2 |  | todo |  |
| 03a5e2 (3707:0372) | 60 | 0 |  | todo |  |
| 03a61e (3707:03ae) | 112 | 2 |  | todo |  |
| 03a68e (3707:041e) | 286 | 10 |  | todo |  |
| 03a7ac (3707:053c) | 445 | 2 |  | todo |  |
| 03a969 (3707:06f9) | 155 | 5 |  | todo |  |
| 03aa04 (3707:0794) | 454 | 3 |  | todo |  |
| 03abca (3707:095a) | 217 | 1 |  | todo |  |
| 03aca3 (3707:0a33) | 342 | 8 |  | todo |  |
| 03adf9 (3707:0b89) | 295 | 0 |  | todo |  |
| 03af20 (3707:0cb0) | 76 | 2 |  | todo |  |
| 03af6c (3707:0cfc) | 44 | 2 |  | todo |  |
| 03af98 (3707:0d28) | 40 | 1 |  | todo |  |
| 03afc0 (3707:0d50) | 419 | 1 |  | todo |  |
| 03b163 (3707:0ef3) | 97 | 1 |  | todo |  |
| 03b1c4 (3707:0f54) | 55 | 1 |  | todo |  |

## seg 37ff (10 routines, 881 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 03b1fb (37ff:000b) | 184 | 1 |  | todo |  |
| 03b2b3 (37ff:00c3) | 35 | 0 |  | todo |  |
| 03b2d6 (37ff:00e6) | 22 | 5 |  | todo |  |
| 03b2ec (37ff:00fc) | 15 | 1 |  | todo |  |
| 03b2fb (37ff:010b) | 19 | 1 |  | todo |  |
| 03b30e (37ff:011e) | 270 | 3 |  | todo |  |
| 03b41c (37ff:022c) | 31 | 0 |  | todo |  |
| 03b43b (37ff:024b) | 39 | 0 |  | todo |  |
| 03b462 (37ff:0272) | 203 | 3 |  | todo |  |
| 03b52d (37ff:033d) | 63 | 0 |  | todo |  |

## seg 3836 (45 routines, 4659 bytes, 100% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 03b577 (3836:0017) | 293 | 1 | int 3 | notneeded | EMS / XMS memory manager (int 67h, int 2fh) |
| 03b69c (3836:013c) | 81 | 2 | int 4 | notneeded | EMS / XMS memory manager (int 67h, int 2fh) |
| 03b6ed (3836:018d) | 34 | 0 |  | notneeded | EMS / XMS memory manager (int 67h, int 2fh) |
| 03b70f (3836:01af) | 12 | 1 |  | notneeded | EMS / XMS memory manager (int 67h, int 2fh) |
| 03b71b (3836:01bb) | 64 | 1 | int 1 | notneeded | EMS / XMS memory manager (int 67h, int 2fh) |
| 03b75b (3836:01fb) | 93 | 1 |  | notneeded | EMS / XMS memory manager (int 67h, int 2fh) |
| 03b7b8 (3836:0258) | 38 | 3 | int 1 | notneeded | EMS / XMS memory manager (int 67h, int 2fh) |
| 03b7de (3836:027e) | 16 | 1 | int 1 | notneeded | EMS / XMS memory manager (int 67h, int 2fh) |
| 03b7ee (3836:028e) | 128 | 2 |  | notneeded | EMS / XMS memory manager (int 67h, int 2fh) |
| 03b86e (3836:030e) | 205 | 1 |  | notneeded | EMS / XMS memory manager (int 67h, int 2fh) |
| 03b93b (3836:03db) | 57 | 2 | int 2 | notneeded | EMS / XMS memory manager (int 67h, int 2fh) |
| 03b974 (3836:0414) | 69 | 2 |  | notneeded | EMS / XMS memory manager (int 67h, int 2fh) |
| 03b9b9 (3836:0459) | 142 | 1 |  | notneeded | EMS / XMS memory manager (int 67h, int 2fh) |
| 03ba47 (3836:04e7) | 102 | 0 |  | notneeded | EMS / XMS memory manager (int 67h, int 2fh) |
| 03baad (3836:054d) | 74 | 1 |  | notneeded | EMS / XMS memory manager (int 67h, int 2fh) |
| 03baf7 (3836:0597) | 123 | 2 |  | notneeded | EMS / XMS memory manager (int 67h, int 2fh) |
| 03bb72 (3836:0612) | 24 | 1 |  | notneeded | EMS / XMS memory manager (int 67h, int 2fh) |
| 03bb8a (3836:062a) | 59 | 1 |  | notneeded | EMS / XMS memory manager (int 67h, int 2fh) |
| 03bbc5 (3836:0665) | 33 | 1 |  | notneeded | EMS / XMS memory manager (int 67h, int 2fh) |
| 03bbe6 (3836:0686) | 30 | 1 |  | notneeded | EMS / XMS memory manager (int 67h, int 2fh) |
| 03bc04 (3836:06a4) | 51 | 2 |  | notneeded | EMS / XMS memory manager (int 67h, int 2fh) |
| 03bc37 (3836:06d7) | 81 | 2 |  | notneeded | EMS / XMS memory manager (int 67h, int 2fh) |
| 03bc88 (3836:0728) | 30 | 2 |  | notneeded | EMS / XMS memory manager (int 67h, int 2fh) |
| 03bca6 (3836:0746) | 12 | 2 |  | notneeded | EMS / XMS memory manager (int 67h, int 2fh) |
| 03bcb2 (3836:0752) | 38 | 2 |  | notneeded | EMS / XMS memory manager (int 67h, int 2fh) |
| 03bcd8 (3836:0778) | 28 | 2 |  | notneeded | EMS / XMS memory manager (int 67h, int 2fh) |
| 03bcf4 (3836:0794) | 12 | 4 |  | notneeded | EMS / XMS memory manager (int 67h, int 2fh) |
| 03bd00 (3836:07a0) | 78 | 2 |  | notneeded | EMS / XMS memory manager (int 67h, int 2fh) |
| 03bd4e (3836:07ee) | 152 | 0 |  | notneeded | EMS / XMS memory manager (int 67h, int 2fh) |
| 03bde6 (3836:0886) | 88 | 0 |  | notneeded | EMS / XMS memory manager (int 67h, int 2fh) |
| 03be3e (3836:08de) | 224 | 1 |  | notneeded | EMS / XMS memory manager (int 67h, int 2fh) |
| 03bf1e (3836:09be) | 234 | 1 | int 6 | notneeded | EMS / XMS memory manager (int 67h, int 2fh) |
| 03c008 (3836:0aa8) | 282 | 0 |  | notneeded | EMS / XMS memory manager (int 67h, int 2fh) |
| 03c122 (3836:0bc2) | 83 | 1 |  | notneeded | EMS / XMS memory manager (int 67h, int 2fh) |
| 03c175 (3836:0c15) | 12 | 2 |  | notneeded | EMS / XMS memory manager (int 67h, int 2fh) |
| 03c181 (3836:0c21) | 167 | 2 |  | notneeded | EMS / XMS memory manager (int 67h, int 2fh) |
| 03c228 (3836:0cc8) | 47 | 1 |  | notneeded | EMS / XMS memory manager (int 67h, int 2fh) |
| 03c257 (3836:0cf7) | 137 | 1 |  | notneeded | EMS / XMS memory manager (int 67h, int 2fh) |
| 03c2e0 (3836:0d80) | 178 | 1 | int 10 | notneeded | EMS / XMS memory manager (int 67h, int 2fh) |
| 03c392 (3836:0e32) | 101 | 1 | int 3 | notneeded | EMS / XMS memory manager (int 67h, int 2fh) |
| 03c3f7 (3836:0e97) | 42 | 0 | int 1 | notneeded | EMS / XMS memory manager (int 67h, int 2fh) |
| 03c421 (3836:0ec1) | 432 | 1 | int 4 | notneeded | EMS / XMS memory manager (int 67h, int 2fh) |
| 03c5d1 (3836:1071) | 216 | 1 |  | notneeded | EMS / XMS memory manager (int 67h, int 2fh) |
| 03c6a9 (3836:1149) | 84 | 0 |  | notneeded | EMS / XMS memory manager (int 67h, int 2fh) |
| 03c6fd (3836:119d) | 173 | 2 | int 1 | notneeded | EMS / XMS memory manager (int 67h, int 2fh) |

## seg 4723 (28 routines, 48367 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 04ac1a (4723:07ea) | 178 | 1 |  | todo |  |
| 04accc (4723:089c) | 1287 | 1 |  | todo |  |
| 04b1d3 (4723:0da3) | 1502 | 1 |  | todo |  |
| 04b7b1 (4723:1381) | 1909 | 1 |  | todo |  |
| 04bf26 (4723:1af6) | 114 | 1 |  | todo |  |
| 04bf98 (4723:1b68) | 597 | 1 |  | todo |  |
| 04c1ed (4723:1dbd) | 2506 | 1 | io 1 | todo |  |
| 04cbb7 (4723:2787) | 2757 | 1 |  | todo |  |
| 04d67c (4723:324c) | 1333 | 1 | io 1 | todo |  |
| 04dbb1 (4723:3781) | 159 | 1 |  | todo |  |
| 04dc50 (4723:3820) | 1540 | 1 |  | todo |  |
| 04e254 (4723:3e24) | 776 | 1 |  | todo |  |
| 04e55c (4723:412c) | 1396 | 1 | io 1 | todo |  |
| 04ead0 (4723:46a0) | 463 | 1 |  | todo |  |
| 04ec9f (4723:486f) | 1693 | 1 |  | todo |  |
| 04f33c (4723:4f0c) | 424 | 1 | io 1 | todo |  |
| 04f4e4 (4723:50b4) | 3202 | 1 |  | todo |  |
| 050166 (4723:5d36) | 1784 | 1 |  | todo |  |
| 05085e (4723:642e) | 3629 | 1 |  | todo |  |
| 05168b (4723:725b) | 1374 | 1 |  | todo |  |
| 051be9 (4723:77b9) | 1703 | 1 |  | todo |  |
| 052290 (4723:7e60) | 6288 | 1 |  | todo |  |
| 053b20 (4723:96f0) | 907 | 1 |  | todo |  |
| 053eab (4723:9a7b) | 2040 | 1 |  | todo |  |
| 0546a3 (4723:a273) | 4335 | 1 |  | todo |  |
| 055792 (4723:b362) | 612 | 1 |  | todo |  |
| 0559f6 (4723:b5c6) | 1091 | 1 |  | todo |  |
| 055e39 (4723:ba09) | 2768 | 1 |  | todo |  |

## ovl 5950 (2 routines, 44 bytes, 100% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 065710 (ovl 5950+0000) | 23 | 0 |  | proved | proof_objclass.py animobj_load |
| 065727 (ovl 5950+0017) | 21 | 0 |  | proved | proof_objclass.py animobj_class_data |

## ovl 5953 (7 routines, 44 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 065750 (ovl 5953+0000) | 5 | 0 |  | todo |  |
| 065755 (ovl 5953+0005) | 14 | 0 |  | todo |  |
| 065763 (ovl 5953+0013) | 5 | 0 |  | todo |  |
| 065768 (ovl 5953+0018) | 5 | 0 |  | todo |  |
| 06576d (ovl 5953+001d) | 5 | 0 |  | todo |  |
| 065772 (ovl 5953+0022) | 5 | 0 |  | todo |  |
| 065777 (ovl 5953+0027) | 5 | 0 |  | todo |  |

## ovl 5958 (6 routines, 2122 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 065790 (ovl 5958+0000) | 339 | 10 |  | todo |  |
| 0658e3 (ovl 5958+0153) | 217 | 10 |  | todo |  |
| 0659bc (ovl 5958+022c) | 1006 | 5 |  | todo |  |
| 065daa (ovl 5958+061a) | 276 | 6 |  | todo |  |
| 065ebe (ovl 5958+072e) | 106 | 0 |  | todo |  |
| 065f28 (ovl 5958+0798) | 178 | 1 |  | todo |  |

## ovl 595c (19 routines, 4422 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 066050 (ovl 595c+0000) | 151 | 0 |  | todo |  |
| 0660e7 (ovl 595c+0097) | 135 | 2 |  | todo |  |
| 06616e (ovl 595c+011e) | 48 | 3 |  | todo |  |
| 06619e (ovl 595c+014e) | 130 | 1 |  | todo |  |
| 066220 (ovl 595c+01d0) | 21 | 2 |  | todo |  |
| 066235 (ovl 595c+01e5) | 322 | 1 |  | todo |  |
| 066377 (ovl 595c+0327) | 248 | 1 |  | todo |  |
| 06646f (ovl 595c+041f) | 108 | 5 |  | todo |  |
| 0664db (ovl 595c+048b) | 484 | 1 | io 1 | todo |  |
| 0666bf (ovl 595c+066f) | 248 | 1 |  | todo |  |
| 0667b7 (ovl 595c+0767) | 71 | 0 |  | todo |  |
| 0667fe (ovl 595c+07ae) | 248 | 1 |  | todo |  |
| 0668f6 (ovl 595c+08a6) | 1242 | 0 |  | todo |  |
| 066dd0 (ovl 595c+0d80) | 188 | 2 |  | todo |  |
| 066e8c (ovl 595c+0e3c) | 206 | 2 |  | todo |  |
| 066f5a (ovl 595c+0f0a) | 92 | 1 |  | todo |  |
| 066fb6 (ovl 595c+0f66) | 386 | 2 |  | todo |  |
| 067138 (ovl 595c+10e8) | 84 | 1 |  | todo |  |
| 06718c (ovl 595c+113c) | 10 | 0 |  | todo |  |

## ovl 5964 (61 routines, 9514 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 0672c0 (ovl 5964+0000) | 49 | 1 |  | todo |  |
| 0672f1 (ovl 5964+0031) | 418 | 11 |  | todo |  |
| 067493 (ovl 5964+01d3) | 410 | 12 |  | todo |  |
| 06762d (ovl 5964+036d) | 477 | 1 |  | todo |  |
| 06780a (ovl 5964+054a) | 272 | 2 |  | todo |  |
| 06791a (ovl 5964+065a) | 187 | 1 |  | todo |  |
| 0679d5 (ovl 5964+0715) | 178 | 1 |  | todo |  |
| 067a87 (ovl 5964+07c7) | 489 | 1 |  | todo |  |
| 067c70 (ovl 5964+09b0) | 72 | 0 |  | todo |  |
| 067cb8 (ovl 5964+09f8) | 85 | 0 |  | todo |  |
| 067d0d (ovl 5964+0a4d) | 48 | 0 |  | todo |  |
| 067d3d (ovl 5964+0a7d) | 94 | 0 |  | todo |  |
| 067d9b (ovl 5964+0adb) | 268 | 0 |  | todo |  |
| 067ea7 (ovl 5964+0be7) | 346 | 0 |  | todo |  |
| 068001 (ovl 5964+0d41) | 266 | 0 |  | todo |  |
| 06810b (ovl 5964+0e4b) | 22 | 0 |  | todo |  |
| 068121 (ovl 5964+0e61) | 45 | 0 |  | todo |  |
| 06814e (ovl 5964+0e8e) | 70 | 0 |  | todo |  |
| 068194 (ovl 5964+0ed4) | 303 | 0 |  | todo |  |
| 0682c3 (ovl 5964+1003) | 189 | 0 |  | todo |  |
| 068380 (ovl 5964+10c0) | 111 | 0 |  | todo |  |
| 0683ef (ovl 5964+112f) | 87 | 0 |  | todo |  |
| 068446 (ovl 5964+1186) | 37 | 0 |  | todo |  |
| 06846b (ovl 5964+11ab) | 37 | 0 |  | todo |  |
| 068490 (ovl 5964+11d0) | 600 | 12 |  | todo |  |
| 0686e8 (ovl 5964+1428) | 259 | 2 |  | todo |  |
| 0687eb (ovl 5964+152b) | 511 | 1 |  | todo |  |
| 0689ea (ovl 5964+172a) | 35 | 1 |  | todo |  |
| 068a0d (ovl 5964+174d) | 33 | 1 |  | todo |  |
| 068a2e (ovl 5964+176e) | 986 | 1 |  | todo |  |
| 068e08 (ovl 5964+1b48) | 57 | 1 |  | todo |  |
| 068e41 (ovl 5964+1b81) | 36 | 1 |  | todo |  |
| 068e65 (ovl 5964+1ba5) | 57 | 1 |  | todo |  |
| 068e9e (ovl 5964+1bde) | 57 | 1 |  | todo |  |
| 068ed7 (ovl 5964+1c17) | 80 | 1 |  | todo |  |
| 068f27 (ovl 5964+1c67) | 80 | 1 |  | todo |  |
| 068f77 (ovl 5964+1cb7) | 69 | 1 |  | todo |  |
| 068fbc (ovl 5964+1cfc) | 69 | 1 |  | todo |  |
| 069001 (ovl 5964+1d41) | 66 | 1 |  | todo |  |
| 069043 (ovl 5964+1d83) | 66 | 1 |  | todo |  |
| 069085 (ovl 5964+1dc5) | 66 | 1 |  | todo |  |
| 0690c7 (ovl 5964+1e07) | 66 | 1 |  | todo |  |
| 069109 (ovl 5964+1e49) | 66 | 1 |  | todo |  |
| 06914b (ovl 5964+1e8b) | 66 | 1 |  | todo |  |
| 06918d (ovl 5964+1ecd) | 46 | 1 |  | todo |  |
| 0691bb (ovl 5964+1efb) | 40 | 1 |  | todo |  |
| 0691e3 (ovl 5964+1f23) | 45 | 1 |  | todo |  |
| 069210 (ovl 5964+1f50) | 58 | 1 |  | todo |  |
| 06924a (ovl 5964+1f8a) | 51 | 1 |  | todo |  |
| 06927d (ovl 5964+1fbd) | 125 | 1 |  | todo |  |
| 0692fa (ovl 5964+203a) | 225 | 1 |  | todo |  |
| 0693db (ovl 5964+211b) | 168 | 1 |  | todo |  |
| 069483 (ovl 5964+21c3) | 168 | 1 |  | todo |  |
| 06952b (ovl 5964+226b) | 21 | 3 |  | todo |  |
| 069540 (ovl 5964+2280) | 19 | 46 |  | todo |  |
| 069553 (ovl 5964+2293) | 22 | 3 |  | todo |  |
| 069569 (ovl 5964+22a9) | 22 | 2 |  | todo |  |
| 06957f (ovl 5964+22bf) | 110 | 2 |  | todo |  |
| 0695ed (ovl 5964+232d) | 167 | 2 |  | todo |  |
| 069694 (ovl 5964+23d4) | 127 | 2 |  | todo |  |
| 069713 (ovl 5964+2453) | 215 | 1 | io 1 | todo |  |

## ovl 5977 (15 routines, 2950 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 069980 (ovl 5977+0000) | 33 | 0 |  | todo |  |
| 0699a1 (ovl 5977+0021) | 59 | 0 |  | todo |  |
| 0699dc (ovl 5977+005c) | 375 | 0 |  | todo |  |
| 069b53 (ovl 5977+01d3) | 99 | 0 |  | todo |  |
| 069bb6 (ovl 5977+0236) | 73 | 0 |  | todo |  |
| 069bff (ovl 5977+027f) | 471 | 0 |  | todo |  |
| 069dd6 (ovl 5977+0456) | 104 | 0 |  | todo |  |
| 069e3e (ovl 5977+04be) | 52 | 0 |  | todo |  |
| 069e72 (ovl 5977+04f2) | 75 | 0 |  | todo |  |
| 069ebd (ovl 5977+053d) | 123 | 0 |  | todo |  |
| 069f38 (ovl 5977+05b8) | 108 | 0 |  | todo |  |
| 069fa4 (ovl 5977+0624) | 37 | 0 |  | todo |  |
| 069fc9 (ovl 5977+0649) | 257 | 0 |  | todo |  |
| 06a0ca (ovl 5977+074a) | 697 | 0 |  | todo |  |
| 06a383 (ovl 5977+0a03) | 387 | 0 |  | todo |  |

## ovl 597e (35 routines, 8375 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 06a590 (ovl 597e+0000) | 421 | 0 |  | todo |  |
| 06a735 (ovl 597e+01a5) | 541 | 1 |  | unproved |  |
| 06a952 (ovl 597e+03c2) | 202 | 1 |  | todo |  |
| 06aa1c (ovl 597e+048c) | 71 | 0 |  | todo |  |
| 06aa63 (ovl 597e+04d3) | 92 | 3 |  | todo |  |
| 06aabf (ovl 597e+052f) | 92 | 2 |  | todo |  |
| 06ab1b (ovl 597e+058b) | 110 | 1 |  | todo |  |
| 06ab89 (ovl 597e+05f9) | 68 | 1 |  | todo |  |
| 06abcd (ovl 597e+063d) | 70 | 0 |  | todo |  |
| 06ac13 (ovl 597e+0683) | 899 | 3 |  | todo | 252: That is too heavy to take.\n / |
| 06af96 (ovl 597e+0a06) | 378 | 5 |  | todo |  |
| 06b110 (ovl 597e+0b80) | 162 | 2 |  | todo |  |
| 06b1b2 (ovl 597e+0c22) | 85 | 1 |  | todo |  |
| 06b207 (ovl 597e+0c77) | 339 | 1 |  | todo |  |
| 06b35a (ovl 597e+0dca) | 169 | 7 |  | todo |  |
| 06b403 (ovl 597e+0e73) | 148 | 3 |  | todo |  |
| 06b497 (ovl 597e+0f07) | 469 | 1 |  | todo |  |
| 06b66c (ovl 597e+10dc) | 51 | 1 |  | todo |  |
| 06b69f (ovl 597e+110f) | 427 | 0 |  | todo |  |
| 06b84a (ovl 597e+12ba) | 535 | 0 |  | todo |  |
| 06ba61 (ovl 597e+14d1) | 13 | 0 |  | todo |  |
| 06ba6e (ovl 597e+14de) | 361 | 0 |  | todo |  |
| 06bbd7 (ovl 597e+1647) | 106 | 3 |  | todo |  |
| 06bc41 (ovl 597e+16b1) | 264 | 2 |  | todo |  |
| 06bd49 (ovl 597e+17b9) | 62 | 2 |  | todo |  |
| 06bd87 (ovl 597e+17f7) | 99 | 5 |  | todo |  |
| 06bdea (ovl 597e+185a) | 315 | 2 |  | todo |  |
| 06bf25 (ovl 597e+1995) | 104 | 2 |  | todo |  |
| 06bf8d (ovl 597e+19fd) | 180 | 1 |  | todo |  |
| 06c041 (ovl 597e+1ab1) | 401 | 1 |  | todo |  |
| 06c1d2 (ovl 597e+1c42) | 341 | 1 |  | todo |  |
| 06c327 (ovl 597e+1d97) | 357 | 1 |  | todo |  |
| 06c48c (ovl 597e+1efc) | 127 | 1 |  | todo |  |
| 06c50b (ovl 597e+1f7b) | 50 | 0 |  | todo |  |
| 06c53d (ovl 597e+1fad) | 266 | 2 |  | todo |  |

## ovl 598a (7 routines, 38 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 06c7f0 (ovl 598a+0000) | 5 | 0 |  | todo |  |
| 06c7f5 (ovl 598a+0005) | 5 | 0 |  | todo |  |
| 06c7fa (ovl 598a+000a) | 8 | 0 |  | todo |  |
| 06c802 (ovl 598a+0012) | 5 | 0 |  | todo |  |
| 06c807 (ovl 598a+0017) | 5 | 0 |  | todo |  |
| 06c80c (ovl 598a+001c) | 5 | 0 |  | todo |  |
| 06c811 (ovl 598a+0021) | 5 | 0 |  | todo |  |

## ovl 5991 (15 routines, 6069 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 06c830 (ovl 5991+0000) | 600 | 3 |  | unproved |  |
| 06ca88 (ovl 5991+0258) | 239 | 1 |  | unproved |  |
| 06cb77 (ovl 5991+0347) | 336 | 1 |  | todo |  |
| 06ccc7 (ovl 5991+0497) | 190 | 1 |  | todo |  |
| 06cd85 (ovl 5991+0555) | 45 | 1 |  | unproved |  |
| 06cdb2 (ovl 5991+0582) | 217 | 1 |  | unproved |  |
| 06ce8b (ovl 5991+065b) | 751 | 2 |  | todo |  |
| 06d17a (ovl 5991+094a) | 357 | 4 |  | todo |  |
| 06d2df (ovl 5991+0aaf) | 405 | 1 |  | todo |  |
| 06d474 (ovl 5991+0c44) | 803 | 1 | io 2 | unproved |  |
| 06d797 (ovl 5991+0f67) | 1350 | 1 |  | todo |  |
| 06dcdd (ovl 5991+14ad) | 34 | 0 |  | todo |  |
| 06dcff (ovl 5991+14cf) | 63 | 0 |  | todo |  |
| 06dd3e (ovl 5991+150e) | 649 | 1 |  | unproved |  |
| 06dfc7 (ovl 5991+1797) | 30 | 1 |  | todo |  |

## ovl 5998 (5 routines, 806 bytes, 100% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 06e140 (ovl 5998+0000) | 22 | 1 |  | proved | proof_treasure.py init_combinables |
| 06e156 (ovl 5998+0016) | 324 | 1 |  | proved | proof_treasure.py ObjsBeCombinable |
| 06e29a (ovl 5998+015a) | 31 | 1 |  | proved | proof_treasure.py CombineObjs |
| 06e2b9 (ovl 5998+0179) | 57 | 1 |  | proved | proof_treasure.py RemoveAfterCombine |
| 06e2f2 (ovl 5998+01b2) | 372 | 1 |  | proved | proof_treasure.py make_stew
 148: The bowl does not contain the correct ingredients.\n / 149: You mix the ingredients i |

## ovl 599c (31 routines, 6533 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 06e490 (ovl 599c+0000) | 407 | 6 |  | todo | 272: There is no reaction from the princess.\n / |
| 06e627 (ovl 599c+0197) | 34 | 0 |  | todo |  |
| 06e649 (ovl 599c+01b9) | 40 | 0 |  | todo |  |
| 06e671 (ovl 599c+01e1) | 1020 | 0 |  | unproved |  |
| 06ea6d (ovl 599c+05dd) | 38 | 1 |  | unproved |  |
| 06ea93 (ovl 599c+0603) | 5 | 0 |  | todo |  |
| 06ea98 (ovl 599c+0608) | 885 | 1 |  | todo |  |
| 06ee0d (ovl 599c+097d) | 208 | 2 |  | unproved |  |
| 06eedd (ovl 599c+0a4d) | 479 | 0 |  | todo |  |
| 06f0bc (ovl 599c+0c2c) | 548 | 0 |  | todo |  |
| 06f2e0 (ovl 599c+0e50) | 252 | 0 |  | todo |  |
| 06f3dc (ovl 599c+0f4c) | 103 | 2 |  | todo |  |
| 06f443 (ovl 599c+0fb3) | 83 | 0 |  | todo |  |
| 06f496 (ovl 599c+1006) | 103 | 2 |  | todo |  |
| 06f4fd (ovl 599c+106d) | 183 | 0 |  | todo |  |
| 06f5b4 (ovl 599c+1124) | 51 | 0 |  | todo |  |
| 06f5e7 (ovl 599c+1157) | 142 | 0 |  | todo |  |
| 06f675 (ovl 599c+11e5) | 135 | 0 |  | todo |  |
| 06f6fc (ovl 599c+126c) | 174 | 0 |  | todo |  |
| 06f7aa (ovl 599c+131a) | 331 | 0 |  | todo |  |
| 06f8f5 (ovl 599c+1465) | 232 | 0 |  | todo |  |
| 06f9dd (ovl 599c+154d) | 210 | 0 |  | todo |  |
| 06faaf (ovl 599c+161f) | 27 | 0 |  | todo |  |
| 06faca (ovl 599c+163a) | 43 | 0 |  | todo |  |
| 06faf5 (ovl 599c+1665) | 27 | 0 |  | todo |  |
| 06fb10 (ovl 599c+1680) | 27 | 0 |  | todo |  |
| 06fb2b (ovl 599c+169b) | 27 | 0 |  | todo |  |
| 06fb46 (ovl 599c+16b6) | 498 | 0 |  | todo |  |
| 06fd38 (ovl 599c+18a8) | 92 | 0 |  | todo |  |
| 06fd94 (ovl 599c+1904) | 54 | 0 |  | todo |  |
| 06fdca (ovl 599c+193a) | 75 | 0 |  | todo |  |

## ovl 59a8 (4 routines, 825 bytes, 94% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 0700b0 (ovl 59a8+0000) | 23 | 0 |  | todo |  |
| 0700c7 (ovl 59a8+0017) | 23 | 0 |  | todo |  |
| 0700de (ovl 59a8+002e) | 48 | 0 |  | proved | proof_creature.py creature_class_data |
| 07010e (ovl 59a8+005e) | 731 | 1 |  | proved | proof_creature.py creature_obj_init |

## ovl 59ae (2 routines, 1667 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 070400 (ovl 59ae+0000) | 1170 | 1 |  | todo |  |
| 070892 (ovl 59ae+0492) | 497 | 1 |  | todo |  |

## ovl 59b1 (15 routines, 3809 bytes, 100% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 070af0 (ovl 59b1+0000) | 62 | 4 |  | proved | proof_crittime.py change_critter_goal |
| 070b2e (ovl 59b1+003e) | 125 | 1 |  | proved | proof_crittime.py yearly_checkup |
| 070bab (ovl 59b1+00bb) | 90 | 2 |  | proved | proof_crittime.py set_gridx_and_y_based_on_tile_type |
| 070c05 (ovl 59b1+0115) | 760 | 1 |  | proved | proof_crittime.py up_crit |
| 070efd (ovl 59b1+040d) | 297 | 1 |  | proved | proof_crittime.py up_mob |
| 071026 (ovl 59b1+0536) | 328 | 2 |  | proved | proof_crittime.py update_all_critters_whilst_player_snoozes |
| 07116e (ovl 59b1+067e) | 115 | 0 |  | proved | proof_crittime.py check_for_hostile_creature |
| 0711e1 (ovl 59b1+06f1) | 43 | 1 |  | proved | proof_crittime.py hostile_creatures_near |
| 07120c (ovl 59b1+071c) | 1124 | 0 |  | proved | proof_crittime.py wander_that_monster |
| 071670 (ovl 59b1+0b80) | 43 | 1 |  | proved | proof_crittime.py wandering_monster_check |
| 07169b (ovl 59b1+0bab) | 52 | 1 |  | proved | proof_crittime.py set_creatures_from_saved_game |
| 0716cf (ovl 59b1+0bdf) | 68 | 1 |  | proved | proof_crittime.py set_creatures_to_saved_game |
| 071713 (ovl 59b1+0c23) | 20 | 2 |  | proved | proof_crittime.py init_level_creature_stuff |
| 071727 (ovl 59b1+0c37) | 533 | 0 |  | proved | proof_crittime.py critter_get_told |
| 07193c (ovl 59b1+0e4c) | 149 | 2 |  | proved | proof_crittime.py player_grabbed |

## ovl 59b8 (33 routines, 5714 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 071a40 (ovl 59b8+0000) | 16 | 2 |  | todo |  |
| 071a50 (ovl 59b8+0010) | 68 | 5 |  | todo |  |
| 071a94 (ovl 59b8+0054) | 10 | 2 |  | todo |  |
| 071a9e (ovl 59b8+005e) | 60 | 0 |  | todo |  |
| 071ada (ovl 59b8+009a) | 163 | 1 |  | todo |  |
| 071b7d (ovl 59b8+013d) | 103 | 1 |  | todo |  |
| 071be4 (ovl 59b8+01a4) | 72 | 1 |  | todo |  |
| 071c2c (ovl 59b8+01ec) | 137 | 1 |  | todo |  |
| 071cb5 (ovl 59b8+0275) | 108 | 1 |  | todo |  |
| 071d21 (ovl 59b8+02e1) | 202 | 1 |  | todo |  |
| 071deb (ovl 59b8+03ab) | 11 | 1 |  | todo |  |
| 071df6 (ovl 59b8+03b6) | 512 | 1 |  | todo |  |
| 071ff6 (ovl 59b8+05b6) | 15 | 0 |  | todo |  |
| 072005 (ovl 59b8+05c5) | 8 | 0 |  | todo |  |
| 07200d (ovl 59b8+05cd) | 45 | 0 |  | todo |  |
| 07203a (ovl 59b8+05fa) | 71 | 0 |  | todo |  |
| 072081 (ovl 59b8+0641) | 65 | 0 |  | todo |  |
| 0720c2 (ovl 59b8+0682) | 15 | 0 |  | todo |  |
| 0720d1 (ovl 59b8+0691) | 20 | 0 |  | todo |  |
| 0720e5 (ovl 59b8+06a5) | 39 | 0 |  | todo |  |
| 07210c (ovl 59b8+06cc) | 77 | 0 |  | todo |  |
| 072159 (ovl 59b8+0719) | 99 | 0 |  | todo |  |
| 0721bc (ovl 59b8+077c) | 34 | 0 |  | todo |  |
| 0721de (ovl 59b8+079e) | 34 | 0 |  | todo |  |
| 072200 (ovl 59b8+07c0) | 61 | 0 |  | todo |  |
| 07223d (ovl 59b8+07fd) | 30 | 0 |  | todo |  |
| 07225b (ovl 59b8+081b) | 21 | 0 |  | todo |  |
| 072270 (ovl 59b8+0830) | 57 | 1 |  | todo |  |
| 0722a9 (ovl 59b8+0869) | 2969 | 1 |  | todo |  |
| 072e42 (ovl 59b8+1402) | 225 | 16 |  | todo |  |
| 072f23 (ovl 59b8+14e3) | 19 | 1 |  | todo |  |
| 072f36 (ovl 59b8+14f6) | 323 | 2 |  | todo |  |
| 073079 (ovl 59b8+1639) | 25 | 1 |  | todo |  |

## ovl 59c5 (3 routines, 37 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 073140 (ovl 59c5+0000) | 27 | 1 |  | todo |  |
| 07315b (ovl 59c5+001b) | 5 | 14 |  | todo |  |
| 073160 (ovl 59c5+0020) | 5 | 0 |  | todo |  |

## ovl 59c8 (21 routines, 6697 bytes, 99% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 073170 (ovl 59c8+0000) | 2377 | 2 |  | proved | proof_worldev.py find_good_x_and_y |
| 073ab9 (ovl 59c8+0949) | 175 | 4 |  | proved | proof_worldev.py do_teleport |
| 073b68 (ovl 59c8+09f8) | 698 | 3 |  | proved | proof_worldev.py change_terrain |
| 073e22 (ovl 59c8+0cb2) | 194 | 1 |  | proved | proof_worldev.py whack_thing |
| 073ee4 (ovl 59c8+0d74) | 80 | 3 |  | proved | proof_worldev.py inanimate_spell |
| 073f34 (ovl 59c8+0dc4) | 221 | 1 |  | proved | proof_worldev.py go_fish
 99: You catch a lovely fish.\n / 101: You cannot fish there.  Perhaps somewhere else.\n / |
| 074011 (ovl 59c8+0ea1) | 362 | 2 |  | proved | proof_worldev.py work_bullfrog_tiles
 192: There is an empty clicking sound.\n / 193: A voice utters the words "Reset Activated. |
| 07417b (ovl 59c8+100b) | 430 | 1 |  | proved | proof_worldev.py emerald_trap |
| 074329 (ovl 59c8+11b9) | 25 | 1 |  | proved | proof_worldev.py player_did_bad |
| 074342 (ovl 59c8+11d2) | 135 | 1 |  | proved | proof_worldev.py eight_pos_switch |
| 0743c9 (ovl 59c8+1259) | 192 | 2 |  | proved | proof_worldev.py ExplodingBook_ovr107_1259 |
| 074489 (ovl 59c8+1319) | 90 | 1 |  | todo |  |
| 0744e3 (ovl 59c8+1373) | 23 | 1 |  | proved | proof_worldev.py do_arial_talking |
| 0744fa (ovl 59c8+138a) | 71 | 0 |  | proved | proof_worldev.py remove_whoami |
| 074541 (ovl 59c8+13d1) | 206 | 1 |  | proved | proof_worldev.py TyballDeath_ovr107_13D1 |
| 07460f (ovl 59c8+149f) | 304 | 2 |  | proved | proof_worldev.py death_check |
| 07473f (ovl 59c8+15cf) | 109 | 2 |  | proved | proof_worldev.py get_rep_diff |
| 0747ac (ovl 59c8+163c) | 280 | 1 |  | proved | proof_worldev.py do_repair |
| 0748c4 (ovl 59c8+1754) | 498 | 1 |  | proved | proof_worldev.py repair_item
 83: .\n / 142: You cannot repair that.\n / 216: You think it will be / 217:  to repair the |
| 074ab6 (ovl 59c8+1946) | 118 | 0 |  | proved | proof_worldev.py clear_loretry |
| 074b2c (ovl 59c8+19bc) | 109 | 1 |  | proved | proof_worldev.py clear_all_loretries |

## ovl 59d3 (18 routines, 1907 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 074c70 (ovl 59d3+0000) | 74 | 1 |  | todo |  |
| 074cba (ovl 59d3+004a) | 362 | 1 |  | unproved |  |
| 074e24 (ovl 59d3+01b4) | 64 | 5 |  | todo |  |
| 074e64 (ovl 59d3+01f4) | 24 | 1 |  | todo |  |
| 074e7c (ovl 59d3+020c) | 148 | 1 |  | todo |  |
| 074f10 (ovl 59d3+02a0) | 11 | 1 |  | todo |  |
| 074f1b (ovl 59d3+02ab) | 25 | 2 |  | todo |  |
| 074f34 (ovl 59d3+02c4) | 59 | 2 |  | todo |  |
| 074f6f (ovl 59d3+02ff) | 111 | 7 |  | todo |  |
| 074fde (ovl 59d3+036e) | 13 | 0 |  | todo |  |
| 074feb (ovl 59d3+037b) | 173 | 0 |  | todo |  |
| 075098 (ovl 59d3+0428) | 20 | 0 |  | todo |  |
| 0750ac (ovl 59d3+043c) | 191 | 2 |  | todo |  |
| 07516b (ovl 59d3+04fb) | 217 | 2 |  | todo |  |
| 075244 (ovl 59d3+05d4) | 10 | 0 |  | todo |  |
| 07524e (ovl 59d3+05de) | 79 | 1 |  | todo |  |
| 07529d (ovl 59d3+062d) | 275 | 2 |  | todo |  |
| 0753b0 (ovl 59d3+0740) | 51 | 1 |  | todo |  |

## ovl 59db (4 routines, 404 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 0754f0 (ovl 59db+0000) | 171 | 1 |  | todo |  |
| 07559b (ovl 59db+00ab) | 28 | 8 |  | todo |  |
| 0755b7 (ovl 59db+00c7) | 140 | 13 |  | todo |  |
| 075643 (ovl 59db+0153) | 65 | 1 |  | todo |  |

## ovl 59e1 (6 routines, 1191 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 0756c0 (ovl 59e1+0000) | 329 | 0 |  | todo |  |
| 075809 (ovl 59e1+0149) | 75 | 1 |  | todo |  |
| 075854 (ovl 59e1+0194) | 47 | 2 |  | todo |  |
| 075883 (ovl 59e1+01c3) | 224 | 1 |  | todo |  |
| 075963 (ovl 59e1+02a3) | 381 | 1 |  | todo |  |
| 075ae0 (ovl 59e1+0420) | 135 | 1 |  | todo |  |

## ovl 59e5 (6 routines, 1051 bytes, 100% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 075b90 (ovl 59e5+0000) | 462 | 2 |  | proved | proof_crpages.py preload_cr |
| 075d5e (ovl 59e5+01ce) | 92 | 1 |  | proved | proof_crpages.py NightCleanCritPages |
| 075dba (ovl 59e5+022a) | 120 | 1 |  | proved | proof_crpages.py punt_idle_crpage |
| 075e32 (ovl 59e5+02a2) | 185 | 2 |  | proved | proof_crpages.py ovr113_2A2 |
| 075eeb (ovl 59e5+035b) | 114 | 1 |  | proved | proof_crpages.py swap_ws_out |
| 075f5d (ovl 59e5+03cd) | 78 | 1 |  | proved | proof_crpages.py unswap_ws |

## ovl 59e9 (17 routines, 1362 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 075ff0 (ovl 59e9+0000) | 53 | 1 |  | todo |  |
| 076025 (ovl 59e9+0035) | 136 | 19 |  | todo |  |
| 0760ad (ovl 59e9+00bd) | 15 | 1 |  | todo |  |
| 0760bc (ovl 59e9+00cc) | 47 | 6 |  | todo |  |
| 0760eb (ovl 59e9+00fb) | 95 | 4 |  | todo |  |
| 07614a (ovl 59e9+015a) | 46 | 6 |  | todo |  |
| 076178 (ovl 59e9+0188) | 43 | 2 |  | todo |  |
| 0761a3 (ovl 59e9+01b3) | 262 | 5 |  | todo |  |
| 0762a9 (ovl 59e9+02b9) | 232 | 4 |  | todo |  |
| 076391 (ovl 59e9+03a1) | 67 | 2 |  | todo |  |
| 0763d4 (ovl 59e9+03e4) | 150 | 2 |  | todo |  |
| 07646a (ovl 59e9+047a) | 23 | 5 |  | todo |  |
| 076481 (ovl 59e9+0491) | 23 | 3 |  | todo |  |
| 076498 (ovl 59e9+04a8) | 23 | 0 |  | todo |  |
| 0764af (ovl 59e9+04bf) | 23 | 0 |  | todo |  |
| 0764c6 (ovl 59e9+04d6) | 40 | 4 |  | todo |  |
| 0764ee (ovl 59e9+04fe) | 84 | 1 |  | todo |  |

## ovl 59f1 (24 routines, 2603 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 0765c0 (ovl 59f1+0000) | 137 | 1 |  | todo |  |
| 076649 (ovl 59f1+0089) | 290 | 2 |  | todo |  |
| 07676b (ovl 59f1+01ab) | 27 | 2 |  | todo |  |
| 076786 (ovl 59f1+01c6) | 185 | 1 |  | todo |  |
| 07683f (ovl 59f1+027f) | 84 | 0 |  | todo |  |
| 076893 (ovl 59f1+02d3) | 65 | 0 |  | todo |  |
| 0768d4 (ovl 59f1+0314) | 64 | 0 |  | todo |  |
| 076914 (ovl 59f1+0354) | 111 | 0 |  | todo |  |
| 076983 (ovl 59f1+03c3) | 117 | 0 |  | todo |  |
| 0769f8 (ovl 59f1+0438) | 11 | 0 |  | todo |  |
| 076a03 (ovl 59f1+0443) | 12 | 0 |  | todo |  |
| 076a0f (ovl 59f1+044f) | 125 | 0 |  | todo |  |
| 076a8c (ovl 59f1+04cc) | 89 | 0 |  | todo |  |
| 076ae5 (ovl 59f1+0525) | 209 | 12 |  | todo |  |
| 076bb6 (ovl 59f1+05f6) | 46 | 1 |  | todo |  |
| 076be4 (ovl 59f1+0624) | 36 | 1 |  | todo |  |
| 076c08 (ovl 59f1+0648) | 38 | 1 |  | todo |  |
| 076c2e (ovl 59f1+066e) | 259 | 1 |  | todo |  |
| 076d31 (ovl 59f1+0771) | 46 | 1 |  | todo |  |
| 076d5f (ovl 59f1+079f) | 61 | 0 |  | todo |  |
| 076d9c (ovl 59f1+07dc) | 55 | 7 |  | todo |  |
| 076dd3 (ovl 59f1+0813) | 43 | 4 |  | todo |  |
| 076dfe (ovl 59f1+083e) | 405 | 1 |  | todo |  |
| 076f93 (ovl 59f1+09d3) | 88 | 1 |  | todo |  |

## ovl 59fb (2 routines, 149 bytes, 100% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 077070 (ovl 59fb+0000) | 58 | 0 |  | proved | proof_objclass.py hack_init |
| 0770aa (ovl 59fb+003a) | 91 | 0 |  | proved | proof_objclass.py hack_class_data |

## ovl 59fe (13 routines, 4239 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 077110 (ovl 59fe+0000) | 402 | 3 |  | todo |  |
| 0772a2 (ovl 59fe+0192) | 77 | 2 |  | todo |  |
| 0772ef (ovl 59fe+01df) | 140 | 2 |  | todo |  |
| 07737b (ovl 59fe+026b) | 182 | 4 |  | todo |  |
| 077431 (ovl 59fe+0321) | 201 | 2 |  | todo |  |
| 0774fa (ovl 59fe+03ea) | 180 | 10 |  | todo |  |
| 0775ae (ovl 59fe+049e) | 478 | 10 |  | todo |  |
| 07778c (ovl 59fe+067c) | 944 | 1 |  | todo |  |
| 077b3c (ovl 59fe+0a2c) | 36 | 1 |  | todo |  |
| 077b60 (ovl 59fe+0a50) | 221 | 1 |  | todo |  |
| 077c3d (ovl 59fe+0b2d) | 796 | 1 |  | todo | 247: You can only put runes in the rune bag.\n / |
| 077f59 (ovl 59fe+0e49) | 392 | 1 |  | todo |  |
| 0780e1 (ovl 59fe+0fd1) | 190 | 4 |  | todo |  |

## ovl 5a05 (12 routines, 1910 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 078270 (ovl 5a05+0000) | 300 | 1 |  | todo |  |
| 07839c (ovl 5a05+012c) | 218 | 3 |  | todo |  |
| 078476 (ovl 5a05+0206) | 224 | 2 |  | todo |  |
| 078556 (ovl 5a05+02e6) | 84 | 1 |  | todo |  |
| 0785aa (ovl 5a05+033a) | 26 | 1 |  | todo |  |
| 0785c4 (ovl 5a05+0354) | 34 | 1 |  | todo |  |
| 0785e6 (ovl 5a05+0376) | 15 | 1 |  | todo |  |
| 0785f5 (ovl 5a05+0385) | 70 | 1 |  | todo |  |
| 07863b (ovl 5a05+03cb) | 243 | 2 |  | todo |  |
| 07872e (ovl 5a05+04be) | 181 | 4 |  | todo |  |
| 0787e3 (ovl 5a05+0573) | 198 | 1 |  | todo |  |
| 0788a9 (ovl 5a05+0639) | 317 | 3 |  | todo |  |

## ovl 5a0b (11 routines, 1503 bytes, 100% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 078a40 (ovl 5a0b+0000) | 100 | 1 |  | proved | proof_runes.py add_rune |
| 078aa4 (ovl 5a0b+0064) | 25 | 1 |  | proved | proof_runes.py clear_runes |
| 078abd (ovl 5a0b+007d) | 98 | 1 |  | proved | proof_runes.py ShowRune |
| 078b1f (ovl 5a0b+00df) | 97 | 0 |  | proved | proof_runes.py RedispRune |
| 078b80 (ovl 5a0b+0140) | 46 | 2 |  | proved | proof_runes.py clear_shelf |
| 078bae (ovl 5a0b+016e) | 330 | 1 |  | proved | proof_runes.py mous_in_rune |
| 078cf8 (ovl 5a0b+02b8) | 17 | 1 |  | proved | proof_runes.py not_a_spell |
| 078d09 (ovl 5a0b+02c9) | 208 | 0 |  | proved | proof_runes.py try_clear |
| 078dd9 (ovl 5a0b+0399) | 204 | 0 |  | proved | proof_runes.py try_cast |
| 078ea5 (ovl 5a0b+0465) | 35 | 1 |  | proved | proof_runes.py fail_spell |
| 078ec8 (ovl 5a0b+0488) | 343 | 1 |  | proved | proof_runes.py player_cast
 214: The spell backfires.\n / |

## ovl 5a11 (20 routines, 3734 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 079070 (ovl 5a11+0000) | 21 | 6 |  | todo |  |
| 079085 (ovl 5a11+0015) | 23 | 7 |  | todo |  |
| 07909c (ovl 5a11+002c) | 43 | 0 |  | todo |  |
| 0790c7 (ovl 5a11+0057) | 243 | 2 |  | todo |  |
| 0791ba (ovl 5a11+014a) | 210 | 4 |  | todo |  |
| 07928c (ovl 5a11+021c) | 560 | 5 |  | todo |  |
| 0794bc (ovl 5a11+044c) | 243 | 3 |  | todo |  |
| 0795af (ovl 5a11+053f) | 101 | 3 |  | todo |  |
| 079614 (ovl 5a11+05a4) | 23 | 1 |  | todo |  |
| 07962b (ovl 5a11+05bb) | 21 | 1 |  | todo |  |
| 079640 (ovl 5a11+05d0) | 21 | 5 |  | todo |  |
| 079655 (ovl 5a11+05e5) | 432 | 3 |  | todo |  |
| 079805 (ovl 5a11+0795) | 136 | 1 |  | todo |  |
| 07988d (ovl 5a11+081d) | 136 | 1 |  | todo |  |
| 079915 (ovl 5a11+08a5) | 48 | 4 |  | todo |  |
| 079945 (ovl 5a11+08d5) | 717 | 2 |  | todo |  |
| 079c12 (ovl 5a11+0ba2) | 109 | 2 |  | unproved |  |
| 079c7f (ovl 5a11+0c0f) | 462 | 1 |  | todo |  |
| 079e4d (ovl 5a11+0ddd) | 151 | 8 |  | todo |  |
| 079ee4 (ovl 5a11+0e74) | 34 | 4 |  | todo |  |

## ovl 5a1a (20 routines, 6472 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 079fa0 (ovl 5a1a+0000) | 72 | 3 |  | todo |  |
| 079fe8 (ovl 5a1a+0048) | 413 | 2 |  | todo |  |
| 07a185 (ovl 5a1a+01e5) | 5 | 1 |  | todo |  |
| 07a18a (ovl 5a1a+01ea) | 87 | 2 |  | todo |  |
| 07a1e1 (ovl 5a1a+0241) | 834 | 4 |  | todo | 186: You cannot barter a container.  Instead, remove the con / |
| 07a523 (ovl 5a1a+0583) | 180 | 1 |  | todo |  |
| 07a5d7 (ovl 5a1a+0637) | 58 | 1 |  | todo |  |
| 07a611 (ovl 5a1a+0671) | 555 | 2 |  | todo |  |
| 07a83c (ovl 5a1a+089c) | 190 | 11 |  | todo |  |
| 07a8fa (ovl 5a1a+095a) | 186 | 2 |  | todo |  |
| 07a9b4 (ovl 5a1a+0a14) | 311 | 3 |  | todo |  |
| 07aaeb (ovl 5a1a+0b4b) | 1472 | 5 | io 1 | unproved | 247: You can only put runes in the rune bag.\n / 248: That item does not fit.\n / 258: The |
| 07b0ab (ovl 5a1a+110b) | 84 | 2 |  | todo |  |
| 07b0ff (ovl 5a1a+115f) | 80 | 2 |  | todo |  |
| 07b14f (ovl 5a1a+11af) | 364 | 2 |  | todo |  |
| 07b2bb (ovl 5a1a+131b) | 773 | 1 |  | todo |  |
| 07b5c0 (ovl 5a1a+1620) | 70 | 6 |  | todo |  |
| 07b606 (ovl 5a1a+1666) | 449 | 8 |  | todo |  |
| 07b7c7 (ovl 5a1a+1827) | 133 | 3 |  | todo |  |
| 07b84c (ovl 5a1a+18ac) | 156 | 4 |  | todo |  |

## ovl 5a23 (10 routines, 3432 bytes, 100% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 07ba10 (ovl 5a23+0000) | 842 | 3 |  | proved | proof_look.py LookAt |
| 07bd5a (ovl 5a23+034a) | 98 | 2 |  | proved | proof_look.py do_mods |
| 07bdbc (ovl 5a23+03ac) | 474 | 2 |  | proved | proof_look.py do_of |
| 07bf96 (ovl 5a23+0586) | 315 | 1 |  | proved | proof_look.py BookLook
 151: Enscribed upon the scroll is your map.\n / |
| 07c0d1 (ovl 5a23+06c1) | 602 | 2 | io 2 | proved | proof_look.py RectLook
 171: You see a bridge.\n / |
| 07c32b (ovl 5a23+091b) | 219 | 1 |  | proved | proof_look.py BonesLook |
| 07c406 (ovl 5a23+09f6) | 66 | 1 |  | proved | proof_look.py KeyLook |
| 07c448 (ovl 5a23+0a38) | 473 | 1 |  | proved | proof_look.py CritterLook |
| 07c621 (ovl 5a23+0c11) | 178 | 1 |  | proved | proof_look.py SpecialLook
 131: The door is spiked.\n / |
| 07c6d3 (ovl 5a23+0cc3) | 165 | 1 |  | proved | proof_look.py talisman_desc
 260: You see / |

## ovl 5a29 (4 routines, 606 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 07c830 (ovl 5a29+0000) | 55 | 1 |  | todo |  |
| 07c867 (ovl 5a29+0037) | 63 | 0 |  | todo |  |
| 07c8a6 (ovl 5a29+0076) | 248 | 1 |  | todo |  |
| 07c99e (ovl 5a29+016e) | 240 | 1 |  | todo |  |

## ovl 5a2f (2 routines, 138 bytes, 100% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 07cac0 (ovl 5a2f+0000) | 58 | 0 |  | proved | proof_objclass.py misc_init |
| 07cafa (ovl 5a2f+003a) | 80 | 0 |  | proved | proof_objclass.py misc_class_data |

## ovl 5a32 (1 routines, 24 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 07cb50 (ovl 5a32+0000) | 24 | 1 |  | todo |  |

## ovl 5a35 (1 routines, 7 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 07cb70 (ovl 5a35+0000) | 7 | 0 | int 1 | todo |  |

## ovl 5a3a (3 routines, 299 bytes, 100% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 07cb90 (ovl 5a3a+0000) | 238 | 1 |  | proved | proof_objclass.py init_objects |
| 07cc7e (ovl 5a3a+00ee) | 7 | 0 |  | proved | proof_objclass.py Return0_ovr129_EE |
| 07cc85 (ovl 5a3a+00f5) | 54 | 2 |  | proved | proof_objclass.py get_class_data |

## ovl 5a3d (21 routines, 2094 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 07cce0 (ovl 5a3d+0000) | 289 | 3 |  | todo |  |
| 07ce01 (ovl 5a3d+0121) | 42 | 5 |  | todo |  |
| 07ce2b (ovl 5a3d+014b) | 59 | 3 |  | todo |  |
| 07ce66 (ovl 5a3d+0186) | 56 | 9 |  | todo |  |
| 07ce9e (ovl 5a3d+01be) | 86 | 7 |  | todo |  |
| 07cef4 (ovl 5a3d+0214) | 30 | 0 |  | todo |  |
| 07cf12 (ovl 5a3d+0232) | 90 | 0 |  | todo |  |
| 07cf6c (ovl 5a3d+028c) | 30 | 0 |  | todo |  |
| 07cf8a (ovl 5a3d+02aa) | 169 | 2 |  | todo |  |
| 07d033 (ovl 5a3d+0353) | 98 | 0 |  | todo |  |
| 07d095 (ovl 5a3d+03b5) | 88 | 0 |  | todo |  |
| 07d0ed (ovl 5a3d+040d) | 88 | 0 |  | todo |  |
| 07d145 (ovl 5a3d+0465) | 178 | 0 |  | todo |  |
| 07d1f7 (ovl 5a3d+0517) | 87 | 0 |  | todo |  |
| 07d24e (ovl 5a3d+056e) | 170 | 0 |  | todo |  |
| 07d2f8 (ovl 5a3d+0618) | 91 | 0 |  | todo |  |
| 07d353 (ovl 5a3d+0673) | 22 | 5 |  | todo |  |
| 07d369 (ovl 5a3d+0689) | 77 | 2 |  | todo |  |
| 07d3b6 (ovl 5a3d+06d6) | 28 | 2 |  | todo |  |
| 07d3d2 (ovl 5a3d+06f2) | 246 | 2 |  | todo |  |
| 07d4c8 (ovl 5a3d+07e8) | 70 | 0 |  | todo | 160: You cannot select options partway through an action.\n / |

## ovl 5a46 (7 routines, 1671 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 07d5b0 (ovl 5a46+0000) | 477 | 1 |  | todo |  |
| 07d78d (ovl 5a46+01dd) | 48 | 0 |  | todo |  |
| 07d7bd (ovl 5a46+020d) | 213 | 1 |  | todo |  |
| 07d892 (ovl 5a46+02e2) | 147 | 1 |  | todo |  |
| 07d925 (ovl 5a46+0375) | 295 | 4 |  | todo |  |
| 07da4c (ovl 5a46+049c) | 325 | 2 |  | todo |  |
| 07db91 (ovl 5a46+05e1) | 166 | 1 |  | todo |  |

## ovl 5a4d (12 routines, 2848 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 07dcc0 (ovl 5a4d+0000) | 5 | 0 |  | todo |  |
| 07dcc5 (ovl 5a4d+0005) | 251 | 1 |  | todo |  |
| 07ddc0 (ovl 5a4d+0100) | 228 | 1 |  | todo |  |
| 07dea4 (ovl 5a4d+01e4) | 120 | 1 |  | unproved |  |
| 07df1c (ovl 5a4d+025c) | 61 | 1 |  | todo |  |
| 07df59 (ovl 5a4d+0299) | 174 | 1 |  | todo |  |
| 07e007 (ovl 5a4d+0347) | 567 | 1 |  | unproved |  |
| 07e23e (ovl 5a4d+057e) | 132 | 2 |  | todo |  |
| 07e2c2 (ovl 5a4d+0602) | 91 | 2 |  | todo |  |
| 07e31d (ovl 5a4d+065d) | 208 | 1 |  | unproved |  |
| 07e3ed (ovl 5a4d+072d) | 87 | 1 |  | unproved |  |
| 07e444 (ovl 5a4d+0784) | 924 | 28 |  | unproved |  |

## ovl 5a53 (16 routines, 3821 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 07e830 (ovl 5a53+0000) | 34 | 2 |  | todo |  |
| 07e852 (ovl 5a53+0022) | 176 | 1 |  | todo |  |
| 07e902 (ovl 5a53+00d2) | 1545 | 1 |  | todo |  |
| 07ef0b (ovl 5a53+06db) | 605 | 1 |  | todo |  |
| 07f168 (ovl 5a53+0938) | 110 | 2 |  | todo |  |
| 07f1d6 (ovl 5a53+09a6) | 109 | 0 |  | todo |  |
| 07f243 (ovl 5a53+0a13) | 27 | 0 |  | todo | 275: Ultima Underworld: The Stygian Abyss v / |
| 07f25e (ovl 5a53+0a2e) | 201 | 2 |  | todo |  |
| 07f327 (ovl 5a53+0af7) | 184 | 1 |  | todo |  |
| 07f3df (ovl 5a53+0baf) | 249 | 6 |  | todo |  |
| 07f4d8 (ovl 5a53+0ca8) | 22 | 1 |  | todo |  |
| 07f4ee (ovl 5a53+0cbe) | 148 | 1 |  | todo |  |
| 07f582 (ovl 5a53+0d52) | 244 | 1 |  | todo |  |
| 07f676 (ovl 5a53+0e46) | 91 | 2 |  | todo |  |
| 07f6d1 (ovl 5a53+0ea1) | 37 | 0 |  | todo |  |
| 07f6f6 (ovl 5a53+0ec6) | 39 | 0 |  | todo |  |

## ovl 5a5e (1 routines, 5 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 07f8b0 (ovl 5a5e+0000) | 5 | 0 |  | todo |  |

## ovl 5a61 (9 routines, 2716 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 07f8c0 (ovl 5a61+0000) | 34 | 0 |  | todo |  |
| 07f8e2 (ovl 5a61+0022) | 114 | 0 |  | todo |  |
| 07f954 (ovl 5a61+0094) | 57 | 3 |  | todo |  |
| 07f98d (ovl 5a61+00cd) | 47 | 1 |  | todo |  |
| 07f9bc (ovl 5a61+00fc) | 332 | 3 |  | todo |  |
| 07fb08 (ovl 5a61+0248) | 809 | 2 |  | todo |  |
| 07fe31 (ovl 5a61+0571) | 503 | 1 |  | todo |  |
| 080028 (ovl 5a61+0768) | 445 | 2 |  | todo |  |
| 0801e5 (ovl 5a61+0925) | 375 | 1 |  | todo |  |

## ovl 5a66 (1 routines, 7 bytes, 100% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 080400 (ovl 5a66+0000) | 7 | 0 |  | proved | proof_objclass.py rect_class_data |

## ovl 5a69 (13 routines, 2539 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 080410 (ovl 5a69+0000) | 146 | 1 |  | todo |  |
| 0804a2 (ovl 5a69+0092) | 155 | 3 |  | todo |  |
| 08053d (ovl 5a69+012d) | 252 | 2 |  | todo |  |
| 080639 (ovl 5a69+0229) | 220 | 5 |  | todo |  |
| 080715 (ovl 5a69+0305) | 152 | 1 |  | todo |  |
| 0807ad (ovl 5a69+039d) | 174 | 1 |  | todo |  |
| 08085b (ovl 5a69+044b) | 201 | 2 |  | todo | 166: Restoring Game / 170: ... / |
| 080924 (ovl 5a69+0514) | 341 | 1 |  | todo | 167: Saving Game / 168: \\6     Please enter a save file description:\n / 170: ... / |
| 080a79 (ovl 5a69+0669) | 131 | 5 |  | todo |  |
| 080afc (ovl 5a69+06ec) | 207 | 3 |  | todo |  |
| 080bcb (ovl 5a69+07bb) | 198 | 2 |  | todo |  |
| 080c91 (ovl 5a69+0881) | 133 | 1 |  | todo |  |
| 080d16 (ovl 5a69+0906) | 229 | 3 |  | todo |  |

## ovl 5a70 (1 routines, 250 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 080ef0 (ovl 5a70+0000) | 250 | 5 |  | todo |  |

## ovl 5a73 (3 routines, 553 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 081010 (ovl 5a73+0000) | 282 | 3 |  | unproved |  |
| 08112a (ovl 5a73+011a) | 103 | 1 |  | todo |  |
| 081191 (ovl 5a73+0181) | 168 | 1 |  | todo |  |

## ovl 5a76 (24 routines, 6913 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 081270 (ovl 5a76+0000) | 112 | 1 |  | todo |  |
| 0812e0 (ovl 5a76+0070) | 37 | 0 |  | todo |  |
| 081305 (ovl 5a76+0095) | 133 | 5 |  | unproved |  |
| 08138a (ovl 5a76+011a) | 120 | 1 |  | unproved | 147: You have attained experience level / |
| 081402 (ovl 5a76+0192) | 29 | 2 |  | unproved |  |
| 08141f (ovl 5a76+01af) | 194 | 1 |  | unproved |  |
| 0814e1 (ovl 5a76+0271) | 335 | 2 |  | unproved |  |
| 081630 (ovl 5a76+03c0) | 70 | 1 |  | todo | 27: You cannot advance any further in that skill.\n / 28: You have advanced greatly in / |
| 081676 (ovl 5a76+0406) | 135 | 1 |  | todo | 29: You have advanced in / 30: None of your skills improved.\n / |
| 0816fd (ovl 5a76+048d) | 738 | 2 |  | unproved | 24: You are not ready to advance.\n / 25: That is not a mantra.\n / 26: Knowledge and Unde |
| 0819df (ovl 5a76+076f) | 1028 | 1 |  | todo |  |
| 081de3 (ovl 5a76+0b73) | 263 | 1 |  | todo |  |
| 081eea (ovl 5a76+0c7a) | 165 | 1 |  | todo |  |
| 081f8f (ovl 5a76+0d1f) | 950 | 3 |  | unproved | 14: There are hostile creatures near!\n / 15: You make camp.\n / 16: You go to sleep.\n / |
| 082345 (ovl 5a76+10d5) | 50 | 0 |  | todo |  |
| 082377 (ovl 5a76+1107) | 127 | 3 |  | todo |  |
| 0823f6 (ovl 5a76+1186) | 373 | 0 |  | todo | 279: The Slasher of Veils is dragged from this world.\n / 280: You are sucked through the |
| 08256b (ovl 5a76+12fb) | 434 | 1 |  | todo |  |
| 08271d (ovl 5a76+14ad) | 116 | 2 |  | todo |  |
| 082791 (ovl 5a76+1521) | 211 | 0 |  | unproved |  |
| 082864 (ovl 5a76+15f4) | 28 | 0 |  | todo |  |
| 082880 (ovl 5a76+1610) | 537 | 1 |  | unproved |  |
| 082a99 (ovl 5a76+1829) | 164 | 2 |  | todo |  |
| 082b3d (ovl 5a76+18cd) | 564 | 2 |  | todo |  |

## ovl 5a80 (1 routines, 7 bytes, 100% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 082f30 (ovl 5a80+0000) | 7 | 0 |  | proved | proof_objclass.py spec_class_data |

## ovl 5a83 (9 routines, 1292 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 082f40 (ovl 5a83+0000) | 243 | 1 |  | todo |  |
| 083033 (ovl 5a83+00f3) | 92 | 1 |  | todo |  |
| 08308f (ovl 5a83+014f) | 116 | 2 |  | todo |  |
| 083103 (ovl 5a83+01c3) | 115 | 2 |  | todo |  |
| 083176 (ovl 5a83+0236) | 78 | 2 |  | todo |  |
| 0831c4 (ovl 5a83+0284) | 219 | 2 |  | todo |  |
| 08329f (ovl 5a83+035f) | 230 | 0 |  | todo |  |
| 083385 (ovl 5a83+0445) | 182 | 1 |  | todo |  |
| 08343b (ovl 5a83+04fb) | 17 | 3 |  | todo |  |

## ovl 5a88 (1 routines, 7 bytes, 100% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 0834b0 (ovl 5a88+0000) | 7 | 0 |  | proved | proof_objclass.py stuff_class_data |

## ovl 5a8f (1 routines, 23 bytes, 100% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 0834e0 (ovl 5a8f+0000) | 23 | 0 |  | proved | proof_trigsave.py trap_save |

## ovl 5a92 (7 routines, 1437 bytes, 100% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 083500 (ovl 5a92+0000) | 302 | 2 |  | proved | proof_treasure.py drop_link_chain |
| 08362e (ovl 5a92+012e) | 44 | 1 |  | proved | proof_treasure.py drop_some_objects |
| 08365a (ovl 5a92+015a) | 353 | 1 |  | proved | proof_treasure.py generate_treasure |
| 0837bb (ovl 5a92+02bb) | 102 | 1 |  | proved | proof_treasure.py generate_food |
| 083821 (ovl 5a92+0321) | 289 | 1 |  | proved | proof_treasure.py generate_weapons |
| 083942 (ovl 5a92+0442) | 212 | 1 |  | proved | proof_treasure.py generate_equipment |
| 083a16 (ovl 5a92+0516) | 135 | 5 |  | proved | proof_treasure.py generate_inventory |

## ovl 5a99 (3 routines, 24 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 083ae0 (ovl 5a99+0000) | 5 | 0 |  | todo |  |
| 083ae5 (ovl 5a99+0005) | 14 | 0 |  | todo |  |
| 083af3 (ovl 5a99+0013) | 5 | 0 |  | todo |  |

## ovl 5a9c (17 routines, 5802 bytes, 86% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 083b00 (ovl 5a9c+0000) | 23 | 0 |  | proved | proof_trigsave.py trap_init |
| 083b17 (ovl 5a9c+0017) | 36 | 0 |  | proved | proof_trigger.py trap_class_data |
| 083b3b (ovl 5a9c+003b) | 527 | 8 |  | proved | proof_trigger.py UseTrigger |
| 083d4a (ovl 5a9c+024a) | 76 | 3 |  | proved | proof_trigger.py SetOffTrap |
| 083d96 (ovl 5a9c+0296) | 2860 | 4 |  | proved | proof_trigger.py UseTrap |
| 0848c2 (ovl 5a9c+0dc2) | 193 | 2 |  | todo |  |
| 084983 (ovl 5a9c+0e83) | 190 | 5 |  | proved | proof_trigger.py delete_trap |
| 084a41 (ovl 5a9c+0f41) | 58 | 0 |  | proved | proof_trigger.py gronkify_talkto_player |
| 084a7b (ovl 5a9c+0f7b) | 399 | 1 | io 8 | proved | proof_trigger.py do_trap_hack |
| 084c0a (ovl 5a9c+110a) | 594 | 1 |  | todo |  |
| 084e5c (ovl 5a9c+135c) | 166 | 1 |  | proved | proof_trigger.py trigger_obj_del |
| 084f02 (ovl 5a9c+1402) | 59 | 1 |  | proved | proof_trigger.py trap_obj_del |
| 084f3d (ovl 5a9c+143d) | 81 | 0 |  | proved | proof_trigger.py is_this_wandering |
| 084f8e (ovl 5a9c+148e) | 54 | 1 |  | todo |  |
| 084fc4 (ovl 5a9c+14c4) | 83 | 2 |  | proved | proof_trigger.py check_alert |
| 085017 (ovl 5a9c+1517) | 173 | 2 |  | proved | proof_trigger.py DoWanderingMonsters |
| 0850c4 (ovl 5a9c+15c4) | 230 | 1 |  | proved | proof_trigger.py DoClosingDoors |

## ovl 5aa4 (20 routines, 1734 bytes, 0% done)

| Routine | Size | Callers | Hints | Status | Note / messages |
|---|---|---|---|---|---|
| 085290 (ovl 5aa4+0000) | 23 | 0 |  | todo |  |
| 0852a7 (ovl 5aa4+0017) | 101 | 0 |  | todo |  |
| 08530c (ovl 5aa4+007c) | 133 | 2 |  | todo |  |
| 085391 (ovl 5aa4+0101) | 26 | 1 |  | todo |  |
| 0853ab (ovl 5aa4+011b) | 192 | 3 |  | todo |  |
| 08546b (ovl 5aa4+01db) | 193 | 0 |  | todo |  |
| 08552c (ovl 5aa4+029c) | 53 | 2 |  | todo |  |
| 085561 (ovl 5aa4+02d1) | 32 | 1 |  | todo |  |
| 085581 (ovl 5aa4+02f1) | 23 | 0 |  | todo |  |
| 085598 (ovl 5aa4+0308) | 43 | 1 |  | todo |  |
| 0855c3 (ovl 5aa4+0333) | 65 | 1 |  | todo |  |
| 085604 (ovl 5aa4+0374) | 51 | 1 |  | todo |  |
| 085637 (ovl 5aa4+03a7) | 167 | 1 |  | todo |  |
| 0856de (ovl 5aa4+044e) | 86 | 1 |  | todo |  |
| 085734 (ovl 5aa4+04a4) | 83 | 8 |  | todo |  |
| 085787 (ovl 5aa4+04f7) | 77 | 25 |  | todo |  |
| 0857d4 (ovl 5aa4+0544) | 77 | 9 |  | todo |  |
| 085821 (ovl 5aa4+0591) | 124 | 1 |  | todo |  |
| 08589d (ovl 5aa4+060d) | 132 | 1 |  | todo |  |
| 085921 (ovl 5aa4+0691) | 53 | 0 |  | todo |  |

