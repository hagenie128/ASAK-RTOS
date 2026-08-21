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

## 처음 설정하기 (WSL Ubuntu)

이 저장소는 Windows 경로(`/mnt/c/...`)가 아니라 WSL Ubuntu의 홈 경로에서 작업합니다.

### 필요한 도구와 GitHub 인증

```bash
sudo apt update
sudo apt install git gh -y

gh auth login
gh auth setup-git
gh auth status
```

로그인 과정에서는 `GitHub.com` → `HTTPS` → `Login with a web browser`를 선택합니다.

### 저장소 열기

처음 받는 팀원:

```bash
cd ~
git clone https://github.com/hagenie128/ASAK-RTOS.git
cd ASAK-RTOS
code .
```

이미 저장소가 있는 경우:

```bash
cd ~/ASAK-RTOS
git pull --ff-only origin main
code .
```

### 변경 반영 순서

```bash
git status --short
git add <승인된-파일만>
git diff --cached --check
git commit -m "chore: RTOS 저장소 기본 구조 추가"
git push -u origin main
```

- `git add .` 대신 승인된 파일만 명시합니다.
- commit과 push는 팀 승인 후에만 실행합니다.
- 토큰, `.env`, 보드별 비밀값은 커밋하지 않습니다.

## 최신 상태 (2026-08-19)

> **시연용 초안 / 미확정 코드**: 이 저장소는 WSL Linux의 FreeRTOS GCC_POSIX 포트로 실행하는 실험용 클라이언트입니다. 실제 프린터, ARM 보드, QEMU 펌웨어, 장치 인증, DB 영속화는 아직 구현·검증하지 않았습니다. 이 섹션이 위 초기 기록보다 최신입니다.

| 항목 | 현재 확인 | 미확정 또는 미구현 |
| --- | --- | --- |
| make 빌드 | asak_rtos 링크 성공 | 팀원 환경 재현 필요 |
| Spring polling | pending 조회와 WorkerTask 생성 관찰 | COMPLETED 전체 흐름 재검증 필요 |
| 영수증 | 콘솔 ASCII 출력 | 실제 프린터 드라이버 없음 |
| QEMU/ARM | 없음 | 별도 qemu-first-sample 단계 |
| 통신 오류 | connect EINTR 재시도 반영 | timeout/재시도 정책은 추가 확정 필요 |

## 현재 데이터 흐름

~~~text
Kiosk/Admin 또는 PowerShell
  -> POST /api/kiosk/orders/{orderId}/receipt-print
     eventType=PRINT_RECEIPT
     payload=주문번호|주문상세|결제금액
  -> Spring Boot GET /api/rtos/device-events/pending
  -> CommandPollTask -> WorkerTask -> 콘솔 영수증
  -> PATCH /api/rtos/device-events/{eventId}/finish
~~~

현재 payload는 JSON 안의 단순 문자열입니다.

~~~text
ORDER-1225|아메리카노 x 1, 카페라떼 x 2|10500
~~~

Handler는 | 로 분리된 주문번호, 메뉴 요약, 금액 세 값만 처리합니다. 중첩 JSON payload, 특수문자 escape, 옵션/제외/요청사항은 아직 처리하지 않습니다.

## WSL 빌드 준비

~~~bash
sudo apt update
sudo apt install -y build-essential cmake git gh
cmake --version
make --version
git clone --depth 1 https://github.com/FreeRTOS/FreeRTOS-Kernel.git ~/FreeRTOS-Kernel
export FREERTOS_KERNEL_PATH=~/FreeRTOS-Kernel
cd ~/ASAK-RTOS
make
~~~

CMakeLists.txt는 FREERTOS_KERNEL_PATH, ~/rtos-kiosk-course/third_party/FreeRTOS-Kernel, ~/FreeRTOS-Kernel 순서로 Kernel을 찾습니다.

Windows의 Spring Boot가 8080으로 실행 중일 때 WSL에서는 아래처럼 실행합니다.
`localhost:8080`은 WSL 자기 자신을 가리키므로 쓰지 말고, Windows 호스트 IP를 씁니다.

~~~bash
HOST_IP=$(ip route show default | awk '/default/ {print $3}')
echo $HOST_IP
make run SERVER_URL=http://$HOST_IP:8080
~~~

정상 시 `[ASAK-RTOS] polling 시작: http://...:8080` 이후 연결 실패 없이 1초 간격 polling이 이어집니다.
영수증 이벤트가 오면 `[WorkerTask]` → 콘솔 ASCII 영수증 → `[RTOS -> Spring] ... COMPLETED` 순서로 출력됩니다.

상세 연결 절차는 [docs/rtos-connection-howto.md](docs/rtos-connection-howto.md)를 참고합니다.

QEMU 수업 예제는 이 POSIX 실행과 별개입니다. qemu-system-arm과 gcc-arm-none-eabi는 QEMU 예제를 시작할 때만 설치합니다.

## 트러블슈팅 (영수증이 안 나올 때)

이 프로젝트의 “영수증 출력”은 실제 프린터가 아니라 **RTOS 콘솔 ASCII 출력**입니다.
아래 세 가지가 모두 맞아야 동작합니다.

1. Windows에서 Spring Boot가 **실제로 기동 중** (8080 listen)
2. WSL에서 `asak_rtos`가 **호스트 IP**로 polling 중
3. Kiosk/Admin 또는 PowerShell로 `PRINT_RECEIPT` 이벤트 생성

### `BUILD SUCCESSFUL` ≠ Spring 실행 중

PowerShell에서 `.\gradlew.bat bootRun` 후 바로 프롬프트로 돌아오며 `BUILD SUCCESSFUL`만 보이면,
Gradle 빌드는 끝났지만 **앱은 기동 실패**한 상태입니다.

성공 시에는 프롬프트로 돌아오지 않고 아래 로그가 남아 있어야 합니다.

~~~text
Tomcat started on port 8080
Started AsakBackendApplication
~~~

WSL에서 연결 확인:

~~~bash
HOST_IP=$(ip route show default | awk '/default/ {print $3}')
curl -sS "http://$HOST_IP:8080/api/rtos/device-events/pending"
~~~

정상 예: `"success":true` 와 `"data":null`(대기 이벤트 없음) 또는 pending 이벤트 JSON.

### Spring Boot Temp 디렉터리 소유권 오류

`Unable to start web server` / `TomcatServletWebServerFactory.getWebServer` 아래에 아래 원인이 있으면
Tomcat 세션 Temp 폴더 소유권 검사 실패입니다.

~~~text
Caused by: java.lang.IllegalStateException:
Existing directory 'C:\Users\...\AppData\Local\Temp\8CA9CEBA...'
is not owned by BUILTIN\Administrators
~~~

PowerShell에서 Temp 폴더를 지운 뒤 다시 실행합니다.

~~~powershell
Remove-Item -Recurse -Force "$env:LOCALAPPDATA\Temp\8CA9CEBA6BC534A234D27F80C99DBB5CB0C0A16C" -ErrorAction SilentlyContinue
cd C:\ASAK-workspace\ASAK-back
.\gradlew.bat bootRun --no-daemon
~~~

재발을 줄이려면 `ASAK-back`의 `application.properties`에 다음을 둡니다.

~~~properties
server.servlet.session.persistent=false
~~~

### RTOS 쪽 `Interrupted system call`

FreeRTOS tick이 `connect()`를 `EINTR`로 깨울 수 있습니다.
`src/http_client.c`는 이 경우 재시도하도록 반영되어 있습니다.
오래된 바이너리를 쓰는 중이면 `make`로 다시 빌드한 뒤 `make run` 하세요.

### 포트/방화벽

- Windows: `netstat -ano | findstr :8080` 으로 LISTENING 확인
- WSL → Windows 8080이 타임아웃이면 Windows 방화벽 인바운드(8080) 확인
- 이미 떠 있는 Spring을 두고 `bootRun`을 또 실행하면 포트 충돌이 날 수 있음

## 파일과 코드 위치별 의미

| 파일 / 위치 | 의미 |
| --- | --- |
| CMakeLists.txt 1-6 | C11 프로젝트와 CMake 최소 버전을 선언합니다. |
| CMakeLists.txt 8-18 | FreeRTOS Kernel 경로를 찾습니다. |
| CMakeLists.txt 29-30 | Linux POSIX 포트 GCC_POSIX와 heap_4를 고정합니다. |
| Makefile 3-14 | make, make run, make clean과 SERVER_URL 전달을 정의합니다. |
| src/main.c 42-46 work_t | eventId, eventType, payload를 WorkerTask로 넘기는 작업 단위입니다. |
| src/main.c 51-78 json_value/json_string | 최소 문자열 방식으로 API JSON 키를 찾습니다. 복잡한 JSON에는 약합니다. |
| src/main.c 96-118 parse_work | data:null 또는 실제 pending event를 구분합니다. |
| src/main.c 120-151 handle_print_receipt | | 구분 payload를 나누고 콘솔 영수증을 출력합니다. |
| src/main.c 153-203 report_result | finish API에 COMPLETED 또는 FAILED 결과를 PATCH합니다. |
| src/main.c 205-283 WorkerTask/PollTask | 한 Worker만 실행하며 1초마다 pending event를 조회합니다. |
| src/http_client.c | POSIX TCP 연결, HTTP 요청/응답, chunked body 처리를 담당합니다. |
| config/FreeRTOSConfig.h | POSIX 시뮬레이터용 선점 스케줄링, tick, 동적 heap 설정입니다. |

## 관리자 영수증 형식 참조

관리자 OrderDetailPanel.jsx의 영수증 출력 UI를 참고합니다. 이 화면은 주문번호/일시/결제수단, 메뉴명·수량·단가, 옵션, 제외 재료, 메뉴 합계, 요청사항, 총 결제 금액을 표시합니다.

| 관리자 화면 위치 | 현재 RTOS 콘솔 반영 |
| --- | --- |
| 72-85 주문 기본 정보 | 주문번호만 반영 |
| 89-98 메뉴명, 수량, 단가 | 메뉴/수량 요약 문자열만 반영 |
| 99-134 옵션, 제외, 메뉴 합계 | 미구현 |
| 143-146 요청사항 | 미구현 |
| 150-154 총 결제 금액 | payload 금액 문자열만 반영 |
| 177-188 영수증 출력 버튼 | onPrintReceipt(orderId)와 RTOS API 연결 필요 |

현재 콘솔 영수증은 관리자 UI를 참고한 최소 시연본입니다. 옵션·제외·요청사항까지 출력하려면 payload를 JSON DTO로 확장하고 handle_print_receipt의 파싱 방식을 별도 설계로 바꿔야 합니다.

## 다음에 확정할 항목

- 이벤트 타입: PRINT_RECEIPT 공통 enum과 Kiosk/Admin 요청값
- payload: pipe 문자열 유지 또는 JSON DTO 전환
- 통신: connect timeout, 재시도와 중복 requestId 정책 (EINTR 재시도는 http_client에 반영됨)
- 장치: 실제 프린터 SDK/GPIO/USB/네트워크 출력 방식
- 운영: RTOS 인증, DB 이벤트 이력, 재시작 복구

- build/ 산출물, 토큰, .env, 보드별 비밀값은 커밋하지 않습니다.
