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

import argparse
import json
from os import path


def main():
    """
    Edits the dobby_schema.json file to add references to any additional schema
    files to allow plugins to be built outside of the main Dobby tree

    Will always include the defs-plugins.json file from this directory
    """
    parser = argparse.ArgumentParser(
        description='Add external plugin schema files')

    parser.add_argument(
        '--dobbyschema', help="Path to dobby_schema.json to modify", required=True)
    parser.add_argument('files', metavar='filepath', type=argparse.FileType(),
                        nargs='*', help="json schema files")
    args = parser.parse_args()

    rdkplugin_schema = {
        "allOf": [
            {
                "$ref": "defs-plugins.json#/definitions/RdkPlugins"
            }
        ]
    }

    filenames = [path.basename(x.name) for x in args.files]

    if filenames:
        for name in filenames:
            rdkplugin_schema["allOf"].append(
                {"$ref": "{}#/definitions/RdkPlugins".format(name)})
    else:
        print("No external schemas to add - setting to include defs-plugins.json only")

    with open(args.dobbyschema, 'r+') as schema_file:
        schema_json = json.load(schema_file)

        schema_json['properties']['rdkPlugins'] = rdkplugin_schema

        schema_file.seek(0)
        json.dump(schema_json, schema_file, indent=4)
        schema_file.truncate()

    print("Add schemas DONE")


if __name__ == "__main__":
    main()
