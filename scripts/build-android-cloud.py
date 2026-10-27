#!/usr/bin/env python3
"""Build with the retained cloud Android toolchain and injected HTTPS proxy.
No credentials or proxy values are printed or saved in the repository.
"""
import os
from pathlib import Path
import subprocess
import urllib.parse

project = Path(__file__).resolve().parent.parent
environ = os.environ.copy()
qt = Path('/workspace/Qt/6.8.3')
jdk = Path('/workspace/.jdk-full/usr/lib/jvm/java-21-openjdk-amd64')
sdk = Path('/workspace/android-sdk')
for file in [jdk / 'bin/javac', sdk / 'ndk/27.2.12479018/source.properties', qt / 'android_arm64_v8a/bin/qt-cmake']:
    if not file.exists():
        raise SystemExit(f'Missing toolchain file: {file}; run scripts/bootstrap-android-cloud.sh first')
environ.update(JAVA_HOME=str(jdk), ANDROID_HOME=str(sdk), ANDROID_SDK_ROOT=str(sdk),
    GRADLE_USER_HOME='/workspace/.gradle', ANDROID_USER_HOME='/workspace/.android',
    LD_LIBRARY_PATH=str(qt / 'gcc_64/lib'))
environ['PATH'] = str(jdk / 'bin') + ':/workspace/.tools-venv/bin:' + environ['PATH']
for key in ['CMAKE_PREFIX_PATH', 'QML_IMPORT_PATH', 'QML2_IMPORT_PATH', 'QT_PLUGIN_PATH']:
    environ.pop(key, None)
environ.pop('ANDROID_SDK_HOME', None)
Path('/workspace/.android').mkdir(parents=True, exist_ok=True)
gradle = Path(environ['GRADLE_USER_HOME'])
gradle.mkdir(exist_ok=True)
managed = ['org.gradle.daemon=false', 'org.gradle.workers.max=4', f'org.gradle.java.home={jdk}']
proxy = environ.get('HTTPS_PROXY') or environ.get('https_proxy')
if proxy:
    parsed = urllib.parse.urlsplit(proxy)
    if parsed.username or parsed.password or not parsed.hostname:
        raise SystemExit('This helper requires the credential-free platform proxy endpoint.')
    properties = [f'{scheme}.proxy{key}={value}' for scheme in ['http', 'https']
        for key, value in [('Host', parsed.hostname), ('Port', str(parsed.port or 80))]]
    environ['GRADLE_OPTS'] = environ.get('GRADLE_OPTS', '') + ' ' + ' '.join('-D' + p for p in properties)
    managed.extend('systemProp.' + p for p in properties)
config = gradle / 'gradle.properties'
old = config.read_text().splitlines() if config.exists() else []
keys = {line.split('=', 1)[0] for line in managed}
old = [line for line in old if line.split('=', 1)[0] not in keys]
config.write_text('\n'.join(old + managed) + '\n')
subprocess.run([str(qt / 'android_arm64_v8a/bin/qt-cmake'), '-S', str(project), '-B', str(project / 'build-android'),
    '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Debug', '-DBUILD_TESTING=OFF', '-DANDROID_ABI=arm64-v8a',
    '-DANDROID_PLATFORM=android-26', f'-DANDROID_SDK_ROOT={sdk}',
    f'-DANDROID_NDK_ROOT={sdk}/ndk/27.2.12479018', f'-DQT_HOST_PATH={qt}/gcc_64',
    '-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON'], env=environ, check=True)
subprocess.run(['cmake', '--build', str(project / 'build-android'), '--target', 'apk', '--parallel', '4'],
    env=environ, check=True)
