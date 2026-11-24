# JS8 decoder component — third-party notices

The JS8 LDPC(174,87) parity-check graph, parity-generator matrix, belief-propagation decoding method, and 75-bit payload/CRC-12 layout in `Js8Ldpc.cpp` are adapted from the JS8Call implementation.

- Upstream project: https://github.com/js8call/js8call
- Upstream source: [JS8.cpp](https://github.com/js8call/js8call/blob/a7ff1be0b389d287fdc56e2ea0d06962aa68127d/JS8.cpp)
- Upstream revision inspected: `a7ff1be0b389d287fdc56e2ea0d06962aa68127d`
- Upstream project license: GNU General Public License version 3 (GPL-3.0); full text is in [COPYING](COPYING).
- The port is reduced to the JS8 message/LDPC core for the ATS Mini's C++17/ESP32 environment. Qt, Boost, FFTW, Eigen, transmit support, and desktop application code are not included.
- The JS8 component is GPL-3.0-only. The existing ATS Mini files retain their existing notices; this file does not relicense unrelated code.

The decoder is receive-side infrastructure only. It does not add any transmit, PTT, or radio-control path.

## Distribution note

This repository is private for development. The presence of a GPL-3.0-only decoder component does not by itself settle the licensing of a firmware image that statically links it with other ATS Mini code. Before distributing firmware or source to anyone else, review the combined-work implications and comply with all applicable GPLv3 requirements, including corresponding source and (where applicable) installation information. Do not describe a distributed combined firmware image as MIT-only without a separate legal review.
