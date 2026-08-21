/**
 * CommandPollTask
 *
 * 1. GET /api/rtos/device-events/pending
 * 2. 응답 data에서 eventId, eventType, payload 읽기
 * 3. PENDING 이벤트가 있으면 WorkerTask 생성
 *
 * WorkerTask
 *
 * PRINT_RECEIPT
 * - payload: 주문번호|메뉴요약|금액
 * - "|" 기준으로 분리한 뒤 기존 영수증 형식으로 출력
 *
 * PRINT_RECEIPT_TEXT
 * - payload: 이미 완성된 영수증 텍스트
 * - 줄바꿈/옵션/제외/결제정보 등을 포함한 payload를 그대로 출력
 *
 * 처리 후
 * PATCH /api/rtos/device-events/{eventId}/finish
 * status=COMPLETED 또는 FAILED 결과 보고
 */

#include "http_client.h"
#include "FreeRTOS.h"
#include "task.h"

#include <stdint.h>

/* Spring 장치 이벤트 한 건을 WorkerTask에 넘기는 작업 단위입니다. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RESPONSE_CAPACITY 4096
#define RESULT_CAPACITY 256
#define PAYLOAD_CAPACITY 2048
#define WORKER_TASK_STACK_SIZE 4096
#define POLL_TASK_STACK_SIZE 4096

typedef struct {
    uint32_t event_id;
    char event_type[32];
    char payload[PAYLOAD_CAPACITY];
} work_t;

static http_server_t spring_server;
static volatile BaseType_t worker_running = pdFALSE;

static const char *json_value(const char *json, const char *key) {
    static char pattern[64];

    snprintf(pattern, sizeof(pattern), "\"%s\":", key);

    const char *value = strstr(json, pattern);
    return value == NULL ? NULL : value + strlen(pattern);
}

static int json_string(
    const char *json,
    const char *key,
    char *out,
    size_t size
) {
    const char *value = json_value(json, key);

    if (value == NULL) {
        return -1;
    }

    /* ':' 뒤 공백 허용 */
    while (*value == ' ' || *value == '\t' ||
           *value == '\r' || *value == '\n') {
        value++;
    }

    if (*value != '"') {
        return -1;
    }

    value++;

    size_t index = 0;

    while (*value != '\0' && index + 1 < size) {

        /* JSON 문자열 종료 */
        if (*value == '"') {
            out[index] = '\0';
            return 0;
        }

        /* JSON escape 처리 */
        if (*value == '\\') {

            value++;

            if (*value == '\0') {
                break;
            }

            switch (*value) {

                case 'n':
                    out[index++] = '\n';
                    break;

                case 'r':
                    out[index++] = '\r';
                    break;

                case 't':
                    out[index++] = '\t';
                    break;

                case '"':
                    out[index++] = '"';
                    break;

                case '\\':
                    out[index++] = '\\';
                    break;

                case '/':
                    out[index++] = '/';
                    break;

                default:
                    /* 최소 구현: 알 수 없는 escape는 문자 자체 복사 */
                    out[index++] = *value;
                    break;
            }

            value++;
            continue;
        }

        out[index++] = *value++;
    }

    out[index] = '\0';

    return -1;
}

/*
 * ASAK 응답:
 * {
 *   "success": true,
 *   "data": {
 *     "eventId": 1,
 *     "eventType": "PRINT_RECEIPT",
 *     "payload": "ORDER-A1035|아메리카노 x 2|9000"
 *   }
 * }
 */
static int parse_work(const char *json, work_t *work) {
    if (strstr(json, "\"data\":null") != NULL) {
        return 0;
    }

    const char *event_id = json_value(json, "eventId");

    if (event_id == NULL) {
        return -1;
    }

    work->event_id = (uint32_t) strtoul(event_id, NULL, 10);

    if (json_string(json, "eventType", work->event_type, sizeof(work->event_type)) != 0) {
        return -1;
    }

    if (json_string(json, "payload", work->payload, sizeof(work->payload)) != 0) {
        return -1;
    }

    return 1;
}

static int handle_print_receipt(
    const work_t *work,
    char *result,
    size_t result_size
) {
    static char payload[PAYLOAD_CAPACITY];

    snprintf(payload, sizeof(payload), "%s", work->payload);

    char *order_no = strtok(payload, "|");
    char *items = strtok(NULL, "|");
    char *amount = strtok(NULL, "|");

    if (order_no == NULL || items == NULL || amount == NULL) {
        snprintf(result, result_size, "invalid receipt payload");
        return -1;
    }

    printf("\n+--------------------------------------+\n");
    printf("|            ASAK RECEIPT              |\n");
    printf("+--------------------------------------+\n");
    printf("  주문 번호 : %s\n", order_no);
    printf("  주문 상세 : %s\n", items);
    printf("  결제 금액 : %s원\n", amount);
    printf("+--------------------------------------+\n\n");

    vTaskDelay(pdMS_TO_TICKS(1200));

    snprintf(result, result_size, "receipt printed: %s", order_no);

    return 0;
}

static int handle_print_receipt_text(
    const work_t *work,
    char *result,
    size_t result_size
) {
    printf("\n%s\n\n", work->payload);

    vTaskDelay(pdMS_TO_TICKS(1200));

    snprintf(
        result,
        result_size,
        "receipt text printed"
    );

    return 0;
}

static void report_result(
    const work_t *work,
    const char *status,
    const char *result
) {
    char path[128];
    char json[512];
    static char response_body[RESPONSE_CAPACITY];
    http_response_t response = {0};

    snprintf(
        path,
        sizeof(path),
        "/api/rtos/device-events/%u/finish",
        work->event_id
    );

    snprintf(
        json,
        sizeof(json),
        "{\"status\":\"%s\",\"result\":\"%s\"}",
        status,
        result
    );

    if (
        http_request(
            &spring_server,
            "PATCH",
            path,
            json,
            response_body,
            sizeof(response_body),
            &response
        ) == 0
        && response.status_code == 200
    ) {
        printf(
            "[RTOS -> Spring] eventId=%u, status=%s, result=%s\n",
            work->event_id,
            status,
            result
        );
    } else {
        fprintf(
            stderr,
            "[RTOS] 결과 보고 실패: HTTP %d\n",
            response.status_code
        );
    }
}

static void worker_task(void *parameter) {

    work_t *work = (work_t *)parameter;

    char result[RESULT_CAPACITY] = {0};

    printf(
        "[WorkerTask] eventId=%u, eventType=%s\n",
        work->event_id,
        work->event_type
    );

    if (strcmp(work->event_type, "PRINT_RECEIPT") == 0) {

        if (handle_print_receipt(
                work,
                result,
                sizeof(result)
            ) == 0) {

            report_result(work, "COMPLETED", result);

        } else {

            report_result(work, "FAILED", result);
        }

    } else if (
        strcmp(work->event_type, "PRINT_RECEIPT_TEXT") == 0
    ) {

        if (handle_print_receipt_text(
                work,
                result,
                sizeof(result)
            ) == 0) {

            report_result(work, "COMPLETED", result);

        } else {

            report_result(work, "FAILED", result);
        }

    } else {

        snprintf(
            result,
            sizeof(result),
            "unsupported eventType: %s",
            work->event_type
        );

        report_result(work, "FAILED", result);
    }

    vPortFree(work);

    worker_running = pdFALSE;

    vTaskDelete(NULL);
}

static void command_poll_task(void *parameter) {

    (void)parameter;

    static char body[RESPONSE_CAPACITY];
    static work_t parsed;

    for (;;) {

        if (!worker_running) {

            memset(&parsed, 0, sizeof(parsed));

            http_response_t response = {0};

            int request_ok = http_request(
                &spring_server,
                "GET",
                "/api/rtos/device-events/pending",
                NULL,
                body,
                sizeof(body),
                &response
            );

            if (
                request_ok == 0
                && response.status_code == 200
                && parse_work(body, &parsed) == 1
            ) {
                work_t *work = pvPortMalloc(sizeof(*work));

                if (work != NULL) {
                    *work = parsed;
                    worker_running = pdTRUE;

                    if (
                        xTaskCreate(
    worker_task,
    "WorkerTask",
    WORKER_TASK_STACK_SIZE,
    work,
    3,
    NULL
) != pdPASS
                    ) {
                        worker_running = pdFALSE;
                        vPortFree(work);
                    }
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void vApplicationMallocFailedHook(void) {
    abort();
}

int main(int argc, char **argv) {
    const char *url =
        argc >= 2 ? argv[1] : "http://localhost:8080";

    if (http_server_parse(url, &spring_server) < 0) {
        return EXIT_FAILURE;
    }

    configASSERT(
        xTaskCreate(
            command_poll_task,
            "CommandPollTask",
            POLL_TASK_STACK_SIZE,
            NULL,
            2,
            NULL
        ) == pdPASS
    );

    printf("[ASAK-RTOS] polling 시작: %s\n", url);

    vTaskStartScheduler();

    return EXIT_FAILURE;
}