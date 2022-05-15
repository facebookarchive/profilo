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

from __future__ import absolute_import, division, print_function, unicode_literals

import importlib


def load_module(configname):
    """
    This needs to be in its own file, so __name__ refers to the imported path
    and not just the value '__main__'.
    """

    base_name = __name__.split(".")[:-1]
    base_name = ".".join(base_name)
    return importlib.import_module(".config." + configname, base_name)
