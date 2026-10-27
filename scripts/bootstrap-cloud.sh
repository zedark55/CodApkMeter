#!/usr/bin/env bash
# Non-root Debian 13 setup; never changes application sources or system directories.
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
tools_dir=/workspace/.tools-venv
apt_dir=/workspace/.apt
qt_dir=/workspace/.qt
mkdir -p "$apt_dir/lists/partial" "$apt_dir/archives/partial" "$qt_dir"
if [[ ! -x "$tools_dir/bin/python" ]]; then python3 -m venv "$tools_dir"; fi
"$tools_dir/bin/pip" install --disable-pip-version-check 'cmake==3.31.6' 'ninja==1.11.1.4'
cat > "$apt_dir/sources.list" <<'SOURCES'
deb [signed-by=/usr/share/keyrings/debian-archive-keyring.gpg] https://deb.debian.org/debian trixie main
deb [signed-by=/usr/share/keyrings/debian-archive-keyring.gpg] https://security.debian.org/debian-security trixie-security main
SOURCES
apt_options=(-o "Dir::Etc::sourcelist=$apt_dir/sources.list" -o Dir::Etc::sourceparts=-
  -o Dir::Etc::parts=- -o "Dir::State::lists=$apt_dir/lists"
  -o "Dir::Cache::archives=$apt_dir/archives" -o APT::Sandbox::User=agent -o Debug::NoLocking=true)
/usr/bin/apt-get "${apt_options[@]}" update
/usr/bin/apt-get "${apt_options[@]}" --download-only --no-install-recommends -y install \
  qt6-base-dev qt6-declarative-dev qt6-connectivity-dev \
  qml6-module-qtquick-controls qml6-module-qtquick-layouts qml6-module-qtquick-window \
  qml6-module-qtqml-workerscript qml6-module-qtquick-templates
for archive in "$apt_dir"/archives/*.deb; do dpkg-deb -x "$archive" "$qt_dir"; done
# The base image supplies Qt runtime packages. Merge references to these retained
# system files with locally extracted development packages, without overwriting.
python3 - <<'PY'
from pathlib import Path
prefix=Path('/workspace/.qt')
libs=prefix/'usr/lib/x86_64-linux-gnu'
for p in Path('/usr/lib/x86_64-linux-gnu').glob('libQt6*'):
    q=libs/p.name
    if not q.exists() and not q.is_symlink(): q.symlink_to(p)
for p in list(libs.glob('*.so*')):
    if p.is_symlink() and not p.exists():
        target=p.resolve(); original=Path('/usr/lib/x86_64-linux-gnu')/target.name
        if original.exists() and not target.exists(): target.symlink_to(original)
for source in [Path('/usr/lib/x86_64-linux-gnu/qt6/qml'),Path('/usr/lib/x86_64-linux-gnu/qt6/plugins')]:
    if not source.is_dir(): continue
    for item in source.rglob('*'):
        dest=prefix/item.relative_to('/')
        if dest.exists() or dest.is_symlink(): continue
        if item.is_dir(): dest.mkdir(parents=True,exist_ok=True)
        else:
            dest.parent.mkdir(parents=True,exist_ok=True)
            dest.symlink_to(item)
PY
source "$project_dir/scripts/cloud-env.sh"
cd "$project_dir"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -U FETCHCONTENT_SOURCE_DIR_MBEDTLS
cmake --build build --target codapkmeter protocol_tests backend_tests --parallel 4
ctest --test-dir build --output-on-failure
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software ./build/codapkmeter --demo --mobile --smoke-test
