
# SKILL: Client-Centric Distributed KVS with RDMA/TCP and Intel TDX

------------------------------------------------------------
1. System Goal

Implement a client-centric distributed key-value store supporting RDMA and TCP transport,
designed to run memory nodes (MN) inside Intel TDX.

The system must prioritize:

1. Client-centric execution
2. Minimal MN CPU usage
3. Shared-memory-only TEE operation
4. MAC-based integrity protection
5. RDMA-based communication with persistent sessions
6. Experimental comparison of cache ON vs OFF
7. SNAPSHOT replication consensus

The system is designed primarily for research experimentation.


------------------------------------------------------------
2. Node Roles

Two types of nodes exist.

CN (Client Node)

Responsible for:

- request execution
- consensus coordination
- encryption / decryption
- RDMA communication
- CAS operations
- deterministic MN selection
- MAC verification

Almost all logic must execute on CN.


MN (Memory Node)

Runs inside Intel TDX or a normal VM.

Responsibilities:

- expose shared memory region
- store ciphertext KV entries
- provide RDMA-accessible memory
- maintain cache / prime cache structures
- perform eviction when cache becomes full

MN must NOT interpret data.

MN acts as a memory appliance.

MN CPU activity must be minimal.


------------------------------------------------------------
3. Memory Model

The system operates using shared memory only when running in TDX.

There is NO private memory data store.

All KV data resides inside shared memory accessible through RDMA.

Example shared memory layout:

|---------------------------------|
| Prime Cache Table               |
|---------------------------------|
| Cache Table                     |
|---------------------------------|
| Backup Slots                    |
|---------------------------------|
| RDMA Communication Buffers      |
|---------------------------------|

Both cache and prime cache are stored in shared memory.


------------------------------------------------------------
4. Data Protection Model

All values stored in MN are encrypted and authenticated.

Data structure:

ciphertext = ENC(key, value)

tag = MAC(key, ciphertext)

Stored format:

[key_hash | ciphertext | MAC]

MN only stores ciphertext.

MN never decrypts data.

Decryption and MAC verification occur only at CN.

If MAC verification fails:

- treat as corruption
- retry or fail the request


------------------------------------------------------------
5. Transport Layer

Transport is configurable:

transport: rdma | tcp


------------------------------------------------------------
6. RDMA Communication Model

IMPORTANT REQUIREMENT:

This project must NOT implement a purely one-sided RDMA system.

Instead, RDMA communication must follow the perftest-style RDMA communication model.

The reference implementation must follow the structure used in:

./2026/perftest

(relative to the working directory)

That project contains examples for:

- RDMA connection setup
- queue pair initialization
- completion queue handling
- send/recv communication
- RDMA latency testing

Your RDMA implementation must follow the same general architecture.

Typical RDMA operations may include:

- send / recv messaging
- RDMA read
- RDMA compare-and-swap

However the communication model must follow the perftest-style connection and messaging pattern.


------------------------------------------------------------
7. Persistent RDMA Session Requirement

RDMA connections must remain alive across multiple client operations.

DO NOT create or destroy RDMA connections per request.

Instead:

1. The client process starts.
2. RDMA connections to MN nodes are established.
3. The client process keeps those connections alive.
4. Multiple commands reuse the same RDMA sessions.


------------------------------------------------------------
8. Interactive Client Process

The CN process must remain running until an explicit termination command is given.

While the CN process is running:

- RDMA connections remain active
- the user can continue entering commands

Example behavior:

Client process starts:

./cn --config build/config/client.json

Client terminal accepts commands:

read key1

write key1 value1

update key1 value2

delete key1

read key1

...

RDMA connections must NOT be recreated for each command.

The client process acts like an interactive REPL that continuously accepts commands and executes them using existing RDMA sessions.

Client terminates only when:

exit

or

quit

command is entered.


------------------------------------------------------------
9. Configuration

Configuration files are located in:

build/config/

Example parameters:

mode: cn | mn

transport: rdma | tcp

replication: integer

tdx: on | off

cache: on | off

mn_memory_size_mb: integer

encryption_key_hex: hexstring

rdma_device: ibp111s0

rdma_gid_index: integer


Example configuration:

mode: cn
transport: rdma
replication: 3
tdx: on
cache: on
mn_memory_size_mb: 4096
encryption_key_hex: "001122334455"
rdma_device: ibp111s0
rdma_gid_index: 0


------------------------------------------------------------
10. Deterministic MN Selection

Key placement must be deterministic.

Example rule:

mn_index = hash(key) mod number_of_mn_nodes

CN computes which MN should contain the key.

No centralized metadata service exists.


------------------------------------------------------------
11. Cache Model

Two structures exist:

Prime Cache
Cache


Prime Cache

Prime cache stores expected values used during CAS operations.

Prime cache acts as the authoritative expected-value table.


Cache

Cache stores recently accessed KV entries.

Cache behaves similarly to backup slots.

Cache entries may be overwritten when needed.


------------------------------------------------------------
12. Cache Modes

Cache behavior must be configurable.

config option:

cache: on | off


Cache ON

Read path:

1 RDMA read prime cache
2 RDMA read cache entry
3 verify MAC
4 return value


Cache OFF

Cache structures still exist but reads behave as cache misses.

Purpose:

enable experiments comparing cache-enabled vs no-cache latency behavior.


------------------------------------------------------------
13. Lazy Cache Writeback Policy

Cache updates must NOT trigger immediate MN CPU work.

Goal:

minimize MN CPU usage.

Write behavior:

CN performs writes using RDMA operations.

MN performs no work during normal writes.

MN performs work only when cache becomes full.


Lazy Writeback:

if cache_usage < threshold:
    do nothing

if cache_usage >= threshold:
    perform eviction


Eviction procedure:

1 select victim cache entry
2 move entry to backup slot or cold region
3 free cache slot


------------------------------------------------------------
14. Replication Model

Replication factor:

replication = N

Each key has:

- one primary slot
- N-1 backup slots

All stored inside shared memory.


------------------------------------------------------------
15. SNAPSHOT Replication Protocol

READ(slot):

v = RDMA_READ_primary(slot)

if v == FAIL:
    handle failure

return v


WRITE(slot, vnew):

vold = RDMA_READ_primary(slot)

v_list = RDMA_CAS_backups(slot, vold, vnew)

v_list = change_list_value(v_list, vold, vnew)

win = EVALUATE_RULES(v_list)

if win == Rule_1:

    RDMA_CAS_primary(slot, vold, vnew)

elif win == Rule_2 or win == Rule_3:

    RDMA_CAS_backups(slot, v_list, vnew)

    RDMA_CAS_primary(slot, vold, vnew)

elif win == LOSE:

    repeat:

        sleep small interval

        vcheck = RDMA_READ_primary(slot)

    until vcheck != vold


------------------------------------------------------------
16. Consensus Rules

Rule 1

Client modifying ALL backup slots becomes the last writer.


Rule 2

Client modifying MAJORITY of backup slots becomes the last writer.


Rule 3

If neither rule determines a winner,

the client writing the minimal value becomes the last writer.


------------------------------------------------------------
17. Testing Framework

Test scripts located in:

src/test/

Test scripts should:

1 launch MN nodes
2 launch CN nodes
3 run workloads
4 measure latency and throughput
5 compare cache ON vs OFF behavior


------------------------------------------------------------
18. Implementation Principles

Important principles:

- client-centric architecture
- minimal MN CPU involvement
- persistent RDMA sessions
- interactive client command loop
- shared-memory-only TDX operation
- MAC-based integrity protection
- configurable system behavior
