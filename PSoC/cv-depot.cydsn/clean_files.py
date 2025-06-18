#!/usr/bin/env python3

import sys
import glob
import os

def normalize_lines(file_path):
    with open(file_path, 'r', newline='') as f:
        lines = f.readlines()

    processed_lines = []
    for line in lines:
        line = line.rstrip()  # Remove trailing whitespace
        processed_lines.append(line + '\r\n')  # Ensure CR+LF

    with open(file_path, 'w', newline='') as f:
        f.writelines(processed_lines)

def main():
    if len(sys.argv) < 2:
        print("Usage: python normalize_lines.py <files...>")
        sys.exit(1)

    for arg in sys.argv[1:]:
        for file_path in glob.glob(arg, recursive=True):
            if os.path.isfile(file_path):
                try:
                    normalize_lines(file_path)
                    print(f"Processed: {file_path}")
                except Exception as e:
                    print(f"Error processing {file_path}: {e}")

if __name__ == "__main__":
    main()
