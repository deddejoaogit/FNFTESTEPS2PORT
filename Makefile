# FNF PS2 - motor do port
#
#   make test      bateria do motor no PC (nao precisa de ps2sdk)
#   make prep      selftest da preparacao de imagens
#   make orcamento tabela de VRAM por palco
#   make limpar
#
# A compilacao do ELF de PS2 depende do toolchain ps2dev, que este
# ambiente nao tem. O alvo 'ps2' fica aqui registrado para quando houver:
# ele compila exatamente os mesmos fontes de engine/ que o teste exercita.
CC      ?= gcc

PY      ?= python3
PS2DEV  ?= /usr/local/ps2dev

.PHONY: test prep orcamento charts limpar ps2

test:
	$(MAKE) -C tests test

prep:
	$(PY) tools/prep_assets.py --selftest

orcamento:
	$(PY) tools/prep_assets.py --orcamento

charts:
	$(PY) tools/check_charts.py --dir charts

limpar:
	$(MAKE) -C tests clean
	rm -rf tools/__pycache__

# Alvo de referencia. Os fontes de engine/ nao incluem nada de PS2: o que
# os liga ao console sao apenas os ponteiros de funcao (carregar/enviar).
ps2:
	@echo "precisa de: $(PS2DEV)/ee/bin/mips64r5900el-ps2-elf-gcc"
	@echo "veja docs/MODELO_CARREGAMENTO.md para o patch do main.c"
