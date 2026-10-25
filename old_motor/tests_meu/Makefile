# Testes do motor no PC. Sem toolchain de PS2, sem console.
#
#   make test            roda todos
#   make test-open       abre os fontes do ps2sdk (nao usado aqui)
#   make rebuild         limpa e roda
#
# Tudo e -Wall -Wextra -Werror: se aparecer aviso, e bug.
CC      ?= gcc
CFLAGS  ?= -O2 -g -Wall -Wextra -Werror -std=gnu11
ENGINE  := ../src/engine
BIN     := bin

TESTES := test_vram_cache test_conductor test_judgement test_chart \
          test_asset_bank test_bootlog

.PHONY: all test clean
all: test

$(BIN):
	@mkdir -p $(BIN)

$(BIN)/test_vram_cache: test_vram_cache.c $(ENGINE)/vram_cache.c | $(BIN)
	$(CC) $(CFLAGS) -o $@ $^ -lm
$(BIN)/test_conductor: test_conductor.c $(ENGINE)/conductor.c | $(BIN)
	$(CC) $(CFLAGS) -o $@ $^ -lm
$(BIN)/test_judgement: test_judgement.c $(ENGINE)/judgement.c | $(BIN)
	$(CC) $(CFLAGS) -o $@ $^ -lm
$(BIN)/test_chart: test_chart.c $(ENGINE)/chart.c | $(BIN)
	$(CC) $(CFLAGS) -o $@ $^ -lm
$(BIN)/test_asset_bank: test_asset_bank.c $(ENGINE)/asset_bank.c $(ENGINE)/vram_cache.c | $(BIN)
	$(CC) $(CFLAGS) -o $@ $^ -lm
$(BIN)/test_bootlog: test_bootlog.c $(ENGINE)/bootlog.c | $(BIN)
	$(CC) $(CFLAGS) -o $@ $^ -lm

test: $(addprefix $(BIN)/,$(TESTES))
	@echo
	@echo "================ BATERIA DE TESTES ================"
	@falhas=0; \
	for t in $(TESTES); do \
	  echo; echo "--- $$t"; \
	  ./$(BIN)/$$t || falhas=$$((falhas+1)); \
	done; \
	echo; echo "==================================================="; \
	if [ $$falhas -eq 0 ]; then echo "TODOS OS TESTES PASSARAM"; else echo "$$falhas TESTE(S) FALHARAM"; fi; \
	exit $$falhas

clean:
	rm -rf $(BIN)
