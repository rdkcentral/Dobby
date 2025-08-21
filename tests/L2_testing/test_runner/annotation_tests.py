# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2024 Sky UK
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
import os
import json
import shutil
import subprocess

tests = [
    test_utils.Test("annotations",
                    "sleepy",
                    "\"Key1\" : \"Value1\"",
                    "Starts container, adds a key&value pair, confirms annotation, then removes the annotation and confirms removal"),
]


def execute_test():
    with test_utils.dobby_daemon():
        output_table = []

        for test in tests:
            result = test_container(test.container_id, test.expected_output)
            output = test_utils.create_simple_test_output(test, result[0], result[1])

            output_table.append(output)
            test_utils.print_single_result(output)


    return test_utils.count_print_results(output_table)


def test_container(container_id, expected_output):
    """Runs container and check if output contains expected output

    Parameters:
    container_id (string): name of container to run
    expected_output (string): output that should be provided by container

    Returns:
    (pass (bool), message (string)): Returns if expected output found and message

    """
    test_utils.print_log("Running %s container test" % container_id, test_utils.Severity.debug)

    with test_utils.untar_bundle(container_id) as bundle_path:
        command = ["DobbyTool","-v",
                "start",
                container_id,
                bundle_path]
        
        if container_id == "sleepy":
            rootfs = os.path.join(bundle_path, "rootfs")
            bin_dir = os.path.join(rootfs, "bin")
            os.makedirs(bin_dir, exist_ok=True)
        
            # Ensure /bin/sleep exists inside rootfs
            sleep_in_rootfs = os.path.join(bin_dir, "sleep")
            if not os.path.exists(sleep_in_rootfs):
                try:
                    shutil.copy2("/bin/sleep", sleep_in_rootfs)
                    os.chmod(sleep_in_rootfs, 0o755)
                    print(f"✅ Copied /bin/sleep -> {sleep_in_rootfs}")
                except Exception as e:
                    print(f"❌ Failed to copy sleep: {e}")
        
            # Copy library dependencies from ldd
            try:
                output = subprocess.check_output(["ldd", "/bin/sleep"], text=True)
                for line in output.splitlines():
                    parts = line.strip().split("=>")
                    raw = parts[1] if len(parts) == 2 else parts[0]
                    lib_path = raw.split("(")[0].strip()
                
                    if lib_path and lib_path.startswith("/"):
                        print(f"Dependency: {lib_path}")
                        dst_path = os.path.join(rootfs, lib_path.lstrip("/"))
                        os.makedirs(os.path.dirname(dst_path), exist_ok=True)
                        try:
                            shutil.copy2(lib_path, dst_path)
                            print(f"   ✅ Copied {lib_path} -> {dst_path}")
                        except Exception as e:
                            print(f"   ❌ Failed to copy {lib_path}: {e}")
            except Exception as e:
                print(f"❌ Could not run ldd: {e}")
        
           # Dump and patch config.json
            config_path = os.path.join(bundle_path, "config.json")
            try:
                with open(config_path) as f:
                    config_content = f.read()
                    print("config.json:\n", config_content)
                    config = json.loads(config_content)
                    print("process.args:", config.get("process", {}).get("args"))
            except Exception as e:
                print(f"❌ Could not read config.json: {e}")
            else:
                # Patch config.json arch to match host (important for ubuntu-latest)
                try:
                    import platform
                    host_arch = platform.machine()
                    fixed_arch = "amd64" if host_arch == "x86_64" else host_arch
            
                    if "platform" in config:
                        old_arch = config["platform"].get("arch")
                        config["platform"]["arch"] = fixed_arch
                        with open(config_path, "w") as f:
                            json.dump(config, f, indent=2)
                        print(f"✅ Patched config.json arch: {old_arch} -> {fixed_arch}")
                    else:
                        print("⚠️ No 'platform' field found in config.json")
                except Exception as e:
                    print(f"❌ Failed to patch config.json: {e}")


        status = test_utils.run_command_line(command)
        if "started '" + container_id + "' container" not in status.stdout:
            debug_msg = (
                f"Container did not launch successfully\n"
                f"Return code: {status.returncode}\n"
                f"STDOUT:\n{status.stdout}\n"
                f"STDERR:\n{status.stderr}\n"
                f"DobbyDaemon logs:\n{test_utils.get_dobby_logs()}\n"
            )
            return False, debug_msg
        
        return validate_annotation(container_id, expected_output)


def validate_annotation(container_id, expected_output):
    """Helper function for finding if annotation is present in container info

    Parameters:
    container_id (string): name of container to run
    expected_output (string): expected key value pair annotation

    Returns:
    (pass (bool), message (string)): True if annotation is found in container info

    """
    annotateCommand = ["DobbyTool",
                        "annotate",
                        container_id,
                        "Key1",
                        "Value1"]
        
    status = test_utils.run_command_line(annotateCommand)
    if "annotate successful for container '" + container_id + "'" not in status.stdout:
        return False, "annotation failed"

    infoCommand = ["DobbyTool",
                "info",
                container_id]
        
    status = test_utils.run_command_line(infoCommand)
    
    test_utils.print_log("command returned %s" % status.stdout, test_utils.Severity.debug)

    if expected_output not in status.stdout:
        return False, "annotation is not found in container info"

    removeAnnotationCommand = ["DobbyTool",
                        "remove-annotation",
                        container_id,
                        "Key1"]
    status = test_utils.run_command_line(removeAnnotationCommand)

    test_utils.print_log("command returned %s" % status.stdout, test_utils.Severity.debug)

    if "removed Key1 key from container '" + container_id + "'" not in status.stdout:
        return False, "remove annotation failed"

    infoCommand = ["DobbyTool",
                "info",
                container_id]

    status = test_utils.run_command_line(infoCommand)

    test_utils.print_log("command returned %s" % status.stdout, test_utils.Severity.debug)

    if expected_output in status.stdout:
        return False, "annotation is still found in container info after removal"

    return True, "Test passed"
    

if __name__ == "__main__":
    test_utils.parse_arguments(__file__, True)
    execute_test()
