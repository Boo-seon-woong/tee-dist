# tee-dist

`tee-dist/skill.md`를 기준으로 새로 만든 client-centric distributed KVS다. `CN`은 지속형 세션과 REPL을 제공하고, `MN`은 shared-memory region을 RDMA/TCP로 노출하는 memory appliance처럼 동작한다.

## 특징

- `CN` 중심 로직: 키 배치, 암복호, MAC 검증, SNAPSHOT 규칙 평가
- `MN` 최소 역할: shared-memory region 유지, RDMA/TCP 접근 노출, cache eviction
- shared-memory layout: `Prime Cache -> Cache -> Backup Slots`
- 데이터 형식: `[key_hash | ciphertext | MAC]`
- transport: `tcp`, `rdma`
- 지속형 세션: `CN` 실행 중 연결 재사용
- cache `on/off` 실험 가능

## 빌드

```bash
make -C tee-dist
```

## 실행 예시

MN 3개를 먼저 띄운다.

```bash
./bin/mn --config build/config/mn1.conf
./bin/mn --config build/config/mn2.conf
./bin/mn --config build/config/mn3.conf
```

그 다음 CN REPL을 실행한다.

```bash
./bin/cn --config build/config/cn.conf
```

CN 벤치마크는 아래처럼 실행한다.

```bash
./bin/cn_bench --config build/config/cn.conf --workload read --iterations 100 --bytes 32 --warmup 16
```

REPL 명령:

- `read key`
- `write key value`
- `update key value`
- `delete key`
- `read key -t`, `write key value -t`, `update key value -t`, `delete key -t`로 단계별 latency breakdown 출력
  - transport별 세부 항목: `tcp_*` 또는 `rdma_*`
  - workload 세부 항목: `workload_snapshot_*`, `workload_cache_*`, `workload_prime_*`
  - probe 통계: `probe_reads`, `probe_slots`, `probe_guard_mismatch`, `probe_tombstones`, `probe_empty`
  - server-side raw timing: `tcp_server_*`, `rdma_server_*`
- `status`
- `evict`
- `quit`

## 구조

- `include/`: 공통 헤더
- `src/cluster.c`: CN REPL, deterministic placement, SNAPSHOT write path
- `src/transport_tcp.c`: persistent TCP session 및 MN 제어 서버
- `src/transport_rdma.c`: `rdma_cm` + verbs 기반 persistent RDMA session
- `src/test/`: smoke / cache 비교 스크립트
- `benchmark/`: CN-side workload benchmark 안내와 소스
