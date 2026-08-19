
# all/run/clean은 파일이 아니라 make가 실행할 작업 이름입니다.

.PHONY: all run clean

APP ?= asak_rtos
SERVER_URL ?= http://localhost:8080

all:
	cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
	cmake --build build -j

run: all
	./build/$(APP) $(SERVER_URL)

clean:
	cmake -E remove_directory build