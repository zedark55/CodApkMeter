#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
if [[ ! -x /workspace/.tools-venv/bin/cmake || ! -f /workspace/.apt/sources.list ]]; then
  bash "$project_dir/scripts/bootstrap-cloud.sh"
fi
if [[ ! -x /workspace/.jdk-full/usr/lib/jvm/java-21-openjdk-amd64/bin/javac ]]; then
  apt_options=(-o Dir::Etc::sourcelist=/workspace/.apt/sources.list -o Dir::Etc::sourceparts=-
    -o Dir::Etc::parts=- -o Dir::State::lists=/workspace/.apt/lists
    -o Dir::Cache::archives=/workspace/.apt/archives -o APT::Sandbox::User=agent -o Debug::NoLocking=true)
  /usr/bin/apt-get "${apt_options[@]}" --download-only --reinstall --no-install-recommends -y install openjdk-21-jre-headless openjdk-21-jdk-headless
  mkdir -p /workspace/.jdk-full
  for archive in /workspace/.apt/archives/openjdk-21-jre-headless_*.deb /workspace/.apt/archives/openjdk-21-jdk-headless_*.deb; do
    dpkg-deb -x "$archive" /workspace/.jdk-full
  done
fi
/workspace/.tools-venv/bin/pip install --disable-pip-version-check 'aqtinstall==3.3.0'
# aqt 3.3 initializes its default temporary folder before reading --config.
# Set the supported local installer paths inside this private tool installation.
/workspace/.tools-venv/bin/python - <<'PY'
from configparser import ConfigParser
from pathlib import Path
p=Path('/workspace/.tools-venv/lib/python3.12/site-packages/aqt/settings.ini')
c=ConfigParser(); c.read(p)
c['qtofficial']['temp_path']='/workspace/.aqt-tmp'
c['qtofficial']['cache_path']='/workspace/.aqt-cache'
with p.open('w') as f: c.write(f)
PY
if [[ ! -f /workspace/Qt/6.8.3/android_arm64_v8a/lib/cmake/Qt6Bluetooth/Qt6BluetoothConfig.cmake || ! -x /workspace/Qt/6.8.3/gcc_64/libexec/moc ]]; then
  /workspace/.tools-venv/bin/aqt install-qt linux android 6.8.3 android_arm64_v8a \
    -O /workspace/Qt -m qtconnectivity --autodesktop -b https://qt.mirror.constant.com
fi
python3 - <<'PY'
import hashlib,subprocess,urllib.request,shutil
from pathlib import Path
sdk=Path('/workspace/android-sdk')
# Expected archive digests are from Google's HTTPS repository2-3.xml catalog.
items=[
 ('platform-35_r02.zip','0bb560a90a7a2cbd0dd8348224d518b638fe7949','android-35','platforms/android-35'),
 ('build-tools_r35_linux.zip','2cfaa0bbb2336e9ec18ed3ecea84fa2e2af607bc','android-15','build-tools/35.0.0'),
 ('android-ndk-r27c-linux.zip','090e8083a715fdb1a3e402d0763c388abb03fb4e','android-ndk-r27c','ndk/27.2.12479018'),
 ('commandlinetools-linux-13114758_latest.zip','5fdcc763663eefb86a5b8879697aa6088b041e70','cmdline-tools','cmdline-tools/19.0'),
 ('platform-tools_r37.0.1-linux.zip','477254aa5f903c15cf51001717bdf347fb6b53e0','platform-tools','platform-tools')]
for name,expected,folder,target in items:
    dest=sdk/target
    if (dest/'source.properties').is_file(): continue
    archive=Path('/workspace')/name
    if not archive.exists(): urllib.request.urlretrieve('https://dl.google.com/android/repository/'+name,archive)
    digest=hashlib.sha1()
    with archive.open('rb') as f:
        for chunk in iter(lambda:f.read(1024*1024),b''): digest.update(chunk)
    if digest.hexdigest()!=expected: raise SystemExit('Archive checksum mismatch: '+name)
    unpack=Path('/workspace/android-toolchain-unpack')/archive.stem
    subprocess.run(['unzip','-qo',str(archive),'-d',str(unpack)],check=True)
    dest.parent.mkdir(parents=True,exist_ok=True)
    if dest.exists(): raise SystemExit('Preserve and inspect incomplete SDK directory before retry: '+str(dest))
    shutil.move(str(unpack/folder),str(dest))
PY
export JAVA_HOME=/workspace/.jdk-full/usr/lib/jvm/java-21-openjdk-amd64
export PATH="$JAVA_HOME/bin:$PATH"
export ANDROID_USER_HOME=/workspace/.android
unset ANDROID_SDK_HOME
mkdir -p "$ANDROID_USER_HOME"
python3 - <<'PY'
import os,subprocess,urllib.parse
args=['/workspace/android-sdk/cmdline-tools/19.0/bin/sdkmanager','--sdk_root=/workspace/android-sdk']
proxy=os.environ.get('HTTPS_PROXY') or os.environ.get('https_proxy')
if proxy:
    p=urllib.parse.urlsplit(proxy)
    if p.username or p.password or not p.hostname: raise SystemExit('A credential-free platform proxy endpoint is required')
    args += ['--proxy=http','--proxy_host='+p.hostname,'--proxy_port='+str(p.port or 80)]
subprocess.run(args+['--licenses'],input='y\n'*100,text=True,check=True)
PY
python3 "$project_dir/scripts/build-android-cloud.py"
