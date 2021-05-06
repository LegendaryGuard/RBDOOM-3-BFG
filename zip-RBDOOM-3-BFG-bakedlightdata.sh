#!/bin/sh

7z a RBDOOM-3-BFG-1.3.0.31-baseSP_bakedlightdata.7z -r base/env/ base/maps/*.lightgrid -mx9 -x!generated

for i in `ls *7z*`; do sha256sum $i >> SHA256SUMS.txt; done
