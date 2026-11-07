.DEFAULT_GOAL := help

UE_ROOT ?= /home/sarmy/UnrealEngine/UE_5.2
PROJECT := $(CURDIR)/AerialWorldLab.uproject
PYTHON ?= .venv/bin/python
CXX ?= g++
CPP_TEST := /tmp/aether_quadrotor_dynamics_test

.PHONY: help build editor play test python-test cpp-test

help:
	@printf '%s\n' \
		'Project commands:' \
		'  make build        Build the Unreal Editor target' \
		'  make editor       Open the project in Unreal Editor' \
		'  make play         Run the project as a standalone game' \
		'  make test         Run Python and standalone C++ tests' \
		'  make python-test  Run the retained Python simulator tests' \
		'  make cpp-test     Build and run the portable C++ dynamics tests' \
		'' \
		'Override the Unreal install with: make build UE_ROOT=/path/to/UE_5.2'

build:
	"$(UE_ROOT)/Engine/Build/BatchFiles/Linux/Build.sh" \
		AerialWorldLabEditor Linux Development -Project="$(PROJECT)" -WaitMutex

editor:
	"$(UE_ROOT)/Engine/Binaries/Linux/UnrealEditor" "$(PROJECT)"

play:
	"$(UE_ROOT)/Engine/Binaries/Linux/UnrealEditor" "$(PROJECT)" \
		-game -windowed -ResX=1280 -ResY=720

test: python-test cpp-test

python-test:
	"$(PYTHON)" -m pytest -q

cpp-test:
	"$(CXX)" -std=c++17 -Wall -Wextra -Werror -pedantic \
		-ISource/AerialWorldLab/Public \
		Source/AerialWorldLab/Private/Flight/QuadrotorDynamics.cpp \
		tests/cpp/quadrotor_dynamics_test.cpp -o "$(CPP_TEST)"
	"$(CPP_TEST)"
