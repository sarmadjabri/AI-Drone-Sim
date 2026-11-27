# Optional shortcuts; scripts/project.py is the shared Windows/Linux/macOS runner.
.DEFAULT_GOAL := help
PYTHON ?= .venv/bin/python
UE_ROOT ?=
UE_TEST_FILTER ?= Aether.Flight
UE_TEST_ARGS ?=
PROJECT_RUNNER = "$(PYTHON)" scripts/project.py
ENGINE_OPTION = $(if $(strip $(UE_ROOT)),--ue-root "$(UE_ROOT)",)

COMMANDS = doctor build editor play play-windowed play-low headless profile test \
	python-test cpp-test readability-check prepare-new-york import-new-york bootstrap-city
.PHONY: help $(COMMANDS) unreal-test

help:
	@$(PROJECT_RUNNER) --help

$(COMMANDS):
	$(PROJECT_RUNNER) $@ $(ENGINE_OPTION)

unreal-test:
	$(PROJECT_RUNNER) unreal-test $(ENGINE_OPTION) --test-filter "$(UE_TEST_FILTER)" \
		$(foreach argument,$(UE_TEST_ARGS),--test-arg=$(argument))
