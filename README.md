# tee-dist

`tee-dist`는 CN(client node)이 대부분의 데이터 경로를 수행하고, MN(memory node)은 shared-memory region을 노출하는 client-centric distributed KVS다. 현재 구현은 persistent TCP/RDMA session, `PRIME -> CACHE -> BACKUP` slot layout, CN-side encryption/MAC verification, `write/update/delete`용 SNAPSHOT-style commit rule, 그리고 CN single-thread benchmark runner를 포함한다.

## 현재 구현 기준 핵심 요약

- CN은 시작 시 `mn_endpoint`마다 세션을 한 번 만들고 `quit`까지 재사용한다.
- CN이 key hash 계산, probe, cache validation, AES-CTR encrypt/decrypt, HMAC-SHA256 생성/검증, replication vote 평가, repair를 수행한다.
- MN은 slot payload를 해석하지 않고 region, CAS/control, eviction만 담당한다.
- slot은 `guard_epoch | visible_epoch | key_hash | tie_breaker | flags | value_len | iv | mac | ciphertext` 형식이다.
- `cache: on`이면 read 시 `CACHE`를 먼저 검증하고, miss면 `PRIME`에서 읽은 뒤 best-effort cache refresh를 수행한다.
- RDMA 경로는 mixed model이다. slot body read/write는 one-sided RDMA READ/WRITE를 사용하고, `HELLO`/`CAS`/`EVICT`/`CLOSE`는 SEND/RECV control message로 처리한다.

## 컴포넌트 역할

- `CN`
  - persistent session 유지
  - primary/backup 선택
  - slot probe와 cache validation
  - encrypt, decrypt, MAC verify
  - SNAPSHOT consensus rule 평가
  - backup repair, cache refresh
- `MN`
  - region open 및 transport 노출
  - TCP worker thread 또는 RDMA connection thread에서 CAS/control 처리
  - cache eviction thread 유지
  - plaintext 해석 없음

## 요청 경로

### `read`

1. CN이 key hash를 계산하고 primary MN의 `PRIME` 영역을 probe한다.
2. `cache: on`이면 같은 primary MN의 `CACHE` 영역도 probe한다.
3. cache slot이 prime slot과 같은 epoch를 가리키고 valid하면 CN이 cache slot을 로컬에서 MAC verify + decrypt 한다.
4. cache miss이면 CN이 `PRIME` slot을 로컬에서 MAC verify + decrypt 한다.
5. prime hit 후에는 CN이 primary `CACHE`에 best-effort refresh를 수행한다.
6. tombstone 또는 empty slot이면 `not_found`를 반환한다.

### `write`

1. CN이 primary `PRIME` slot을 probe해 현재 epoch와 candidate slot을 잡는다.
2. CN이 새 slot을 로컬에서 생성한다.
   이 단계에 key hash, tie-breaker, IV 생성, AES-256-CTR encrypt, HMAC-SHA256이 포함된다.
3. replication이 2 이상이면 CN이 각 backup MN의 `BACKUP` 영역에 proposal body를 쓰고 CAS로 epoch commit을 시도한다.
4. CN이 vote를 모아 SNAPSHOT rule을 평가한다.
   `rule=1`은 모든 backup 성공, `rule=2`는 과반 성공, `rule=3`은 tie-breaker 우세, `rule=0`은 실패다.
5. rule이 성립하면 CN이 primary `PRIME`에 body write 후 CAS commit을 수행한다.
6. backup 실패분은 best-effort repair를 시도하고, 마지막에 primary `CACHE`도 best-effort refresh 한다.

### `update`

- `write`와 동일한 경로를 타지만, primary probe 결과가 missing 또는 tombstone이면 실패한다.

### `delete`

- `write`와 동일한 경로를 타되 value 길이 0의 tombstone slot을 만든다.
- 이후 `read`는 tombstone을 `not_found`로 처리한다.

## workload와 CPU 자원 사용

| workload | CN CPU | MN CPU over TCP | MN CPU over RDMA |
| --- | --- | --- | --- |
| `read` | key hash, prime/cache probe loop, cache validation, MAC verify, AES decrypt, cache refresh 결정 | worker thread가 request recv/send, read buffer alloc, `td_region_read_bytes()` lock+`memcpy()` 수행 | 데이터 fetch 자체는 one-sided RDMA READ라 server CPU를 거의 쓰지 않는다. 다만 cache refresh의 CAS는 control message로 들어와 MN thread가 처리한다. |
| `write` | key hash, slot 생성, encrypt, MAC, backup probe, consensus eval, primary commit, repair, cache refresh | backup/primary 각각에 대해 read/write/CAS가 worker thread에서 socket syscall과 region lock을 사용한다 | slot body write는 one-sided RDMA WRITE라 MN CPU를 거의 쓰지 않지만, backup commit/primary commit/repair/cache refresh에 필요한 모든 CAS는 MN thread가 처리한다. |
| `update` | `write`와 동일 + primary 존재 여부 검사 | `write`와 동일 | `write`와 동일 |
| `delete` | `write`와 동일한 probe/replication/CAS 경로, 단 encrypt/decrypt payload는 0-byte tombstone 기준 | `write`와 동일 | `write`와 동일 |

### CPU 해석 포인트

- CN이 항상 CPU hot path의 중심이다.
  probe scan, crypto, vote evaluation, repair, cache refresh 판단은 모두 CN에서 실행된다.
- TCP는 MN CPU 사용량이 크다.
  read/write/CAS마다 MN worker thread가 socket recv/send, buffer allocation, `pthread_mutex` lock, `memcpy()`를 수행한다.
- RDMA는 raw data path를 MN CPU에서 밀어낸다.
  read/write body 이동은 one-sided RDMA라 MN application thread가 관여하지 않는다.
- 대신 RDMA는 polling CPU를 CN과 MN connection thread에 남긴다.
  client와 server 모두 `ibv_poll_cq()` 루프를 돌고, completion이 없으면 `sched_yield()` backoff를 수행한다.
- cache가 켜져 있으면 read당 CN CPU가 늘어난다.
  cache probe, cache validation, cache miss 후 refresh commit이 추가되기 때문이다.
- replication이 커질수록 CN CPU와 transport 작업량이 거의 선형으로 증가한다.
  특히 `write/update/delete`는 backup 수만큼 probe, body write, CAS, repair 가능성이 추가된다.
- `delete`는 payload crypto 비용은 작지만, probe/CAS/replication 비용은 `write`와 거의 동일하다.

### background CPU

- 모든 MN은 eviction thread를 하나 띄운다.
  `mn_main`에서 100ms마다 `td_region_evict_if_needed()`를 호출한다.
- eviction threshold 기본값은 80%다.
  threshold를 넘기면 cache usage를 다시 세고 valid cache slot의 약 25%를 지운다.
- TCP listener는 `select()` 기반 accept loop를 사용하고 connection마다 detached thread를 만든다.
- RDMA listener는 CM event channel에 `poll()`을 걸고 connection마다 detached thread를 만든다.

## latency breakdown 해석

REPL에서 `-t`를 붙이면 단계별 latency breakdown을 출력한다.

- `workload_*`
  CN이 수행한 logical phase 시간이다.
  예: `workload_prime_probe`, `workload_snapshot_primary_cas`, `workload_cache_refresh_write`
- `crypto_*`
  CN의 hash, IV, encrypt, decrypt, MAC setup/body/finalize 시간이다.
- `tcp_*`, `rdma_*`
  transport send/wait/copy/poll/backoff 시간이다.
- `tcp_server_*`, `rdma_server_*`
  MN이 control/read/write/CAS를 처리하는 raw server-side 시간이다.
- `probe_*`
  slot scan 수, tombstone 수, empty hit 수, guard mismatch 수를 보여준다.
- `rdma_empty_polls`, `rdma_backoffs`
  CQ polling이 CPU를 얼마나 소비하는지 보는 데 유용하다.

## 빌드

```bash
make -C tee-dist
```

## 현재 체크인된 config preset

- `mn_memory_size`
  `64MB`, `16GB`처럼 단위를 붙여 입력한다.
- MN config에서 `prime_slots`, `cache_slots`, `backup_slots`를 생략하면
  header를 제외한 나머지 공간을 4:1:4 비율로 자동 배분한다.
- 일부 slot만 수기로 적으면 그 항목은 고정하고, 남은 공간만 나머지 항목에 비율대로 채운다.
- 수기 slot 합계가 `mn_memory_size` 안에 들어가지 않으면 MN은 부팅 전에 config error로 실패한다.
- 현재 체크인된 MN preset들은 slot 수를 직접 적지 않고 auto allocation을 사용한다.

- `build/config/mn1.conf`, `mn2.conf`, `mn3.conf`
  `127.0.0.1` 기반 3-node TCP MN preset
- `build/config/cn.cache-off.conf`
  위 3-node TCP preset과 맞는 CN preset
- `build/config/mn1.rdma.conf`, `mn2.rdma.conf`, `mn3.rdma.conf`
  `127.0.0.1` 기반 3-node RDMA MN preset
- `build/config/mn.bench.conf`, `cn.bench.conf`
  `10.20.26.87:7301` 기준 single-node TCP benchmark preset
- `build/config/mn.conf`, `cn.conf`
  single-node TCP 예시 preset이지만 `listen_host`와 `mn_endpoint`를 실제 환경에 맞춰 확인해야 한다.
- `build/config/mn.rdma.conf`, `cn.rdma.conf`
  single-node RDMA preset

현재 저장소에는 3-node `cache:on` CN preset과 3-node RDMA CN preset이 따로 체크인되어 있지 않다. 필요하면 `cn.cache-off.conf` 또는 `cn.rdma.conf`를 복사해 endpoint와 `cache` 값을 맞춰 쓰면 된다.

## 실행 예시

### 3-node TCP replication

먼저 MN 3개를 띄운다.

```bash
./bin/mn --config build/config/mn1.conf
./bin/mn --config build/config/mn2.conf
./bin/mn --config build/config/mn3.conf
```

그 다음 CN REPL을 실행한다.

```bash
./bin/cn --config build/config/cn.cache-off.conf
```

### single-node RDMA

```bash
./bin/mn --config build/config/mn.rdma.conf
./bin/cn --config build/config/cn.rdma.conf
```

## REPL 명령

- `read <key>`
- `write <key> <value>`
- `update <key> <value>`
- `delete <key>`
- `read <key> -t`
- `write <key> <value> -t`
- `update <key> <value> -t`
- `delete <key> -t`
- `status`
- `evict`
- `help`
- `quit`

`status`는 transport, replication, cache mode, session 수와 각 MN의 `prime/cache/backup` slot 수를 출력한다. `evict`는 모든 MN에 즉시 eviction control message를 보낸다.

## benchmark

`cn_bench`는 CN 한 개에서 workload를 순차적으로 반복 실행하는 single-thread runner다. 세션은 benchmark 시작 시 한 번 연결되고, iteration마다 다시 연결하지 않는다.

```bash
./bin/cn_bench --config build/config/cn.bench.conf --workload read --iterations 1000 --bytes 32 --warmup 128
./bin/cn_bench --config build/config/cn.bench.conf --workload write --iterations 1000 --bytes 32 --warmup 128
./bin/cn_bench --config build/config/cn.bench.conf --workload update --iterations 1000 --bytes 32 --warmup 128
./bin/cn_bench --config build/config/cn.bench.conf --workload delete --iterations 1000 --bytes 32 --warmup 128
```

- `write`
  warm-up 후 새 key에 대해 측정한다.
- `read`
  측정 전 `iterations + warmup` 개 key를 seed 한 뒤 read를 측정한다.
- `update`
  측정 전 seed 후 update를 측정한다.
- `delete`
  측정 전 seed 후 delete를 측정하고, warm-up에서는 delete 뒤 다시 write 해서 key를 복구한다.

출력 통계는 `min`, `max`, `p50(t_typical)`, `avg`, `stdev`, `p99`, `p99.9`다.

## 소스 맵

- `src/cn_main.c`: CN REPL entrypoint
- `src/mn_main.c`: MN entrypoint와 eviction thread 시작점
- `src/cluster.c`: read/write/update/delete, cache refresh, repair, latency breakdown
- `src/crypto.c`: AES-256-CTR + HMAC-SHA256
- `src/transport_tcp.c`: persistent TCP session과 MN worker server
- `src/transport_rdma.c`: one-sided RDMA READ/WRITE + control SEND/RECV
- `src/layout.c`: region layout, mutex-protected local access, eviction
- `benchmark/cn_bench.c`: benchmark runner
