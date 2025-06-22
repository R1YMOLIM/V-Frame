import subprocess
import os
import shutil
import platform

def run_command(command, description):
    print(f"\n--- {description} ---")
    print(f"Running: {' '.join(command)}")
    try:
        process = subprocess.run(command, capture_output=True, text=True)
        if process.stdout:
            print(process.stdout)
        if process.stderr:
            print("Errors:", process.stderr)
        if process.returncode != 0:
            print(f"❌ Error: Command failed with code {process.returncode}")
            return False
        print("✅ Command succeeded.")
        return True
    except FileNotFoundError:
        print(f"❌ Command '{command[0]}' not found.")
        return False

def clear_build_dir(preset, clean_type="full"):
    build_dir = f"out/build/{preset}"
    if not os.path.exists(build_dir):
        return

    extensions_to_delete = [".dll", ".lib", ".exp", ".pdb"]

    if clean_type == "full":
        print(f"🧹 Fully clearing build directory: {build_dir}")
        shutil.rmtree(build_dir)
    elif clean_type == "files_only":
        print(f"🧹 Removing only specific files (.dll, .lib, .exp, .pdb) in: {build_dir}")
        for root, dirs, files in os.walk(build_dir):
            for file in files:
                if any(file.endswith(ext) for ext in extensions_to_delete):
                    file_path = os.path.join(root, file)
                    try:
                        os.remove(file_path)
                        print(f"Deleted file: {file_path}")
                    except Exception as e:
                        print(f"⚠️ Could not delete file {file_path}: {e}")

def copy_files(preset):
    build_dir = f"out/build/{preset}"
    dest_dir = f"bin/{'Debug' if 'debug' in preset.lower() else 'Release'}"
    os.makedirs(dest_dir, exist_ok=True)

    extensions = [".dll", ".lib", ".exp", ".pdb"]
    copied_any = False

    for ext in extensions:
        filename = f"vFrame{ext}"
        src_path = os.path.join(build_dir, filename)
        dst_path = os.path.join(dest_dir, filename)
        try:
            shutil.copy2(src_path, dst_path)
            print(f"✅ Copied {src_path} → {dst_path}")
            copied_any = True
        except FileNotFoundError:
            print(f"⚠️ File not found (skipping): {src_path}")
        except Exception as e:
            print(f"❌ Error copying {src_path}: {e}")

    if not copied_any:
        print("⚠️ No files were copied. Check if build output files exist.")


def is_tool_available(tool_name):
    """Перевіряє, чи доступна команда tool_name в PATH."""
    try:
        subprocess.run([tool_name, "--version"], capture_output=True, text=True, check=True)
        return True
    except (subprocess.CalledProcessError, FileNotFoundError):
        return False

def detect_platform_compiler():
    system = platform.system()
    if system == "Windows":
        return "windows"  # cl.exe
    elif system in ("Linux", "Darwin"):
        # Спробуємо clang
        if is_tool_available("clang"):
            return f"{system.lower()}-clang"
        # Якщо немає clang, перевіримо gcc
        elif is_tool_available("gcc"):
            return f"{system.lower()}-gcc"
        else:
            print(f"⚠️ On {system} not found gcc or clang. Please choose compilator maually.")
            while True:
                manual_choice = input("Enter 'clang' or 'gcc': ").strip().lower()
                if manual_choice in ("clang", "gcc"):
                    return f"{system.lower()}-{manual_choice}"
                print("Wrong Choose! Try again")
    else:
        return None

def build_with_preset(preset, clean_type="full"):
    clear_build_dir(preset, clean_type)
    # Configure
    if not run_command(["cmake", "--preset", preset], f"Configuring ({preset})"):
        return False
    # Build
    if not run_command(["cmake", "--build", "--preset", preset], f"Building ({preset})"):
        return False
    # Copy build artifacts
    copy_files(preset)
    return True

if __name__ == "__main__":
    print("🛠️ Select build configuration:")
    print("1 - Debug")
    print("2 - Release")
    choice = input("Enter number (1/2): ").strip()
    build_type = "debug" if choice == "1" else "release"

    print("\n🧹 Select clean type:")
    print("1 - Full clean (remove all build directory)")
    print("2 - Files only (remove only .dll, .lib, .exp, .pdb files)")
    clean_choice = input("Enter number (1/2): ").strip()
    clean_type = "full" if clean_choice == "1" else "files_only"

    platform_compiler = detect_platform_compiler()
    if platform_compiler is None:
        print("❌ Unsupported platform.")
        exit(1)

    preset = f"x64-{build_type}-{platform_compiler}"

    print(f"\n🚀 Starting build with preset: {preset}, clean type: {clean_type}")
    if build_with_preset(preset, clean_type):
        print("🎉 Build succeeded and files copied successfully!")
    else:
        print("💥 Build failed.")
