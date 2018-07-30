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


import collections
import subprocess
import os
import datetime
import sys
import gzip
import zipfile
from io import BytesIO


# Traces go to the cache/ directory when they are created, then moved into the
# cache/upload/ directory for uploading, and then moved back to cache/ once
# the upload is finished. This exception will get thrown if a trace file was
# found in one place and then moved and thus was not found at a later point
# in time.
class FileMovedException(Exception):
    def __init__(self, message):
        Exception.__init__(self, message)

# FileManager#getBaseFolder has the logic for where Profilo traces will exist. For
# now we assume that the traces will exist inside the internal data dir of our
# package, under the cache/ folder.
_PROFILO_DIR = 'cache/'
_TRACE_FILE_EXT = ".log"
_TRACE_FILE_EXPRESSION = '*' + _TRACE_FILE_EXT
_PROFILO_HEADER_START = 'dt\n'.encode("utf-8")

# -t to order by modified time for a nice default ordering.
_ADB_CMD_BASE = ['adb', 'shell', 'run-as']
_GET_DATA_DIR_FULL_PATH_CMD = ['pwd']
_GET_TRACES_CMD_FIND = ['find', '.', '-name', _TRACE_FILE_EXPRESSION]
_GET_TRACES_CMD_LS = ['ls', '-R', '|', 'grep', '-B', '1', '\.log']
_GET_FILE_MODIFIED_UNIX_TIME_CMD = ['stat', '-c', '%Y']
_GET_FILE_SIZE_CMD = ['stat', '-c', '%s']
# I'd rather use `test -d $dir` but we need a binary to run within `run-as`
_CHECK_PROFILO_DIR_EXISTS_CMD = ['ls', _PROFILO_DIR]
_ADB_EXEC_OUT_CMD_BASE = ['adb', 'exec-out', 'run-as']
_CAT_CMD = ['cat']

_NO_SUCH_FILE = b'No such file or directory'


DeviceTrace = collections.namedtuple('DeviceTrace', ['file_name', 'full_path',
    'modified_time', 'size'])


def _get_file_modified_unix_time(package, path):
    # ls on Android phones is missing --time-style, so use `stat` to get the
    # precise modified time.
    command = list(_ADB_CMD_BASE) + [package] \
        + list(_GET_FILE_MODIFIED_UNIX_TIME_CMD) + [path]
    output = subprocess.Popen(command, stdout=subprocess.PIPE,
        stderr=subprocess.PIPE)
    o_out, o_err = output.communicate()
    if o_err and _NO_SUCH_FILE in o_err:
        raise FileMovedException("File {f} was moved".format(f=path))
    return int(o_out.strip())


def _get_data_dir_full_path(package):
    command = list(_ADB_CMD_BASE) + [package] + list(_GET_DATA_DIR_FULL_PATH_CMD)
    return subprocess.check_output(command).decode("utf-8").strip()


def _is_profilo_trace(gzip_file):
    try:
        data = gzip_file.read()
        if not data.startswith(_PROFILO_HEADER_START):
            return False
    except IOError:
        # Not a gzipped file
        return False

    return True


def _validate_trace(package, data_dir_path, file_path):
    """
    1) If the file is a zip file (not gzip) then this is a multiproc trace,
       so unzip it and validate that all the files inside are profilo traces
       (skipping the "extra" directory)
    2) If the file is not a zip file, validate that it is a profilo trace

    A file is a "profilo trace" if:

        a) It is a gzipped file
        b) It starts with the magic _PROFILO_HEADER_START bytes
    """
    full_path = "/data/data/{package}/{path}".format(
                package=package, path=file_path)
    command = list(_ADB_EXEC_OUT_CMD_BASE) + [package] + list(_CAT_CMD) + [full_path]

    output = subprocess.Popen(command, stdout=subprocess.PIPE,
        stderr=subprocess.PIPE)
    o_out, o_err = output.communicate()

    # I've seen "no such file or directory" errors in cat being thrown to
    # STDOUT (in Android devices) and to STDERR in Linux desktops, so I'm
    # not going to take any chances and will just check both
    if (o_out and _NO_SUCH_FILE in o_out) or \
       (o_err and _NO_SUCH_FILE in o_err):
        raise FileMovedException("File {f} was moved".format(f=full_path))

    content = o_out

    file_like = BytesIO(content)
    if zipfile.is_zipfile(file_like):
        with zipfile.ZipFile(file_like) as zipped:
            info = zipped.infolist()
            for info_file in info:
                # Ignore *.tnd files
                if info_file.filename.startswith("extra/"):
                    continue
                f = zipped.open(info_file)
                gz = gzip.GzipFile(fileobj=BytesIO(f.read()))
                if not _is_profilo_trace(gz):
                    return False
    else:
        gz = gzip.GzipFile(fileobj=BytesIO(content))
        if not _is_profilo_trace(gz):
            return False

    return True


def _create_trace(package, data_dir_path, file_path):
    """
    Note: we could pull thet trace ID out of the filename, but this would be
    confusing because the ID is sanitized (special chars like '/' replaced)
    before being used as part of the filename, so it's likely the real ID
    is different.
    """

    file_path = file_path.strip()
    full_path = os.path.join(data_dir_path, file_path)
    full_path = os.path.abspath(full_path)
    file_name = os.path.basename(file_path)

    modified_time = \
        datetime.datetime.fromtimestamp(_get_file_modified_unix_time(package,
            full_path))

    command = list(_ADB_CMD_BASE) + [package] + list(_GET_FILE_SIZE_CMD) + \
                [full_path]

    output = subprocess.Popen(command, stdout=subprocess.PIPE,
        stderr=subprocess.PIPE)
    o_out, o_err = output.communicate()
    if o_err and _NO_SUCH_FILE in o_err:
        raise FileMovedException("File {f} was moved".format(f=full_path))

    size = int(o_out.strip())

    return DeviceTrace(file_name=file_name, full_path=full_path,
        modified_time=modified_time, size=size)


def _pull_trace(package, trace):
    # Since we're pulling files from internal data (without root), we can't use
    # adb pull :( So we just `cat` the file and write it out to a local file
    # with the same trace name.
    with open(trace.file_name, 'w') as trace_file:
        command = list(_ADB_EXEC_OUT_CMD_BASE) + [package] + list(_CAT_CMD) + [trace.full_path]
        popen = subprocess.Popen(command, stdout=trace_file)
        return popen.wait() == 0


def _check_profilo_dir_exists(package):
    dev_null = open(os.devnull, 'w')
    command = list(_ADB_CMD_BASE) + [package] + list(_CHECK_PROFILO_DIR_EXISTS_CMD)
    return subprocess.call(command, stdout=dev_null) == 0


def list_traces(package):
    if not _check_profilo_dir_exists(package):
        print("Profilo directory doesn't exist, looked for", _PROFILO_DIR,
            "inside package internal storage", file=sys.stderr)
        return []

    data_dir_path = _get_data_dir_full_path(package)
    used_ls = False

    try:
        command = list(_ADB_CMD_BASE) + [package] + list(_GET_TRACES_CMD_FIND)
        trace_files = subprocess.check_output(command).decode("utf-8").split('\n')
        trace_files = [x.strip() for x in trace_files]
        trace_files = [_f for _f in trace_files if _f]
    except subprocess.CalledProcessError as e:
        # Some devices don't have "find", so do this with "ls -R"
        print(str(e) + " Retrying with ls", file=sys.stderr)
        used_ls = True

    if used_ls:
        command = list(_ADB_CMD_BASE) + [package] + list(_GET_TRACES_CMD_LS)
        trace_files = subprocess.check_output(command).decode("utf-8").split('\n')
        trace_files = [x.strip() for x in trace_files]
        trace_files = [_f for _f in trace_files if _f]
        directory = trace_files.pop(0)[:-1]  # Remove trailing ":"
        trace_files = ["{d}/{f}".format(d=directory, f=x) for x in trace_files]

    # Due to the generality of the trace file expression, we might get stuff
    # that isn't actually a trace. Thus, do some validation.
    trace_files = [x for x in trace_files if _validate_trace(package, data_dir_path, x)]
    traces = [_create_trace(package, data_dir_path, x) for x in trace_files]
    traces = [_f for _f in traces if _f]
    return traces


def pull_last_trace(package):
    while True:
        try:
            traces = list_traces(package)
            break
        except FileMovedException as e:
            print(str(e) + ". Retrying...", file=sys.stderr)
        except Exception as e:
            print(str(e) + ". Exiting", file=sys.stderr)
            sys.exit(1)

    if not traces:
        print("Could not find any traces for package", package, file=sys.stderr)
        return None

    last_trace = max(traces, key=lambda x: x.modified_time)

    if _pull_trace(package, last_trace):
        # HACK: print out trace file name so that fbsystrace can read output to find which
        # was pulled. This should be removed after fbsystrace upgrade to python3 so that
        # we can directly call this function from fbsystrace.
        print(last_trace.file_name)
        return last_trace.file_name
    else:
        return None
