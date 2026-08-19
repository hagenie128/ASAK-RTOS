# ASAK-RTOS

ASAK 키오스크의 FreeRTOS 장치 명령 클라이언트용 독립 저장소입니다.

## 현재 상태

- WSL Ubuntu 네이티브 경로에서 Git 저장소만 초기화했습니다.
- C/FreeRTOS 소스, CMake/Makefile, 실제 보드 설정은 아직 추가하지 않았습니다.
- Spring Boot API 기본틀은 `ASAK-back`의 `common/device`에 있습니다.

## 예정 역할

```text
src/     FreeRTOS Task, HTTP client, PRINT_RECEIPT Handler
config/  FreeRTOSConfig.h, 보드별 설정
docs/    빌드 및 장비 연결 안내
```

RTOS는 Spring Boot의 `GET /api/rtos/device-events/pending`을 polling하고,
처리 후 `PATCH /api/rtos/device-events/{eventId}/finish`로 결과를 보고합니다.

ASAK API는 공통 envelope를 사용하므로 FreeRTOS parser는 응답의 `data`에서
`eventId`, `eventType`, `payload`, `status`를 읽어야 합니다.

커밋, 원격 저장소 연결, push는 하지 않았습니다.
