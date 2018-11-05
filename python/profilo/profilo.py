"""
Copyright 2018-present, Facebook, Inc.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
"""


import argparse
from .device.device import pull_last_trace, pull_all_traces, pull_n_traces

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Profilo commandline utility!")
    subparsers = parser.add_subparsers(help="Subparsers")

    pull = subparsers.add_parser("pull_traces", help="Pull traces from the device")
    group = pull.add_mutually_exclusive_group(required=True)
    group.add_argument("--last", action="store_true", help="Pull only the last trace")
    group.add_argument("--all", action="store_true", help="Pull all existing traces")
    group.add_argument("--count", type=int, help="Pull the last COUNT traces")

    parser.add_argument("package", type=str, help="Specify the package name using Profilo, e.g. com.foo.bar")

    args = parser.parse_args()

    if args.last:
        pull_last_trace(args.package)
    elif args.all:
        pull_all_traces(args.package)
    else:
        pull_n_traces(args.package, args.count)
