# Benchmark mode of the standalone benchmark

The standalone benchmark (`./ca`, or `o2-gpu-standalone-benchmark` in O2) has a benchmark mode `--benchmark <mode>`, which sets and checks all options needed for a reproducible performance measurement of the GPU reconstruction, and prints the timing only once at the end. How to build the standalone benchmark and how to obtain datasets is described in [build-standalone.md](https://github.com/AliceO2Group/AliceO2/blob/dev/GPU/documentation/build-standalone.md).

# 1. Syntax

```
./ca -e <dataset> --benchmark [sync|async|combined|alldetectors]-[gpu|cpu]-[wall|kernel] (--runs N | -n N) [--gpuType CUDA|HIP|OCL|CPUDEVICE|...] [-s S]
```

The benchmark string has 3 parts, separated by `-`:

| Part | Value | Meaning |
|------|-------|---------|
| Processing mode | `sync` | Synchronous reconstruction as online (`--sync`): ZS raw data in, cluster finding, TPC tracking, compression. |
| | `async` | Asynchronous reconstruction (`--async`): compressed clusters (CTF) in, decoding, TPC tracking with the full settings. |
| | `combined` | Same input and steps as `sync`, but neither the sync nor the async settings are applied (default settings). |
| | `alldetectors` | Like `combined`, but with TRD tracking. Requires TPC ZS raw data and TRD tracklets in the dataset. |
| Device | `gpu` | Run on a GPU backend (`-g`). CPUDevice counts as GPU. |
| | `cpu` | Run on the CPU backend (`-c`). |
| Timing | `wall` | Measure the wall time of the processing of a time frame (TF) (`--debug 0`). |
| | `kernel` | Measure the time of the individual GPU kernels / CPU tasks (`--debug 1`). |

Options that must or may be given in addition:

- `--runs N` or `-n N` (one of them is required):
  - `--runs N`: process the same TF `N` times (TF number `-s`, default 0).
  - `-n N`: process the TFs `s` to `N - 1` (`-n` is the end index, not the count, the number of iterations is `N - s`), each one once. With double pipeline (see below) they are preloaded and processed round-robin in the pipeline.
  - Both may be given if they agree (`--runs` = `N - s`); `-n s+1` (a single TF) may be combined with any `--runs`.
  - At least 5 iterations are required, at least 10 with double pipeline. The first iteration is excluded from the average (`--runsInit`, default 1), with double pipeline the first 2 iterations.
  - The dataset must contain at least `N` TFs with `-n`.
- `--gpuType`: the GPU backend for `gpu` (`CUDA`, `HIP`, `OCL`, `METAL`, `CPUDEVICE`); default `AUTO` takes the first available one of CUDA, HIP, OpenCL, Metal. With `cpu`, `--gpuType` must be `AUTO` or `CPU`.
- `-s S`: first TF (default 0).
- `-e <dataset>`: the input must match the processing mode: TPC ZS raw data (`tpcZS`) for `sync`, `combined` and `alldetectors`, compressed clusters for `async` (e.g. the `*-ctf` datasets). Otherwise the benchmark fails at loading the TF.
- `--runsExternal N`: repeat the whole benchmark `N` times (each repetition reports its own result).

The following are checked and give an error:
- an unknown benchmark string,
- `--sync` / `--async` not matching the processing mode, `--syncAsync`,
- `--recoSteps` (the steps must not be changed, `--recoStepsGPU` is allowed),
- `--debug` other than the value of the timing mode (0 for `wall`, 1 for `kernel`),
- neither `-n` nor `--runs`, `-n` and `--runs` disagreeing, too few iterations, not enough TFs in the dataset,
- a device that does not match the benchmark (checked after the backend is created, e.g. `--gpuType CPU` with `gpu`),
- input data not matching the processing mode.

`ca` returns exit code 1 for all errors.

## Pinning CPU cores, memory, and GPUs to one NUMA domain

On multi-socket servers and on CPUs with several NUMA domains, it is important to pin the CPU cores and the host memory of the process, and to select a GPU attached to the same NUMA domain. Otherwise host memory accesses and DMA transfers cross the socket interconnect, and the results fluctuate and are too slow. E.g. for an AMD GPU:

```
ROCR_VISIBLE_DEVICES=0 numactl --membind 0 --cpunodebind 0 ./ca -e 50kHz --benchmark sync-gpu-wall --runs 10
```

and for an NVIDIA GPU:

```
CUDA_VISIBLE_DEVICES=0 numactl --membind 0 --cpunodebind 0 ./ca -e 50kHz --benchmark sync-gpu-wall --runs 10
```

The NUMA domain of a GPU can be found with `nvidia-smi topo -m`, `rocm-smi --showtoponuma`, `lstopo`, or `cat /sys/bus/pci/devices/<PCI address>/numa_node`.

Selecting the GPU with `ROCR_VISIBLE_DEVICES` / `HIP_VISIBLE_DEVICES` / `CUDA_VISIBLE_DEVICES` is preferable to the `--gpuDevice` option of the standalone benchmark: with `--gpuDevice`, the runtime still sees all GPUs and maps their memory into the process address space (and the backend creates a context on each GPU while selecting the device), while with the environment variable the runtime sees only the selected GPU, so less memory is mapped and less is initialized.

The CPU benchmark should be pinned in the same way; the number of CPU threads can be set with `--PROCnThreads` (default: `TBB_NUM_THREADS` or `OMP_NUM_THREADS` if set, otherwise all cores of the affinity mask, i.e. respecting `numactl`).

# 2. Output examples and how to read them

The examples below were made on the `50kHz` dataset (one 32-orbit TF of Pb-Pb collisions at 50 kHz, ZS raw data) on a 32-core CPU, with the `CPUDEVICE` backend standing in for a GPU (it runs the GPU code path with the CPU kernels on the host). The output of a real GPU backend has the same format, only the times differ. `[...]` marks removed lines.

## sync-cpu-wall

```
$ ./ca -e 50kHz --benchmark sync-cpu-wall --runs 5
Reading events from Directory 50kHz
Created GPUReconstruction instance for device type CPU (1)
Read event settings from dir events/50kHz/ (solenoidBz: 5.006668, constBz 0, maxTimeBin 14257)
Standalone Test Framework for CA Tracker - Using CPU
Using random seed -1497483323
Loading time: 263,363 us
Processing Event 0
Run 1 (thread 0)
Event has 253799 8kb TPC ZS pages (version 4), 888107032 digits
Event has 92348578 TPC Clusters, 0 TRD Tracklets
Output Tracks (O2): 757312 - Clusters: 46097261 / 92348578 / 86121328 / 0 / 0 (attached / total / compressed / fitted / adjacent)
[... Run 2 to 4 ...]
Run 5 (thread 0)
Event has 253799 8kb TPC ZS pages (version 4), 888107032 digits
Event has 92348578 TPC Clusters, 0 TRD Tracklets
Output Tracks (O2): 757502 - Clusters: 46104404 / 92348578 / 86121895 / 0 / 0 (attached / total / compressed / fitted / adjacent)
Total Wall Time:   27172700 us   (avergage of 4 runs)
Sync phase: 31.64 sec per 32 orbit TF, 11120.1 GPUs required (Measured wall time - Extrapolated from 92348578 clusters to 107546607)
```

- Each run prints the input (`Event has ...`: ZS pages, digits, clusters found by the cluster finder) and the output (`Output Tracks`: tracks and attached / total / compressed clusters). The track and cluster counts vary slightly between runs, since the reconstruction is not deterministic (see [deterministic-mode.md](https://github.com/AliceO2Group/AliceO2/blob/dev/GPU/documentation/deterministic-mode.md)). They are a sanity check, e.g. when comparing backends or versions.
- `Total Wall Time` is the result: 27.2 s per TF, averaged over runs 2 to 5 (`average of 4 runs`, the first run is excluded).
- The `Sync phase` line extrapolates it from the 92.3 M clusters of this TF to the 107.5 M clusters of an average 50 kHz Pb-Pb TF: 31.6 s, i.e. 11120 such CPUs (with 32 cores each) would be needed to process 50 kHz Pb-Pb in real time. For a CPU benchmark, read "GPUs" as "CPUs".

## sync-gpu-wall (with double pipeline)

```
$ ./ca -e 50kHz --benchmark sync-gpu-wall --gpuType CPUDEVICE --runs 10
Reading events from Directory 50kHz
GPU Tracker library loaded and GPU tracker object created sucessfully
Created GPUReconstruction instance for device type CPUDEVICE (6)
Created GPUReconstruction instance for device type CPUDEVICE (6) (slave)
Read event settings from dir events/50kHz/ (solenoidBz: 5.006668, constBz 0, maxTimeBin 14257)
Standalone Test Framework for CA Tracker - Using GPU
Using RTC cache file rtc.cpudevice.cache.0854D04A70
CPUDevice Initialisation successfull (32 blocks, 8 streams, 4294967296 / 12000000000 bytes host / device memory, Constant memory 26920)
GPU Tracker initialization successfull
CPUDevice Initialisation successfull (from master)
Rescaling buffer size limits from 20500000000 to 12000000000 bytes of memory (factor 0.585366)
Rescaling buffer size limits from 20500000000 to 11499264064 bytes of memory (factor 0.560940)
Using random seed 95342693
Loading time: 261,887 us
Processing Event 0 in Pipeline 10 times
Run 1 (thread 0)
Event has 253799 8kb TPC ZS pages (version 4), 888107032 digits
Event has 92327129 TPC Clusters, 0 TRD Tracklets
Output Tracks (O2): 757383 - Clusters: 46087384 / 92327129 / 86100255 / 0 / 0 (attached / total / compressed / fitted / adjacent)
Run 2 (thread 0)
Event has 253799 8kb TPC ZS pages (version 4), 888107032 digits
Event has 92327129 TPC Clusters, 0 TRD Tracklets
Output Tracks (O2): 757330 - Clusters: 46084191 / 92327129 / 86102928 / 0 / 0 (attached / total / compressed / fitted / adjacent)
Run 3 (thread 0)
Run 4 (thread 1)
Event has 253799 8kb TPC ZS pages (version 4), 888107032 digits
Event has 92327129 TPC Clusters, 0 TRD Tracklets
Event has 253799 8kb TPC ZS pages (version 4), 888107032 digits
Event has 92327129 TPC Clusters, 0 TRD Tracklets
Output Tracks (O2): 757356 - Clusters: 46084848 / 92327129 / 86100156 / 0 / 0 (attached / total / compressed / fitted / adjacent)
Run 5 (thread 0)
[... until Run 10 ...]
Pipeline wall time: 211.651554, 8 iterations, 26.456444 per event
Sync phase: 30.82 sec per 32 orbit TF, 10829.5 GPUs required (Measured wall time - Extrapolated from 92327129 clusters to 107546607)
Maximum Memory Allocation: Host 1,193,148,416 / Device 10,904,500,224
```

- Two instances are created (the second one is the `slave`), both with RTC: here the compiled code was found in the cache (`Using RTC cache file`), otherwise the compilation runs first (`Starting ... RTC Compilation`, `RTC Compilation finished (... seconds)`, `Storing RTC compilation result in cache file ...`).
- `Rescaling buffer size limits`: the buffer size estimates are scaled to the memory pool given by `--memSize`. This is normal.
- Runs 1 and 2 are the warm-up, one per instance, one after the other. From run 3 on, the two instances (`thread 0` / `thread 1`) run concurrently, so their output lines interleave.
- `Pipeline wall time` is the result: 211.7 s for the 8 pipelined iterations (runs 3 to 10), 26.5 s per TF. This is the throughput, it includes the overlap of the input / output of one TF with the processing of the other. No `Total Wall Time` is printed in this mode.
- The `Sync phase` line extrapolates the pipeline time per TF as above.
- `Maximum Memory Allocation`: peak use of the host and device memory pools, to check how much margin `--memSize` has (10.9 GB of 12 GB here).

## sync-gpu-kernel

```
$ ./ca -e 50kHz --benchmark sync-gpu-kernel --gpuType CPUDEVICE --runs 5
[...]
Memory ptrs: Device (12000000000 bytes): 0x7f6703f4f040 - Host (2147483648 bytes): 0x7f6683f4e040
Using RTC cache file rtc.cpudevice.cache.0854D04A70
Loaded 104 CPUDevice RTC kernels from 104 libraries
CPUDevice Initialisation successfull (32 blocks, 8 streams, 2147483648 / 12000000000 bytes host / device memory, Constant memory 26920)
GPU Tracker initialization successfull
Rescaling buffer size limits from 20500000000 to 12000000000 bytes of memory (factor 0.585366)
Enabled Reconstruction Steps: 0x7ffffcef (on GPU: 0x7ffffcef) - TPC Transformation (G) - TPC Sector Tracking (G) - TPC Track Merging and Fit (G) - TPC Compression (G) - [...]
Using random seed -473613791
Loading time: 265,060 us
Processing Event 0
Run 1 (thread 0)
[... Run 1 to 5 as above ...]
Execution Time: Task (K     1632x):                                      GPUMemClean16 Time:  1,207,979 us
Execution Time: Task (K      576x):                          GPUTPCCFDecodeZSDenseLink Time:  1,154,337 us
Execution Time: Task (K      144x):                           GPUTPCCFCheckPadBaseline Time:    211,133 us (  19.821 GB/s -  4,184,893,440 bytes -     29,061,760 per call)
Execution Time: Task (K      576x):                                 GPUTPCCFPeakFinder Time:  2,028,258 us
[...]
Execution Time: Task (K      144x):                          GPUTPCTrackletConstructor Time:  5,525,946 us
[...]
Execution Time: Task (K        4x):                         GPUTPCGMMergerTrackFit_def Time:  2,511,689 us
[...]
Execution Time: Task (K        4x):           GPUTPCCompressionKernels_step1unattached Time:    226,217 us
Execution Time: Step              :       Tasks                     TPC Transformation Time:          0 us                                                                  ( Total Time :              2 us, CPU Time :              9 us,    5.47x )
Execution Time: Step              :       Tasks                    TPC Sector Tracking Time:  7,192,605 us                                                                  ( Total Time :      7,231,439 us, CPU Time :    153,442,086 us,   21.22x )
Execution Time: Step (D      148x):  DMA to GPU                    TPC Sector Tracking Time:      1,620 us (   0.279 GB/s -        451,584 bytes -          3,051 per call)
Execution Time: Step (D      292x): DMA to Host                    TPC Sector Tracking Time:      3,179 us (   0.003 GB/s -          8,176 bytes -             28 per call)
Execution Time: Step              :       Tasks              TPC Track Merging and Fit Time:  8,146,231 us                                                                  ( Total Time :      8,348,434 us, CPU Time :    199,641,803 us,   23.91x )
[...]
Execution Time: Step              :       Tasks                    TPC Cluster Finding Time:  9,385,483 us                                                                  ( Total Time :     11,180,328 us, CPU Time :    254,368,064 us,   22.75x )
Execution Time: Step (D    13264x):  DMA to GPU                    TPC Cluster Finding Time:    525,526 us (   4.003 GB/s -  2,103,499,528 bytes -        158,587 per call)
Execution Time: Step (D     2352x): DMA to Host                    TPC Cluster Finding Time:    129,677 us (   8.545 GB/s -  1,108,059,180 bytes -        471,113 per call)
Execution Time: General Step      :                                            Prepare Time:     17,015 us
Execution Time: Total   :                                       Total Kernel Time: 25,403,766 us   (avergage of 4 runs)
Execution Time: Total   :                                         Total Wall Time: 49,756,826 us ( CPU Time : 1,283,795,429 us,   25.80x )    (avergage of 4 runs)
Maximum Memory Allocation: Host 687,669,248 / Device 10,403,805,184
```

- The timing table is printed once, after the last run, as average per TF over runs 2 to 5. The counts (`K 576x`) are summed over these 4 runs, i.e. per TF the kernel `GPUTPCCFPeakFinder` runs 144 times (once per sector and cluster-finder fragment).
- `Task (K ...)` lines: time per TF of each kernel, summed over all its calls; for some kernels also the memory bandwidth. Use them to find the expensive kernels, and to compare kernels between versions or GPUs.
- `Step ... Tasks` lines: per reconstruction step the summed kernel time, the total time of the step (incl. gaps, transfers and host work, `Total Time`), and the CPU time used by the process during the step with the ratio CPU time / total time (here the CPUDevice kernels use the CPU cores, on a real GPU the ratio is small).
- `Step (D ...)` lines: DMA transfers per step and direction, with number of transfers, time, bandwidth and bytes. Low bandwidth with many small transfers points to inefficient transfers.
- `Total Kernel Time` is the result: 25.4 s per TF of pure kernel time. There is no `Sync phase` extrapolation in `kernel` mode, since the kernel time is not the time needed per TF.
- `Total Wall Time` (49.8 s) is much larger than in the wall benchmark, due to the synchronization after every kernel; it is not a meaningful wall time.
- With the `CPUDEVICE` backend, the `TPC Compression` step shows millions of small DMA transfers to the host: CPUDevice uses the CPU parameters, which gather the compressed clusters to the host element-wise. Real GPUs gather them on the GPU (`COMP_GATHER_MODE` 3) and transfer one block.

# 3. Settings applied by the benchmark mode, and how to run the benchmarks manually

The benchmark mode applies these settings (before all other option processing, since they enable further settings):

| Setting | Value | Condition |
|---------|-------|-----------|
| `--sync` | 1 | `sync` |
| `--async` | 1 | `async` |
| `-g` / `-c`, `--runGPUforce 1` | GPU / CPU, no fall back to the CPU | always |
| `--runTRD` | 0 | `sync`, `combined` |
| `--runTRD` | 1 | `alldetectors` |
| `--tfTime` | 1 | `wall` |
| `--debug` | 0 / 1 | `wall` / `kernel` |
| `--PROCdebugSilent` | 1, cleared for the last iteration without double pipeline | always |
| `--PROCdoublePipeline` | 1 | `gpu` with `sync` or `combined`, and `wall` |
| `--memSize` | 12000000000 | `gpu`, if not set |
| `--inputMemory` | 6000000000 | `gpu`, if not set |
| `--outputMemory` | 10000000000 | `gpu`, if not set |
| `--RTCenable 1 --RTCcacheOutput 1` | | `gpu`, if the backend supports RTC (CUDA, HIP, CPUDevice) |
| `--runs` | 1 with `-n`, `N - s` with `-n` and double pipeline, else as given | always |
| `-n` | `s + 1` with `--runs` | always |
| `--preloadEvents` | 1 | `-n` with double pipeline |

In addition, the benchmark mode averages the timers over all TFs with `-n` (normally they are reset for each TF), and it checks the input type of each TF.

The equivalent manual command lines with the CUDA backend (`50kHz` is a dataset with ZS raw data, `50kHz-ctf` the corresponding compressed clusters):

```
# sync-gpu-wall
./ca -e 50kHz -g --gpuType CUDA --runGPUforce 1 --sync --runTRD 0 --tfTime --debug 0 --PROCdoublePipeline 1 --memSize 12000000000 --inputMemory 6000000000 --outputMemory 10000000000 --RTCenable 1 --RTCcacheOutput 1 --runs 10
# sync-gpu-wall with -n 10: iterate over 10 different TFs in the pipeline
./ca -e 50kHz -g --gpuType CUDA --runGPUforce 1 --sync --runTRD 0 --tfTime --debug 0 --PROCdoublePipeline 1 --memSize 12000000000 --inputMemory 6000000000 --outputMemory 10000000000 --RTCenable 1 --RTCcacheOutput 1 -n 10 --runs 10 --preloadEvents
# sync-gpu-kernel
./ca -e 50kHz -g --gpuType CUDA --runGPUforce 1 --sync --runTRD 0 --debug 1 --memSize 12000000000 --inputMemory 6000000000 --outputMemory 10000000000 --RTCenable 1 --RTCcacheOutput 1 --runs 5
# combined-gpu-wall
./ca -e 50kHz -g --gpuType CUDA --runGPUforce 1 --runTRD 0 --tfTime --debug 0 --PROCdoublePipeline 1 --memSize 12000000000 --inputMemory 6000000000 --outputMemory 10000000000 --RTCenable 1 --RTCcacheOutput 1 --runs 10
# combined-gpu-kernel
./ca -e 50kHz -g --gpuType CUDA --runGPUforce 1 --runTRD 0 --debug 1 --memSize 12000000000 --inputMemory 6000000000 --outputMemory 10000000000 --RTCenable 1 --RTCcacheOutput 1 --runs 5
# async-gpu-wall / async-gpu-kernel (no double pipeline, --runTRD unchanged)
./ca -e 50kHz-ctf -g --gpuType CUDA --runGPUforce 1 --async --tfTime --debug 0 --memSize 12000000000 --inputMemory 6000000000 --outputMemory 10000000000 --RTCenable 1 --RTCcacheOutput 1 --runs 5
./ca -e 50kHz-ctf -g --gpuType CUDA --runGPUforce 1 --async --debug 1 --memSize 12000000000 --inputMemory 6000000000 --outputMemory 10000000000 --RTCenable 1 --RTCcacheOutput 1 --runs 5
# alldetectors-gpu-wall / alldetectors-gpu-kernel (no double pipeline)
./ca -e <dataset with TRD> -g --gpuType CUDA --runGPUforce 1 --runTRD 1 --tfTime --debug 0 --memSize 12000000000 --inputMemory 6000000000 --outputMemory 10000000000 --RTCenable 1 --RTCcacheOutput 1 --runs 5
./ca -e <dataset with TRD> -g --gpuType CUDA --runGPUforce 1 --runTRD 1 --debug 1 --memSize 12000000000 --inputMemory 6000000000 --outputMemory 10000000000 --RTCenable 1 --RTCcacheOutput 1 --runs 5
# The cpu variants: -c instead of the GPU, memory, RTC, and double pipeline options, e.g. sync-cpu-wall / sync-cpu-kernel
./ca -e 50kHz -c --sync --runTRD 0 --tfTime --debug 0 --runs 5
./ca -e 50kHz -c --sync --runTRD 0 --debug 1 --runs 5
# combined-cpu-*, async-cpu-*, alldetectors-cpu-*: the same with the mode options of the gpu variants above
```

Differences of the manual commands to the benchmark mode:
- `--PROCdebugSilent 1` is left out, since the manual command cannot clear it for the last iteration. The manual commands therefore print the timing of every iteration (each one is the running average of the iterations so far, without the first). The last one is the result.
- With `-n` and without double pipeline, the manual command resets the timers for each TF, so the printed result is that of the last TF only, not the average over the TFs.
- The input type of the TFs is not checked.

# 4. Meaning of the settings, and what is measured

- `--sync` / `--async`: apply the settings of the synchronous / asynchronous reconstruction (`GPUChainTracking::ApplySyncSettings`), e.g. without material lookup table and with reduced dEdx and rebuild settings in sync mode, with material lookup table and full dEdx in async mode. Neither for `combined` and `alldetectors`.
- `-g` / `-c` with `--runGPUforce 1`: select the GPU / CPU backend; with `--runGPUforce` the benchmark fails instead of falling back to the CPU if the GPU backend is not available.
- `--runTRD 0` / `1`: disable / enable TRD tracking. The TPC benchmarks (`sync`, `combined`) disable it, since it is not part of the TPC processing and the double pipeline does not support it; `alldetectors` enables it.
- `--debug 0` / `--debug 1`: the timing mode, see below.
- `--PROCdebugSilent 1`: suppress the per-TF output of the debug levels (timing tables, wall time, debug messages), but still run the debug code paths. The benchmark clears it for the last iteration, so the timing is printed once, as average over all iterations except the first.
- `--tfTime`: print the time extrapolated to a 50 kHz Pb-Pb TF (see below), only in `wall` mode.
- `--memSize`: size of the GPU memory pool. The reconstruction allocates all its GPU memory once at initialization and manages it itself, the pool must be large enough for the largest TF. 12 GB are sufficient for 32-orbit 50 kHz Pb-Pb TFs.
- `--inputMemory` / `--outputMemory`: size of the external input / output buffers in host memory, allocated once and registered (page-locked) for the GPU, as O2 does with its shared memory. The input buffer holds the raw input of the TF (not used with `--preloadEvents`), the output buffer the output (tracks, clusters, compressed clusters). With double pipeline, there is one output buffer per pipeline instance.
- `--RTCenable 1 --RTCcacheOutput 1`: run-time compilation (RTC) of the GPU code, as used in O2 production. At initialization, the GPU code is compiled again with the actual configuration, in which all settings marked as RTC options become compile-time constants (`--RTCoptConstexpr`, default on), and code that is not needed in the current mode (e.g. sync) is removed (`--RTCoptSpecialCode`, default: on in sync mode). This gives faster kernels with fewer registers. The compilation takes from tens of seconds to minutes and is not part of the measured time. With `--RTCcacheOutput`, the result is stored in a cache file `rtc.<backend>.cache.<id>` in `--RTCTECHcacheFolder` (default `./rtccache/`) and reused by the next run with the same configuration (the id is a hash of the source code, the parameters, the compiler commands and versions, and the RTC settings). The log says `Using RTC cache file ...` or `Storing RTC compilation result in cache file ...`. Backends without RTC (OpenCL, Metal) run the precompiled code. Details: [run-time-compilation.md](https://github.com/AliceO2Group/AliceO2/blob/dev/GPU/documentation/run-time-compilation.md).
- `--PROCdoublePipeline 1`: two reconstruction instances (master and slave) share the GPU and its memory pool and process consecutive TFs alternately, as the GPU reconstruction in O2 does in synchronous processing. While one instance finishes its TF (output transfer to the host and host-side finalization), the other one already transfers and processes the input of the next TF, so the GPU does not idle between TFs. A pipeline worker thread of the master coordinates the two instances. This measures the throughput of the GPU in production-like conditions, which is better than the inverse of the wall time of a single TF. Not used for `async` (no such overlap in async processing), `alldetectors`, and `kernel` timing (`--debug 1` synchronizes the GPU after each kernel, so there is no overlap, and the per-kernel times would be split between the instances). Details of the mechanism: the double-pipeline code in `GPUReconstruction` (`EnqueuePipeline`, `RunPipelineWorker`).
- `--preloadEvents`: load all TFs into host memory before the processing, needed for the double pipeline to cycle through different TFs.

What is measured:
- `wall` without double pipeline: `Total Wall Time` is the wall time of `GPUReconstruction::RunChains()`, i.e. of the complete processing of one TF: transfer of the input to the GPU, all reconstruction steps, transfer of the output to the host. Not included: loading the TF from disk (`Loading time`), the initialization (incl. RTC compilation and memory allocation), and the QA. It is averaged over all iterations except the first (`--runsInit`), which includes one-time overheads (e.g. first kernel launches, page faults).
- `wall` with double pipeline: `Pipeline wall time` is the elapsed time from the start of the 3rd iteration until the end of the last one, divided by the number of these iterations (`runs - 2`). The first 2 iterations (one per instance) run one after the other as warm-up. This is the average time per TF at full pipeline throughput. The `Total Wall Time` of the single TFs is not printed (they overlap).
- `kernel`: `--debug 1` synchronizes after every GPU kernel and measures each kernel with GPU timers (`deviceTimers`) (on the CPU: each task with a host timer), and the time of the DMA transfers per reconstruction step. The output lists per kernel the number of calls and the time per TF, per step the summed kernel time (`Tasks`), the total time and the CPU time, and the DMA transfers with their bandwidth. `Total Kernel Time` is the sum of all kernel times per TF, i.e. the pure GPU compute time without the gaps between kernels, the transfers and the host work. The `Total Wall Time` in `kernel` mode is larger than in `wall` mode due to the synchronization and the timer overhead and should not be used as wall time.
- `--tfTime` line: `<Sync phase|...>: X sec per 32 orbit TF, Y GPUs required (Measured wall time - Extrapolated from N clusters to M)`. The measured time (pipeline time per TF with double pipeline, otherwise `Total Wall Time`) is scaled linearly with the number of TPC clusters from the `N` clusters of the TF to the `M` = 107,546,607 clusters of a 32-orbit TF of Pb-Pb collisions at 50 kHz (755,851 clusters per collision, 11245 Hz orbit frequency). `Y` = time per TF / duration of the TF (32 / 11245 s) is the number of GPUs (or CPUs) needed to process 50 kHz Pb-Pb in real time.

# 5. Further information

## Datasets

- For meaningful numbers, use large Pb-Pb datasets with real TF size, e.g. 32-orbit TFs at 50 kHz. Small datasets (pp, few collisions) are dominated by fixed overheads (kernel launches, synchronization), and their extrapolation with `--tfTime` is not meaningful.
- `sync`, `combined`, `alldetectors` need TPC ZS raw data; datasets with digits or clusters are rejected. `async` needs compressed clusters, e.g. created from a sync dataset with `--syncAsync --dumpEvents 2` (see [build-standalone.md](https://github.com/AliceO2Group/AliceO2/blob/dev/GPU/documentation/build-standalone.md)).
- `--runs N` repeats the same TF, so caches and memory are warm; `-n N` processes different TFs, which also covers the variation between TFs. With double pipeline, `--runs` processes the same TF in both instances.

## Build

- Use an optimized build without debug settings (`GPUCA_BUILD_DEBUG 0` in `config.cmake`) and without deterministic mode (`GPUCA_DETERMINISTIC_MODE OFF`), the deterministic mode is much slower.
- Build only for the GPU architecture of the benchmark (`CUDA_COMPUTETARGET` / `HIP_AMDGPUTARGET`); with RTC, the code is compiled again for the actual GPU anyway.

## Further output options

- `--PROCdebugCSV <file>` appends the timing table of `kernel` mode to a CSV file, `--PROCdebugMarkdown 1` prints it as markdown table.
- `--PROCrecoTaskTiming 1` prints the time per reconstruction step also in `wall` mode (enabled automatically with `--debug 1`).
- `--memoryStat` prints memory statistics; `Maximum Memory Allocation: Host ... / Device ...` at the end shows the peak use of the memory pools, which helps to choose `--memSize`.

## Memory

- The GPU must have at least `--memSize` of free memory, plus the memory of the runtime and, with RTC, of the loaded kernels.
- Host memory: the input and output buffers (6 + 10 GB, 10 GB more for the second output buffer with double pipeline), the page-locked host memory pool of the reconstruction (1 GB per instance, 2 GB if the compression output is gathered on the host), and the loaded TFs (all of them with `--preloadEvents`; a 50 kHz TF with ZS raw data has about 2 GB).
- CPUDevice (the CPU kernels run as if on a GPU device, for testing without a GPU) needs the same memory in host memory, and the CPU kernel parameters.

## Notes

- The `--tfTime` line is labeled `Sync phase` also for the other processing modes.
- For `async`, the cluster count `N` is the number of clusters after the cluster rejection of the sync phase, so the extrapolation refers to a TF with 107,546,607 clusters after rejection, i.e. more data than a real 50 kHz TF.
- With `-n` and double pipeline, the extrapolation uses the cluster count of the last TF, while the time is the average over all TFs.
- The double pipeline does not support TRD tracking (initialization fails), therefore `sync` and `combined` disable TRD.
- Without a GPU, the `gpu` benchmarks can be tested with `--gpuType CPUDEVICE` (functionally, the timing is not representative of a GPU).
