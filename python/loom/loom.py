from __future__ import absolute_import

import argparse
from .device.device import pull_last_trace

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Loom commandline utility!")
    parser.add_argument('command', type=str, choices=['pull_last_trace'],
        help="Pull most recent trace file")
    parser.add_argument('package', type=str,
        help="Specify the package name using Loom, e.g. com.foo.bar")
    args = parser.parse_args()

    if args.command == 'pull_last_trace':
        pull_last_trace(args.package)
