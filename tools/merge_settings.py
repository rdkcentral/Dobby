# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2021 Sky UK
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Used if we have a -DSETTINGS_APPEND build time argument. Combines multiple
# json settings files together into a single file

import argparse
import pathlib
import json
from sys import exit
from os import path

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('files', metavar='filepath', type=pathlib.Path, nargs='+')
    parser.add_argument('--base', type=pathlib.Path, required=True)
    parser.add_argument('--output', type=pathlib.Path, required=True)
    args = parser.parse_args()

    # Check we were given valid files
    invalid_files = [x for x in args.files if not x.is_file()]

    if invalid_files:
        print("ERROR: File(s) not found:", [str(x) for x in invalid_files])
        exit(1)

    # Load the json to dicts
    loaded_json = []
    for file in args.files:
        with file.open() as json_file:
            loaded_json.append(json.load(json_file))

    base_settings = {}
    with args.base.open() as json_file:
        base_settings = json.load(json_file)

    # Merge everything into one
    merged = merge_dicts(base_settings, loaded_json)

    # Save the file
    with args.output.open('w') as output_file:
        json.dump(merged, output_file, ensure_ascii=False, indent=4)


def merge_dicts(base_file, list_dicts_to_merge):
    """
    Taking an original settings file, merge settings file(s) on top of the original
    without deleting any of the original settings file

    E.G if the original settings file containers:
    foo: {
        mySettings: ['A', 'B']
    }

    and the append file contains:
    foo: {
        mySettings: ['C']
    }

    the final result will be:
    foo: {
        mySettings: ['A', 'B', 'C']
    }
    """
    result = base_file

    for dictionary in list_dicts_to_merge:
        do_merge(result, dictionary)

    return result

def do_merge(result, to_merge):
    """
    Recursively merge the dictionaries together
    """
    for k, v in to_merge.items():
        if isinstance(v, list):
            if k not in result:
                result[k] = []
            result[k].extend([x for x in v if x not in result[k]])
        elif isinstance(v, dict):
            if k not in result:
                result[k] = {}
            do_merge(result[k], v)
        else:
            result[k] = v


if __name__ == "__main__":
    main()