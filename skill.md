# SKILL: tee-dist Current Runtime

이 문서는 현재 `tee-dist` 코드베이스를 수정할 때 따라야 하는 실제 런타임 모델을 정리한다. 예전의 목표 상태가 아니라, 지금 저장소에 구현되어 있는 동작을 기준으로 한다.

## 언제 이 문서를 사용할까

- `tee-dist`의 CN/MN 실행 경로를 수정할 때
- `read/write/update/delete` semantics를 바꿀 때
- TCP/RDMA transport를 수정할 때
- benchmark, latency profile, config preset, README를 함께 맞춰야 할 때

## 현재 아키텍처

- `src/cn_main.c`
  CN 프로세스 entrypoint다. `td_cluster_init()`로 `mn_endpoint` 전부에 연결한 뒤 REPL을 유지한다.
- `src/mn_main.c`
  MN 프로세스 entrypoint다. shared region을 열고 eviction thread를 시작한 뒤 TCP 또는 RDMA server를 돈다.
- `src/cluster.c`
  placement, probe, cache validation, cache refresh, SNAPSHOT vote 평가, repair, REPL command, latency breakdown을 담당한다.
- `src/crypto.c`
  AES-256-CTR encrypt/decrypt와 HMAC-SHA256을 담당한다.
- `src/transport_tcp.c`
  persistent TCP session과 MN side worker-thread server를 담당한다.
- `src/transport_rdma.c`
  one-sided RDMA READ/WRITE와 SEND/RECV control path를 담당한다.
- `src/layout.c`
  `PRIME -> CACHE -> BACKUP` region layout, local read/write/CAS, cache usage count, eviction을 담당한다.
- `benchmark/cn_bench.c`
  single-thread benchmark runner다.

## ownership 원칙

- CN이 hot path를 가진다.
  hash, probe, cache validation, encrypt/decrypt, MAC verify, vote evaluation, repair는 CN 책임이다.
- MN은 memory appliance처럼 동작한다.
  plaintext 해석, vote 판단, value-level business logic는 하지 않는다.
- RDMA 경로는 pure control-plane server가 아니다.
  slot body read/write는 one-sided RDMA고, `HELLO`/`CAS`/`EVICT`/`CLOSE`만 control message로 처리한다.
- TCP 경로는 payload copy까지 MN CPU가 담당한다.

## 현재 request semantics

### `read`

1. primary `PRIME` probe
2. `cache:on`이면 primary `CACHE` probe
3. cache가 prime과 같은 epoch를 가리키고 valid면 cache decode
4. cache miss면 prime decode
5. prime hit 후 best-effort cache refresh
6. tombstone 또는 empty면 `not_found`

### `write`

1. primary `PRIME` probe로 candidate slot과 current epoch 확인
2. CN에서 proposal slot 생성
3. 각 backup MN `BACKUP`에 body write + CAS commit 시도
4. vote 평가
   `rule=1` all success
   `rule=2` majority success
   `rule=3` tie-breaker win
   `rule=0` fail
5. primary `PRIME` commit
6. 실패한 backup best-effort repair
7. primary `CACHE` best-effort refresh

### `update`

- `write`와 같지만 primary probe 결과가 missing 또는 tombstone이면 실패한다.

### `delete`

- `write`와 같지만 0-byte tombstone slot을 쓴다.

## CPU 자원 관점에서 꼭 기억할 점

- `read`는 CN에서 probe와 decrypt/MAC verify가 핵심 CPU를 쓴다.
- `write/update/delete`는 CN에서 encrypt, MAC, vote 평가, repair가 핵심 CPU를 쓴다.
- TCP에서는 MN worker thread가 read/write/CAS마다 socket syscall, allocation, `pthread_mutex` lock, `memcpy()`를 수행한다.
- RDMA에서는 raw data read/write는 MN CPU를 거의 쓰지 않지만, 모든 CAS와 control response는 MN thread가 처리한다.
- RDMA client/server 모두 CQ polling loop와 `sched_yield()` backoff를 사용하므로 polling CPU가 생긴다.
- eviction thread는 100ms마다 cache 상태를 검사하고 threshold 초과 시 cache의 약 25%를 정리한다.

## 수정할 때 지켜야 할 규칙

- request 단계가 바뀌면 `td_latency_profile_t`와 `td_print_latency_profile()`도 같이 갱신한다.
- 새 transport 동작을 추가하면 CN 측 시간과 MN 측 raw timing이 모두 관측 가능해야 한다.
- RDMA 변경 시 one-sided body I/O와 control-message CAS 경계를 흐리지 말아야 한다.
- `read/write/update/delete` semantics를 바꾸면 `README.md`도 같은 턴에 갱신한다.
- benchmark가 single-thread라는 사실이 바뀌지 않았다면 문서에서 throughput saturator처럼 설명하지 않는다.
- config preset을 추가하거나 바꾸면 어떤 조합이 실제로 서로 맞는지 README에 명시한다.

## 현재 체크인된 preset을 볼 때 주의할 점

- `mn_memory_size`는 `64MB`, `16GB`처럼 단위가 붙은 값으로 넣는다.
- MN config에서 slot 항목을 빼면 남은 용량 안에서 4:1:4 비율로 자동 배분된다.
- 일부 slot만 명시하면 그 값은 고정하고, 나머지 slot만 자동 배분된다.
- 수기 slot 값이 `mn_memory_size`를 넘기면 config load 단계에서 실패한다.
- 3-node TCP는 `mn1.conf/mn2.conf/mn3.conf`와 `cn.cache-off.conf`가 바로 맞는다.
- 3-node `cache:on` CN preset은 따로 없다.
- single-node benchmark preset은 `10.20.26.87:7301` 기준이다.
- 3-node RDMA MN preset은 있지만, matching 3-node RDMA CN preset은 따로 체크인되어 있지 않다.

## 문서와 코드가 어긋나기 쉬운 파일

- `src/cluster.c`
- `src/transport_tcp.c`
- `src/transport_rdma.c`
- `src/layout.c`
- `benchmark/cn_bench.c`
- `build/config/*.conf`
- `README.md`
