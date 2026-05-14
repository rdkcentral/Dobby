# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2020 Sky UK
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

import test_utils
import json
from copy import deepcopy

# in case we would like to change container name
container_name = "sleepy"

tests = (
    test_utils.Test("Create bundle",
                    container_name,
                    "Dobby Bundle Generator Tool",
                    "Generates bundle with DobbyBundleGenerator"),
    test_utils.Test("Diff bundles",
                    container_name,
                    "",
                    "Compares config.json between original bundle and generated one, and verifies rootfs exists"),
    test_utils.Test("Remove bundle",
                    container_name,
                    "",
                    "Removes files created by this test group"),
)


def _load_json(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def _normalise_config(config):
    # make a copy so we don't mutate the original object
    cfg = deepcopy(config)

    # Some runtimes place this at top-level, some under linux
    cfg.pop("rootfsPropagation", None)
    if isinstance(cfg.get("linux"), dict):
        cfg["linux"].pop("rootfsPropagation", None)

        # User namespace mappings can be injected by generator/runtime on some platforms
        cfg["linux"].pop("uidMappings", None)
        cfg["linux"].pop("gidMappings", None)

        if isinstance(cfg["linux"].get("namespaces"), list):
            cfg["linux"]["namespaces"] = [
                ns for ns in cfg["linux"]["namespaces"]
                if ns.get("type") != "user"
            ]

        # realtime fields often appear as explicit nulls in generated configs
        resources = cfg["linux"].get("resources")
        if isinstance(resources, dict) and isinstance(resources.get("cpu"), dict):
            cpu = resources["cpu"]
            if cpu.get("realtimeRuntime") is None:
                cpu.pop("realtimeRuntime", None)
            if cpu.get("realtimePeriod") is None:
                cpu.pop("realtimePeriod", None)
            if not cpu:
                resources.pop("cpu", None)

        # swap limit is injected by the OCI config template (set equal to
        # memory limit to disable swap).  Original test bundles pre-date
        # this addition, so strip it to keep the comparison stable.
        if isinstance(resources, dict) and isinstance(resources.get("memory"), dict):
            resources["memory"].pop("swap", None)

    # Runtime may append tmpfs size options at generation time
    for mount in cfg.get("mounts", []):
        if mount.get("destination") in ("/tmp", "/dev") and isinstance(mount.get("options"), list):
            mount["options"] = [opt for opt in mount["options"] if not str(opt).startswith("size=")]

    # Networking plugin can be auto-disabled depending on environment
    if isinstance(cfg.get("rdkPlugins"), dict):
        cfg["rdkPlugins"].pop("networking", None)

    return cfg


def execute_test():

    # this testcase is using tarball bundle so it gets all empty folders. They would get skipped by git. So in that
    # case it is better to untar and cleanup to have easy "diff" than to hack "diff -ignore" files.

    output_table = []

    bundle_ctx = test_utils.untar_bundle(container_name)
    with bundle_ctx as bundle_path:
        if not bundle_ctx.valid:
            test = tests[0]
            output = test_utils.create_simple_test_output(
                test, False,
                "Bundle extraction or validation failed",
                "Bundle tarball could not be extracted or config.json was missing"
            )
            test_utils.print_single_result(output)
            return test_utils.count_print_results([output])
        
        # Test 0
        test = tests[0]
        status = test_utils.run_command_line(["DobbyBundleGenerator",
                                              "-i",
                                              test_utils.get_container_spec_path(test.container_id),
                                              "-o",
                                              test_utils.get_bundle_path(test.container_id)])

        message = ""
        result = True
        if test.expected_output.lower() not in status.stdout.lower():
            message = "Failed to run Dobby Bundle Generator Tool"
            result = False
        if "failed to create bundle" in status.stderr.lower():
            message = "Failed to create bundle"
            result = False

        output = test_utils.create_simple_test_output(test, result, message, status.stderr)
        output_table.append(output)
        test_utils.print_single_result(output)

        # Test 1
        test = tests[1]
        generated_config_path = test_utils.get_bundle_path(test.container_id) + "/config.json"
        original_config_path = bundle_path + "/config.json"

        result = True
        message = ""
        log = ""

        try:
            generated_config = _normalise_config(_load_json(generated_config_path))
            original_config = _normalise_config(_load_json(original_config_path))

            if generated_config != original_config:
                result = False
                message = "Normalized config.json mismatch"
                log = (
                    "Generated config:\n" + json.dumps(generated_config, sort_keys=True) +
                    "\nOriginal config:\n" + json.dumps(original_config, sort_keys=True)
                )

            # Verify rootfs directory exists in generated bundle
            import os
            generated_rootfs = os.path.join(test_utils.get_bundle_path(test.container_id), "rootfs")
            if not os.path.isdir(generated_rootfs):
                result = False
                message = (message + "; " if message else "") + "Generated bundle missing rootfs directory"
                log = (log + "\n" if log else "") + "Expected rootfs at: %s" % generated_rootfs
        except Exception as err:
            result = False
            message = "Failed to compare bundle configs"
            log = str(err)

        output = test_utils.create_simple_test_output(test, result, message, log)
        output_table.append(output)
        test_utils.print_single_result(output)

        # Test 2
        test = tests[2]
        status = test_utils.run_command_line(["rm",
                                              "-rf",
                                              test_utils.get_bundle_path(test.container_id)])

        output = test_utils.create_simple_test_output(test, (status.stderr == ""), "", status.stderr)
        output_table.append(output)
        test_utils.print_single_result(output)

    return test_utils.count_print_results(output_table)


if __name__ == "__main__":
    test_utils.parse_arguments(__file__)
    execute_test()

