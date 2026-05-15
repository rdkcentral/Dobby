#!/usr/bin/env python3
"""
Script to regenerate L2 test bundles for cgroupv2 compatibility.

This script:
1. Extracts each .tar.gz bundle
2. Patches config.json to remove cgroupv2-incompatible settings
3. Repacks the bundle

Changes made for cgroupv2 compatibility:
- Removes 'swappiness' from memory resources (not supported in cgroupv2)
- Sets realtimeRuntime and realtimePeriod to valid values or removes them
- Updates rootfsPropagation to 'slave' for better compatibility
"""

import json
import shutil
import sys
import tarfile
from pathlib import Path


def patch_config_for_cgroupv2(config: dict, bundle_name: str = "") -> dict:
    """Patch OCI config.json for cgroupv2 compatibility."""
    
    # Remove swappiness from memory resources (not supported in cgroupv2)
    if 'linux' in config and 'resources' in config['linux']:
        resources = config['linux']['resources']
        
        if 'memory' in resources:
            memory = resources['memory']
            if 'swappiness' in memory:
                del memory['swappiness']
                print("  - Removed 'swappiness' from memory resources")
        
        # Fix cpu realtime settings - remove null values
        if 'cpu' in resources:
            cpu = resources['cpu']
            if cpu.get('realtimeRuntime') is None:
                del cpu['realtimeRuntime']
                print("  - Removed null 'realtimeRuntime'")
            if cpu.get('realtimePeriod') is None:
                del cpu['realtimePeriod']
                print("  - Removed null 'realtimePeriod'")
            # Remove cpu section entirely if empty
            if not cpu:
                del resources['cpu']
                print("  - Removed empty 'cpu' section")
    
    # Remove rootfsPropagation entirely - it causes "make rootfs private" errors
    # in user namespace environments like GitHub Actions
    if 'linux' in config and 'rootfsPropagation' in config['linux']:
        del config['linux']['rootfsPropagation']
        print("  - Removed linux.rootfsPropagation")
    
    # Remove top-level rootfsPropagation as well
    if 'rootfsPropagation' in config:
        del config['rootfsPropagation']
        print("  - Removed top-level rootfsPropagation")
    
    # Remove user namespace - causes issues in GitHub Actions which already uses user namespaces
    if 'linux' in config:
        # Remove uidMappings and gidMappings
        if 'uidMappings' in config['linux']:
            del config['linux']['uidMappings']
            print("  - Removed uidMappings")
        if 'gidMappings' in config['linux']:
            del config['linux']['gidMappings']
            print("  - Removed gidMappings")
        
        # Remove 'user' from namespaces list
        if 'namespaces' in config['linux']:
            namespaces = config['linux']['namespaces']
            original_len = len(namespaces)
            config['linux']['namespaces'] = [ns for ns in namespaces if ns.get('type') != 'user']
            if len(config['linux']['namespaces']) < original_len:
                print("  - Removed 'user' namespace")
    
    # Fix filelogging bundle - needs terminal: true for logging plugin to capture stdout
    if 'filelogging' in bundle_name:
        if 'process' in config:
            if not config['process'].get('terminal', False):
                config['process']['terminal'] = True
                print("  - Set 'terminal' to true for logging plugin stdout capture")
    
    return config


def process_bundle(bundle_tarball: Path, backup: bool = True):
    """Extract, patch, and repack a bundle tarball."""
    
    print(f"\nProcessing: {bundle_tarball.name}")
    
    bundle_dir = bundle_tarball.parent
    bundle_name = bundle_tarball.name.replace('.tar.gz', '')
    extract_path = bundle_dir / bundle_name
    
    # Backup original
    if backup:
        backup_path = bundle_tarball.with_suffix('.tar.gz.bak')
        if not backup_path.exists():
            shutil.copy2(bundle_tarball, backup_path)
            print(f"  Backed up to: {backup_path.name}")
    
    # Clean up any stale extraction directory left behind by a prior run
    # to avoid mixing old files into the repacked bundle.
    if extract_path.exists():
        print(f"  Removing stale extraction directory: {extract_path.name}")
        shutil.rmtree(extract_path)

    # Extract (with path-traversal protection)
    print(f"  Extracting...")
    with tarfile.open(bundle_tarball, 'r:gz') as tar:
        # Reject members that escape the target directory via absolute paths
        # or '..' components to prevent path-traversal attacks.
        for member in tar.getmembers():
            member_path = (bundle_dir / member.name).resolve()
            if not str(member_path).startswith(str(bundle_dir.resolve())):
                raise RuntimeError(
                    f"Tarball member '{member.name}' would escape extraction "
                    f"directory '{bundle_dir}' — aborting for safety"
                )
        tar.extractall(path=bundle_dir)
    
    # Find and patch config.json
    config_path = extract_path / 'config.json'
    if not config_path.exists():
        print(f"  ERROR: config.json not found at {config_path}")
        return False
    
    print(f"  Patching config.json...")
    with open(config_path, 'r') as f:
        config = json.load(f)
    
    patched_config = patch_config_for_cgroupv2(config, bundle_name)
    
    with open(config_path, 'w') as f:
        json.dump(patched_config, f, indent=4)
    
    # Repack
    print(f"  Repacking...")
    with tarfile.open(bundle_tarball, 'w:gz') as tar:
        tar.add(extract_path, arcname=bundle_name)
    
    # Cleanup extracted folder
    shutil.rmtree(extract_path)
    print(f"  Done!")
    
    return True


def main():
    bundle_dir = Path(__file__).parent
    
    # Find all bundle tarballs
    bundles = list(bundle_dir.glob('*_bundle.tar.gz'))
    
    if not bundles:
        print("No bundles found!")
        return 1
    
    print(f"Found {len(bundles)} bundles to process:")
    for b in bundles:
        print(f"  - {b.name}")
    
    # Process each bundle
    success_count = 0
    for bundle in bundles:
        try:
            if process_bundle(bundle):
                success_count += 1
        except Exception as e:
            print(f"  ERROR processing {bundle.name}: {e}")
    
    print(f"\n{'='*50}")
    print(f"Processed {success_count}/{len(bundles)} bundles successfully")
    
    return 0 if success_count == len(bundles) else 1


if __name__ == '__main__':
    sys.exit(main())

