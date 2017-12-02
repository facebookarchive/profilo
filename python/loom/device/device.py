from __future__ import print_function

import collections
import subprocess
import os
import datetime
import sys

# FileManager#getBaseFolder has the logic for where Loom traces will exist. For
# now we assume that the traces will exist inside the internal data dir of our
# package, under the cache/ folder.
_LOOM_DIR = 'cache/'
_TRACE_FILE_EXT = "zip.log"

# -t to order by modified time for a nice default ordering.
_ADB_CMD_BASE = ['adb', 'shell', 'run-as']
_GET_DATA_DIR_FULL_PATH_CMD = ['pwd']
_GET_TRACES_CMD = ['ls', '-lt', _LOOM_DIR]
_GET_FILE_MODIFIED_UNIX_TIME_CMD = ['stat', '-c', '%Y']
# I'd rather use `test -d $dir` but we need a binary to run within `run-as`
_CHECK_LOOM_DIR_EXISTS_CMD = ['ls', _LOOM_DIR]
_CAT_TRACE_CMD = ['cat']


DeviceTrace = collections.namedtuple('DeviceTrace', ['file_name', 'full_path',
    'modified_time', 'size'])


def _get_file_modified_unix_time(package, path):
    # ls on Android phones is missing --time-style, so use `stat` to get the
    # precise modified time.
    command = list(_ADB_CMD_BASE) + [package] \
        + list(_GET_FILE_MODIFIED_UNIX_TIME_CMD) + [path]
    return int(subprocess.check_output(command).strip())


def _get_data_dir_full_path(package):
    command = list(_ADB_CMD_BASE) + [package] + list(_GET_DATA_DIR_FULL_PATH_CMD)
    return subprocess.check_output(command).strip()


def _create_trace(package, data_dir_path, line):
    """
    Parse the output of `ls -lt` into DeviceTrace objects, returning None if
    not a trace file or if line is malformed.

    Note: we could pull thet trace ID out of the filename, but this would be
    confusing because the ID is sanitized (special chars like '/' replaced)
    before being used as part of the filename, so it's likely the real ID
    is different.

    -rw------- 1 u0_a79 u0_a79 20855 2017-12-01 13:32 override-EA_1vtQZg5J.zip.log
    """
    parts = line.split(" ")

    if len(parts) != 8:
        return None

    # I have no idea why `adb shell run-as $pkg ls -lt` returns a relative path
    # instead of the full path... so we create the full path manually.
    file_name = parts[-1]
    full_path = os.path.join(data_dir_path, _LOOM_DIR, file_name)

    if not file_name.endswith(_TRACE_FILE_EXT):
        return None

    modified_time = \
        datetime.datetime.fromtimestamp(_get_file_modified_unix_time(package,
            full_path))
    size = int(parts[4])

    return DeviceTrace(file_name=file_name, full_path=full_path,
        modified_time=modified_time, size=size)


def _pull_trace(package, trace):
    # Since we're pulling files from internal data (without root), we can't use
    # adb pull :( So we just `cat` the file and write it out to a local file
    # with the same trace name.
    with open(trace.file_name, 'w') as trace_file:
        command = list(_ADB_CMD_BASE) + [package] + list(_CAT_TRACE_CMD) \
            + [trace.full_path]
        popen = subprocess.Popen(command, stdout=trace_file)
        return popen.wait() == 0


def _check_loom_dir_exists(package):
    dev_null = open(os.devnull, 'w')
    command = list(_ADB_CMD_BASE) + [package] + list(_CHECK_LOOM_DIR_EXISTS_CMD)
    return subprocess.call(command, stdout=dev_null) == 0


def list_traces(package):
    if not _check_loom_dir_exists(package):
        print("Loom directory doesn't exist, looked for", _LOOM_DIR,
            "inside package internal storage", file=sys.stderr)
        return []

    command = list(_ADB_CMD_BASE) + [package] + list(_GET_TRACES_CMD)
    trace_files = subprocess.check_output(command).split('\n')

    trace_files = filter(None, trace_files)

    data_dir_path = _get_data_dir_full_path(package)
    traces = map(lambda x: _create_trace(package, data_dir_path, x), trace_files)
    traces = filter(None, traces)
    return traces


def pull_last_trace(package):
    traces = list_traces(package)
    if not traces:
        return False

    last_trace = max(traces, key=lambda x: x.modified_time)
    return _pull_trace(package, last_trace)
