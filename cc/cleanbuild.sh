#!/bin/bash

# Function to recursively delete files with .md5 extension
delete_md5_files() {
  for file in "$1"/*; do
    if [[ -d "$file" ]]; then
      delete_md5_files "$file"  # Recursively delete in subdirectories
    elif [[ "$file" == *.md5 ]]; then
      echo "Deleting: $file"
      rm "$file"
    fi
  done
}

# Main script starts here
echo "Cleaning .md5 files in the current directory and its subdirectories..."
delete_md5_files .

echo "Cleanup complete!"
