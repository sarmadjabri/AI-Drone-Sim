"""Command-line entry point for running and viewing Project AETHER simulations."""

import argparse
import json
from pathlib import Path

from .config import Scenario
from .simulation import Simulation


def _parser():
    parser = argparse.ArgumentParser(
        prog="aether",
        description="Simulate a motor-driven quadrotor flying a waypoint mission.",
    )
    parser.add_argument("--scenario", type=Path, help="scenario JSON file")
    parser.add_argument(
        "--seconds", type=float, default=30.0, help="maximum simulated duration (default: 30)"
    )
    parser.add_argument("--viewer", action="store_true", help="open the interactive 3D viewer")
    parser.add_argument("--seed", type=int, help="override the scenario's sensor-noise seed")
    return parser


def main(argv=None):
    args = _parser().parse_args(argv)
    scenario = Scenario.load(args.scenario) if args.scenario else Scenario()
    if args.seed is not None:
        scenario.seed = args.seed
    simulation = Simulation(scenario)

    if args.viewer:
        from .viewer import run_viewer

        run_viewer(simulation, args.seconds)
        return 0

    result = simulation.run(args.seconds)
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
