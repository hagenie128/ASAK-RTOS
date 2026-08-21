# RTOS 실연결 방법 (2026-08-21 오전)

> 대상: `ASAK-back`(Windows) ↔ `ASAK-RTOS`(WSL Ubuntu, `~/ASAK-RTOS`)
> 참고: [asak-doc-sync-rtos-device-event-guide.md](../2026-08-19/asak-doc-sync-rtos-device-event-guide.md)

## 0. 현재 상태 확인 결과

- `ASAK-RTOS/src/main.c`, `http_client.c`에 polling(`GET /api/rtos/device-events/pending`) /
  결과보고(`PATCH /api/rtos/device-events/{eventId}/finish`) 로직이 이미 구현돼 있고,
  ASAK 공통 envelope의 `data.eventId/eventType/payload`를 파싱하도록 되어 있음.
- WSL에서 `make` 클린 빌드 성공 확인 (`asak_rtos` 바이너리 생성됨).
- FreeRTOS Kernel 경로: `~/rtos-kiosk-course/third_party/FreeRTOS-Kernel` (CMakeLists.txt가 자동 탐색).
- WSL → Windows 접근용 호스트 IP: `172.29.144.1` (환경마다 바뀔 수 있으니 아래 1-3 명령으로 매번 재확인).

## 1. Windows: Spring Boot 실행

```powershell
cd C:\ASAK-workspace\ASAK-back
.\gradlew.bat bootRun --no-daemon
```

- 로그에 `Tomcat started on port 8080`, `Started AsakBackendApplication` 뜨면 준비 완료.
- Windows 방화벽이 8080을 막고 있으면 WSL에서 연결이 안 되니, 필요하면 인바운드 규칙 확인.

## 2. WSL: 호스트 IP 확인

```bash
HOST_IP=$(ip route show default | awk '/default/ {print $3}')
echo $HOST_IP   # 예: 172.29.144.1
```

## 3. WSL: RTOS 클라이언트 빌드 & 실행

```bash
cd ~/ASAK-RTOS
export FREERTOS_KERNEL_PATH=~/rtos-kiosk-course/third_party/FreeRTOS-Kernel   # 필요시
make run SERVER_URL=http://$HOST_IP:8080
```

- 정상 시 콘솔에 `[ASAK-RTOS] polling 시작: http://172.29.144.1:8080` 출력 후 1초 간격 polling.
- `PENDING` 이벤트가 생기면 `[WorkerTask] eventId=...` → ASCII 영수증 출력 → `[RTOS -> Spring] eventId=..., status=COMPLETED` 순서로 로그가 찍혀야 정상.

## 4. Windows(또는 다른 터미널): 테스트 이벤트 생성

PowerShell 예시 (한글 payload는 `-Encoding utf8` 없이 보내면 깨질 수 있으니 아래처럼 UTF-8 바이트로 보낼 것):

```powershell
$body = '{"eventType":"PRINT_RECEIPT","payload":"ORDER-A1035|아메리카노 x 2|9000","requestId":"demo-001"}'

Invoke-RestMethod -Method Post `
  -Uri "http://localhost:8080/api/kiosk/orders/35/receipt-print" `
  -ContentType "application/json; charset=utf-8" `
  -Body ([System.Text.Encoding]::UTF8.GetBytes($body))
```

> 주의: bash(WSL)의 `curl -d '...'`로 한글 payload를 보내면 로케일에 따라
> `Invalid UTF-8 start byte` 500 에러가 날 수 있음 (오늘 아침 실제로 재현됨).
> PowerShell + UTF-8 바이트 전송, 또는 `curl --data-binary @file.json` 방식을 권장.

## 5. 확인 포인트

- [ ] Spring Boot 로그에 `POST /api/kiosk/orders/{id}/receipt-print` 200 응답
- [ ] RTOS 콘솔에 polling → WorkerTask → ASCII 영수증 출력
- [ ] RTOS 콘솔에 `report_result` PATCH 성공 로그
- [ ] `GET /api/kiosk/orders/device-events/{eventId}` 또는 `GET /api/admin/device-events`로 최종 `COMPLETED` 상태 확인

## 6. 막히면 볼 곳

- 전체 배경·API 계약·다음 단계(Kiosk/Admin React 연결, DB 전환)는
  [asak-doc-sync-rtos-device-event-guide.md](../2026-08-19/asak-doc-sync-rtos-device-event-guide.md) 참고.
- `ASAK-RTOS/README.md`에 파일별 라인 위치 설명과 최신 상태 표가 있음.

## 7. 역할 분담

| 담당 | 범위 | 저장소 |
|---|---|---|
| 팀원 | Kiosk: 영수증 / 번호표 / 영수증+번호표 출력 | `ASAK-Kiosk` |
| 나 | Admin: 주문 상세에서 영수증(재)출력 | `ASAK-Admin` |

공통 계약: `eventType`은 백엔드에서 **plain String**(enum 아님, `CreateDeviceEventRequest.eventType`)이라
값 이름만 맞추면 서로 충돌 없이 각자 작업 가능. 현재 확정된 값은 `PRINT_RECEIPT` 하나뿐이므로,
팀원이 `번호표`/`영수증+번호표`용 값(예: `PRINT_TICKET`, `PRINT_RECEIPT_AND_TICKET`)을 새로 쓸 계획이면
이름을 미리 맞춰두는 게 좋음. RTOS C 클라이언트(`handle_print_receipt`)는 현재 `PRINT_RECEIPT`만
처리하므로, 새 이벤트 타입이 생기면 `main.c`의 `worker_task`에 분기 추가가 필요함(팀원 or 별도 조율).

### 7-1. Kiosk (팀원 담당, 참고용 연결점만 기록)

- `ASAK-Kiosk/src/pages/kiosk/OrderCompletePage.jsx`: 주문 완료 후 `orderId/orderNo/totalAmount`를
  세션에 보관해 SCR-020으로 전달.
- `ASAK-Kiosk/src/pages/kiosk/ReceiptPage.jsx`: 출력 여부 선택 화면(Default/Loading/Empty/Error/Success/Disabled).
- `ASAK-Kiosk/src/components/kiosk/ReceiptActions.jsx`: 현재 `return null`인 빈 스텁 — 실제 CTA 구현 필요.
- 호출 API: `POST /api/kiosk/orders/{orderId}/receipt-print`, `GET /api/kiosk/orders/device-events/{eventId}`.

### 7-2. Admin 영수증 출력 (내 담당 — 구체 진행 순서)

현재 상태: 버튼 UI와 훅 자리는 이미 있고, TODO 주석으로 연결 지점만 비워둔 상태.

1. **`ASAK-Admin/src/constants/api.js`** — `API_ENDPOINTS`에 엔드포인트 추가.
   ```js
   printReceipt: (orderId) => `${API_BASE_PATH}/orders/${orderId}/receipt-print`,
   deviceEvents: `${API_BASE_PATH}/device-events`,
   ```

2. **`ASAK-Admin/src/api/ordersApi.js`** — TODO-041 자리에 실제 함수 추가 (mock `printAdminOrderReceipt`
   대신 백엔드 호출로 교체).
   ```js
   printReceipt: (orderId) =>
     apiClient.post(API_ENDPOINTS.printReceipt(orderId), {
       eventType: "PRINT_RECEIPT",
       payload: `...`,      // 주문번호|메뉴요약|금액 형식 (RTOS 파서 계약)
       requestId: crypto.randomUUID(),
     }),
   listDeviceEvents: () => apiClient.get(API_ENDPOINTS.deviceEvents),
   ```
   - 응답 `data.eventId`, `data.status`를 그대로 씀 (envelope 해제는 `apiClient`가 이미 처리하는지 확인).
   - `payload` 문자열은 RTOS `handle_print_receipt`가 `|`로 3분할해서 읽으므로 순서·구분자 유지 필수.

3. **`ASAK-Admin/src/pages/admin/OrderManagePage.jsx` (94~158줄 근처)** — TODO-043 주석 해제하고 구현.
   - `handlePrintReceipt(orderId)`에서 `printAdminOrderReceipt` 대신 `ordersApi.printReceipt(orderId)` 호출.
   - 성공 시 `result.data.eventId`를 저장해뒀다가, `PENDING`/`PROCESSING`일 때는
     "출력 완료"로 표시하지 말 것 (가이드 문서 4절 경고 사항). 필요하면 1초 polling으로
     `COMPLETED`/`FAILED`를 확인.
   - `<OrderDetailPanel onPrintReceipt={handlePrintReceipt} />` 주석 해제 (247번째 줄 근처).

4. **`ASAK-Admin/src/components/admin/orders/OrderDetailPanel.jsx`** — 이미 `onPrintReceipt(selectedOrder.orderId)`
   버튼(185줄)이 연결되어 있으므로 추가 수정 불필요. `canPrintReceipt` 조건(46~47줄)만 재확인.

5. **테스트**: Admin에서 결제완료 주문 하나 골라 "영수증 출력" 클릭 → 위 1~6절 방식으로 띄운
   Spring Boot + WSL RTOS 클라이언트에서 실제로 polling → 콘솔 영수증 → `finish` PATCH까지
   찍히는지 확인. `GET /api/admin/device-events`로 최종 상태 재조회.

6. **주의**: `requestSource=ADMIN`으로 기록되는지 확인(`AdminDeviceEventController`가
   `createReceiptPrintEvent(orderId, request, "ADMIN")`로 고정 전달하므로 별도 처리 불필요, 백엔드 응답에서
   `requestSource` 필드로 재검증만).
