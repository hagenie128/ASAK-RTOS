# ASAK-RTOS README 동기화 기록

## 요청 범위

- README에 WSL 설정, 파일·코드 위치별 역할, 현재 검증 범위와 미확정 경계를 추가했다.
- 관리자 OrderDetailPanel.jsx의 영수증 상세 표시 형식을 참조했다.
- C/FreeRTOS/Spring Boot 소스와 Git 원격은 수정하지 않았다.

## 확인 근거

- CMakeLists.txt: GCC_POSIX, heap_4, FreeRTOS Kernel 탐색, pthreads 링크
- Makefile: make, make run, make clean, SERVER_URL
- src/main.c: pending polling, 최소 JSON 문자열 파싱, PRINT_RECEIPT, finish PATCH
- src/http_client.[ch]: POSIX TCP HTTP 요청·응답
- config/FreeRTOSConfig.h: 동적 할당과 POSIX 시뮬레이터 설정
- ASAK-Admin/src/components/admin/orders/OrderDetailPanel.jsx: 주문 상세와 영수증 출력 UI

## 확인 및 미검증

- 빌드 성공, RTOS WorkerTask 생성, 실패 결과 보고가 관찰되었다.
- COMPLETED 전체 흐름, 실제 프린터, QEMU/ARM 실행은 미검증이다.
- Interrupted system call의 HTTP 재시도 정책은 미확정이다.
- 관리자 화면의 옵션·제외·요청사항은 현재 RTOS payload에 포함되지 않는다.
