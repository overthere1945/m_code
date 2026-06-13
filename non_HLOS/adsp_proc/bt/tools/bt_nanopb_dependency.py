#===============================================================================
# Script to configure nanopb dependency on BT PAL layer.
#
# GENERAL DESCRIPTION
#    Script to configure nanopb dependecy for BT PAL layer.
#
# @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# All Rights Reserved.
# Confidential and Proprietary - Qualcomm Technologies, Inc.
#
#===============================================================================
import sys
import os
import platform
import tarfile
import zipfile
import shutil
import subprocess
import argparse

# --- Configuration for Nanopb ---
NANOPB_VERSION = "0.4.8" # Target Nanopb version
# --- End Configuration ---

def ensure_nanopb_from_local(nanopb_install_dir, local_package_path):
    if not local_package_path:
        print("[ERROR] No local Nanopb package path provided.")
        return False

    if os.path.exists(nanopb_install_dir):
        try:
            shutil.rmtree(nanopb_install_dir)
        except Exception as e:
            if "Windows" in platform.system() or "CYGWIN" in platform.system():
                os.system('rmdir /S /Q "{}"'.format(nanopb_install_dir))
            else:
                print(f"[ERROR] Failed to remove {nanopb_install_dir}: {e}")
                return False

    os.makedirs(nanopb_install_dir, exist_ok=True)

    lower = local_package_path.lower()
    if lower.endswith(".zip"):
        archive_type = "zip"
    else:
        archive_type = "tar.gz"

    try:
        if archive_type == "zip":
            with zipfile.ZipFile(local_package_path, 'r') as zip_ref:
                zip_ref.extractall(nanopb_install_dir)
        else:
            with tarfile.open(local_package_path, 'r:gz') as tar_ref:
                tar_ref.extractall(nanopb_install_dir)

        extracted_sub_dir_name = f"nanopb-{NANOPB_VERSION}-linux-x86"
        if "Windows" in platform.system() or "CYGWIN" in platform.system():
            extracted_sub_dir_name = f"nanopb-{NANOPB_VERSION}-windows-x86"

        extracted_sub_dir = os.path.join(nanopb_install_dir, extracted_sub_dir_name)
        if os.path.isdir(extracted_sub_dir):
            for item in os.listdir(extracted_sub_dir):
                shutil.move(os.path.join(extracted_sub_dir, item), nanopb_install_dir)
            shutil.rmtree(extracted_sub_dir)

        return True

    except (tarfile.ReadError, zipfile.BadZipFile) as e:
        print(f"[ERROR] Failed to extract Nanopb package: {e}")
        return False
    except Exception as e:
        print(f"[ERROR] Unexpected error during extraction: {e}")
        return False

def organize_and_cleanup_nanopb_installation(nanopb_install_dir):
    """
    Organizes .proto files and core Nanopb runtime files into a flat structure
    and performs aggressive cleanup, keeping only essential generator tools,
    the new 'pb' folder, and the 'src'/'inc' folders.
    """
    print(f"[INFO] Organizing and cleaning up Nanopb installation...")

    src_dir = os.path.join(nanopb_install_dir, "src")
    inc_dir = os.path.join(nanopb_install_dir, "inc")
    pb_dir = os.path.join(nanopb_install_dir, "pb")
    
    os.makedirs(src_dir, exist_ok=True)
    os.makedirs(inc_dir, exist_ok=True)
    os.makedirs(pb_dir, exist_ok=True)

    # --- Move and Flatten .proto files into 'pb' directory ---
    # For this package, protos are typically in generator-bin/proto/
    proto_source_roots = [
        os.path.join(nanopb_install_dir, "generator-bin", "proto"),
    ]
    
    for source_root in proto_source_roots:
        if os.path.exists(source_root):
            for root, _, files in os.walk(source_root):
                for file in files:
                    if file.endswith(".proto"):
                        full_proto_path = os.path.join(root, file)
                        destination_path = os.path.join(pb_dir, file) # Just use base filename
                        try:
                            if not os.path.exists(destination_path):
                                shutil.move(full_proto_path, destination_path)
                        except Exception as e:
                            print(f"[WARNING] Could not move {full_proto_path}: {e}")
            # Clean up original proto source directory after moving files
            try:
                if os.path.exists(source_root) and os.path.isdir(source_root) and not os.listdir(source_root):
                    shutil.rmtree(source_root)
            except Exception as e:
                print(f"[WARNING] Could not remove original proto source directory {source_root}: {e}")


    # --- Attempt to Move Core Nanopb Runtime Files to src/inc ---
    # These files are often NOT present in the nanopb-0.4.8-linux-x86.tar.gz package
    # Warnings will be printed if they are not found.
    core_src_files = ["pb_common.c", "pb_decode.c", "pb_encode.c"]
    core_inc_files = ["pb_common.h", "pb_decode.h", "pb_encode.h", "pb.h"]

    for f_name in core_src_files:
        src_path = os.path.join(nanopb_install_dir, f_name) # Look directly in the root of the extracted package
        dest_path = os.path.join(src_dir, f_name)
        if os.path.exists(src_path) and not os.path.exists(dest_path):
            try:
                shutil.move(src_path, dest_path)
            except Exception as e:
                print(f"[WARNING] Could not move core source file {src_path}: {e}")
        elif not os.path.exists(src_path):
            print(f"[WARNING] Core source file not found at expected location: {src_path}. This might mean your Nanopb package is missing core runtime files.")

    for f_name in core_inc_files:
        src_path = os.path.join(nanopb_install_dir, f_name) # Look directly in the root of the extracted package
        dest_path = os.path.join(inc_dir, f_name)
        if os.path.exists(src_path) and not os.path.exists(dest_path):
            try:
                shutil.move(src_path, dest_path)
            except Exception as e:
                print(f"[WARNING] Could not move core header file {src_path}: {e}")
        elif not os.path.exists(src_path):
            print(f"[WARNING] Core header file not found at expected location: {src_path}. This might mean your Nanopb package is missing core runtime files.")


    # --- Aggressive Cleanup of Non-Essential Top-Level Items ---
    essential_items_at_top_level = [
        "generator",    # Contains nanopb_generator.py and protoc binary folder
        "generator-bin", # May contain other tools besides 'proto' directory (e.g. grpc_tools)
        "src",          # Where generated/core .c files will reside
        "inc",          # Where generated/core .h files will reside
        "pb",           # Where .proto files will reside
    ]

    all_items_in_root = os.listdir(nanopb_install_dir)

    for item in all_items_in_root:
        item_path = os.path.join(nanopb_install_dir, item)
        if item not in essential_items_at_top_level:
            try:
                if os.path.isdir(item_path):
                    shutil.rmtree(item_path)
                else:
                    os.remove(item_path)
            except Exception as e:
                print(f"[WARNING] Could not remove {item_path}: {e}")
    
    print("[INFO] File organization and cleanup complete.")


def generate_nanopb_c_files(nanopb_install_dir):
    """
    Generates C header and source files for nanopb.proto and descriptor.proto
    using the protoc --plugin=protoc-gen-nanopb method.
    Protoc and protoc-gen-nanopb are explicitly sourced from the 'generator-bin' directory.
    """
    print(f"[INFO] Starting C file generation using protoc --plugin method (from generator-bin directly)...")

    if "Windows" in platform.system() or "CYGWIN" in platform.system():
        protoc_executable_name = "protoc.exe"
        protoc_plugin_name = "protoc-gen-nanopb.exe"
    else:
        protoc_executable_name = "protoc"
        protoc_plugin_name = "protoc-gen-nanopb"

    # Absolute paths to protoc and protoc-gen-nanopb, explicitly from 'generator-bin'
    protoc_abs_path = os.path.join(nanopb_install_dir, "generator-bin", protoc_executable_name)
    protoc_gen_nanopb_abs_path = os.path.join(nanopb_install_dir, "generator-bin", protoc_plugin_name)

    # Verify absolute paths exist before proceeding
    if not os.path.exists(protoc_abs_path):
        print(f"[ERROR] Protoc executable not found at expected path: {protoc_abs_path}")
        return False
    if not os.path.exists(protoc_gen_nanopb_abs_path):
        print(f"[ERROR] protoc-gen-nanopb plugin not found at expected path: {protoc_gen_nanopb_abs_path}")
        return False

    # Ensure execute permissions for protoc and protoc-gen-nanopb (only relevant for non-Windows)
    if not (platform.system() == "Windows" or "CYGWIN" in platform.system()):
        try:
            os.chmod(protoc_abs_path, 0o755)
            os.chmod(protoc_gen_nanopb_abs_path, 0o755)
        except Exception as e:
            print(f"[WARNING] Could not set execute permissions for protoc or protoc-gen-nanopb: {e}")

    # Absolute path for temporary output directory
    temp_output_dir = os.path.join(nanopb_install_dir, "generated_pb_temp")
    
    final_src_dir = os.path.join(nanopb_install_dir, "src")
    final_inc_dir = os.path.join(nanopb_install_dir, "inc")

    os.makedirs(temp_output_dir, exist_ok=True)

    # Process both nanopb.proto and descriptor.proto
    proto_files_to_process = ["nanopb.proto", "descriptor.proto"] 

    overall_generation_success = True
    for proto_file_name in proto_files_to_process:
        # Absolute path to the directory containing the .proto files (for --proto_path)
        proto_include_path = os.path.join(nanopb_install_dir, "pb")

        # The input .proto file name (relative to --proto_path)
        input_proto_file_relative = proto_file_name

        # Verify the absolute path to the input proto file exists
        if not os.path.exists(os.path.join(proto_include_path, input_proto_file_relative)):
            print(f"[WARNING] .proto file not found at expected path: {os.path.join(proto_include_path, input_proto_file_relative)}, skipping generation.")
            overall_generation_success = False
            continue

        # Construct the command list for subprocess.run
        command = [
            protoc_abs_path,
            f"--plugin=protoc-gen-nanopb={protoc_gen_nanopb_abs_path}",
            f"--proto_path={proto_include_path}", # Absolute include path
            f"--nanopb_out={temp_output_dir}", # Absolute output path
            input_proto_file_relative # Input proto file relative to --proto_path
        ]
        
        print(f"[INFO] Executing command from CWD ({nanopb_install_dir}): {' '.join(command)}") # Print the exact command
        
        try:
            # Execute command using subprocess.run, allowing direct output to console.
            result = subprocess.run(command, cwd=nanopb_install_dir)
            
            # Check for the *existence* of the generated files to confirm success.
            expected_h_file = os.path.join(temp_output_dir, f"{os.path.splitext(proto_file_name)[0]}.pb.h")
            expected_c_file = os.path.join(temp_output_dir, f"{os.path.splitext(proto_file_name)[0]}.pb.c")

            if os.path.exists(expected_h_file) and os.path.exists(expected_c_file):
                print(f"[INFO] Successfully generated temporary C files for {os.path.basename(proto_file_name)}.")
                if result.returncode != 0:
                    print(f"[WARNING] Protoc command for {os.path.basename(proto_file_name)} exited with non-zero code {result.returncode}, but files were generated. (Likely a harmless plugin warning).")
            else:
                # If files are not found, then it's a real failure
                print(f"[ERROR] Failed to generate C files for {os.path.basename(proto_file_name)}. Expected output files not found.")
                print(f"Protoc command exited with code {result.returncode}. Please check the console output for details.")
                overall_generation_success = False
                # If one file fails, the overall process fails
                break 

        except FileNotFoundError:
            print(f"[ERROR] Protoc or protoc-gen-nanopb not found. Check paths.")
            overall_generation_success = False
            break
        except Exception as e:
            print(f"[ERROR] An unexpected error occurred during generation for {os.path.basename(proto_file_name)}: {e}")
            overall_generation_success = False
            break
    
    if overall_generation_success:
        print(f"[INFO] Moving generated files to final directories.")
        try:
            for root, _, files in os.walk(temp_output_dir):
                for file in files:
                    full_temp_path = os.path.join(root, file)
                    
                    if file.endswith('.c'):
                        final_dest_path = os.path.join(final_src_dir, os.path.basename(file))
                    elif file.endswith('.h'):
                        final_dest_path = os.path.join(final_inc_dir, os.path.basename(file)) 
                    else:
                        continue
                    
                    os.makedirs(os.path.dirname(final_dest_path), exist_ok=True)
                    shutil.move(full_temp_path, final_dest_path)

            if os.path.exists(temp_output_dir):
                shutil.rmtree(temp_output_dir)

        except Exception as e:
            print(f"[ERROR] An error occurred during file movement or cleanup: {e}")
            overall_generation_success = False

    print("[INFO] C file generation and organization complete.")
    return overall_generation_success

def edit_nanopb_header_for_dependency(nanopb_install_dir):
    """
    Edits nanopb.pb.h to include descriptor.pb.h, which is a dependency.
    """
    nanopb_header_path = os.path.join(nanopb_install_dir, "inc", "nanopb.pb.h")
    descriptor_include_line = '#include "descriptor.pb.h"\n'
    
    if not os.path.exists(nanopb_header_path):
        print(f"[WARNING] nanopb.pb.h not found at {nanopb_header_path}. Cannot add descriptor.pb.h include.")
        return False

    print(f"[INFO] Modifying {nanopb_header_path} to include descriptor.pb.h...")
    
    try:
        with open(nanopb_header_path, 'r') as f:
            lines = f.readlines()

        # Find a suitable place to insert the include.
        # After #include <pb.h> is a good spot, or at the very beginning.
        insert_index = -1
        for i, line in enumerate(lines):
            if '#include <pb.h>' in line:
                insert_index = i + 1
                break
        
        if insert_index == -1: # If pb.h not found, insert at top after initial comments/defines
            # Find first non-comment/non-define line to insert after
            for i, line in enumerate(lines):
                if not (line.strip().startswith('/*') or line.strip().startswith('*') or line.strip().startswith('#')):
                    insert_index = i
                    break
            if insert_index == -1: # Fallback to very end if file is just comments/defines
                insert_index = len(lines)

        # Check if it's already included to prevent duplicates
        if descriptor_include_line.strip() in [line.strip() for line in lines]:
            print(f"[INFO] {descriptor_include_line.strip()} already found in {nanopb_header_path}. Skipping insertion.")
            return True

        lines.insert(insert_index, descriptor_include_line)

        with open(nanopb_header_path, 'w') as f:
            f.writelines(lines)
        
        print(f"[INFO] Successfully added {descriptor_include_line.strip()} to {nanopb_header_path}.")
        return True
    except Exception as e:
        print(f"[ERROR] Failed to modify {nanopb_header_path}: {e}")
        return False


if __name__ == "__main__":
    script_dir = os.path.dirname(os.path.abspath(__file__))
    nanopb_install_dir = os.path.abspath(os.path.join(script_dir, os.pardir, "nanopb"))

    parser = argparse.ArgumentParser(
        description="Configure Nanopb dependency from a local archive (no download)."
    )
    parser.add_argument(
        "-f", "--file",
        required=True,
        help=(
            "Nanopb package base name in bt/tools (with or without extension), "
            "e.g. nanopb-0.4.8-linux-x86 for bt/tools/nanopb-0.4.8-linux-x86.tar.gz"
        ),
    )
    args = parser.parse_args()

    tools_dir = script_dir
    input_name = args.file

    candidates = [
        os.path.join(tools_dir, input_name),
        os.path.join(tools_dir, input_name + ".tar.gz"),
        os.path.join(tools_dir, input_name + ".zip"),
    ]

    local_pkg_path = None
    for c in candidates:
        if os.path.exists(c):
            local_pkg_path = c
            break

    if not local_pkg_path:
        print(f"[ERROR] Nanopb package not found in {tools_dir}. Tried:")
        for c in candidates:
            print(f"  {os.path.basename(c)}")
        sys.exit(1)

    if not ensure_nanopb_from_local(nanopb_install_dir, local_pkg_path):
        print("[ERROR] Failed to extract Nanopb from local package.")
        sys.exit(1)

    organize_and_cleanup_nanopb_installation(nanopb_install_dir)

    success = generate_nanopb_c_files(nanopb_install_dir)

    if success:
        if not edit_nanopb_header_for_dependency(nanopb_install_dir):
            print("[WARN] Failed to edit nanopb.pb.h for dependency.")
        sys.exit(0)
    else:
        print("[ERROR] Nanopb C file generation and organization failed.")
        sys.exit(1)