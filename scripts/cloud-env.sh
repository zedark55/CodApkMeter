# Source this file from Bash to activate the prepared Debian cloud toolchain.
export PATH="/workspace/.tools-venv/bin:$PATH"
export CMAKE_PREFIX_PATH="/workspace/.qt/usr${CMAKE_PREFIX_PATH:+:$CMAKE_PREFIX_PATH}"
export LD_LIBRARY_PATH="/workspace/.qt/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export QML_IMPORT_PATH="/workspace/.qt/usr/lib/x86_64-linux-gnu/qt6/qml:/usr/lib/x86_64-linux-gnu/qt6/qml${QML_IMPORT_PATH:+:$QML_IMPORT_PATH}"
export XDG_CACHE_HOME="${XDG_CACHE_HOME:-/workspace/.cache}"
