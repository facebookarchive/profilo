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
from .device.device import pull_last_trace

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Profilo commandline utility!")
    parser.add_argument('command', type=str, choices=['pull_last_trace'],
        help="Pull most recent trace file")
    parser.add_argument('package', type=str,
        help="Specify the package name using Profilo, e.g. com.foo.bar")
    args = parser.parse_args()

    if args.command == 'pull_last_trace':
        pull_last_trace(args.package)
