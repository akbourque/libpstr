#!/usr/bin/env python3
import os
import glob

def bundle_project():
    output_filename = "unified_project.code"
    # Clean up old run if it exists
    if os.path.exists(output_filename):
        os.remove(output_filename)

    # Search patterns for source and headers
    patterns = ["src/**/*.c", "src/**/*.h", "include/**/*.c", "include/**/*.h"]
    files_found = []

    for pattern in patterns:
        # recursive=True allows matching nested subdirectories automatically
        for filepath in glob.glob(pattern, recursive=True):
            if os.path.isfile(filepath):
                files_found.append(filepath)

    if not files_found:
        print("⚠️ No .c or .h files found in src/ or include/")
        return

    print(f"📦 Bundling {len(files_found)} files into {output_filename}...")

    with open(output_filename, "w", encoding="utf-8", errors="ignore") as fout:
        for filepath in sorted(files_found):
            fout.write("========================================\n")
            fout.write(f"FILE: {filepath}\n")
            fout.write("========================================\n")
            
            with open(filepath, "r", encoding="utf-8", errors="ignore") as fin:
                fout.write(fin.read())
            
            fout.write("\n\n")
            
    print("🚀 Done! Snapshot is pristine.")

if __name__ == "__main__":
    bundle_project()
