#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <stddef.h>


/* main.c가 HTTP client에 요청할 때 사용하는 공개 자료형과 함수 선언입니다. */

typedef struct {
    char host[128];
    unsigned short port;
} http_server_t;

typedef struct {
    int status_code;
    size_t body_length;
} http_response_t;

int http_server_parse(const char *url, http_server_t *server);

int http_request(
    const http_server_t *server,
    const char *method,
    const char *path,
    const char *json_body,
    char *response_body,
    size_t response_capacity,
    http_response_t *response
);

#endif