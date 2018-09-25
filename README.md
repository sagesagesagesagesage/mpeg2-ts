MPEG2-TS Tool
=====================
Overview

This repository publishes tools targeting MPEG2-TS data.


## Description

This repository publishes tools targeting MPEG2-TS data.
Currently the following tools are available.

base: Basic TS file analyzer
spliter: Fetches the MPEG2-TS file at the TOT time contained in the file.

## How to use

ts_base

./ts_base -i input.ts
./ts_base -i input.ts > output.csv

spliter

./ts_tot_spliter  -i input.ts -o output.ts -s 2018/09/01-10:00:00 -e 2018/09/01-11:00:00
