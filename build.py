
import subprocess
import os
import sys

# Налаштування
build_dir = "build"
cmake_args = ["-DCMAKE_BUILD_TYPE=Release"]

os.makedirs(build_dir, exist_ok=True)

print("🔧 Start CMake Generation...")
result = subprocess.run(["cmake", "-B", build_dir] + cmake_args, capture_output=True, text=True)
if result.returncode != 0:
    print("❌ Помилка генерації CMake:")
    print(result.stderr)
    sys.exit(1)

print("🔨 Build Project...")
result = subprocess.run(["cmake", "--build", build_dir], capture_output=True, text=True)
if result.returncode != 0:
    print("❌ Error while build: ")
    print(result.stderr)
    sys.exit(1)

print("✅ Build exit successful!")

