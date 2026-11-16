#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""User-only installation and reversible per-device activation."""
import argparse
import base64
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import signal
import subprocess
import tempfile
import time

PLUGIN = 'kdeconnect_richnotifications'
STOCK = 'kdeconnect_sendnotifications'
STATE = Path.home() / '.local/state/kdeconnect-rich-notifications/install.json'


def run(*args):
    return subprocess.check_output(args, text=True).strip()


def write(path, data, mode=0o644):
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=path.parent, delete=False) as temporary:
        temporary.write(data)
        name = temporary.name
    os.chmod(name, mode)
    os.replace(name, path)


def fingerprint(data):
    return hashlib.sha256(data).hexdigest()


def install(build, replace=False):
    compatibility = json.loads((build / 'compatibility.json').read_text())
    version = re.search(r'\d+\.\d+\.\d+', run('kdeconnect-cli', '--version'))
    if not version or version.group() != compatibility['kdeconnect']:
        raise RuntimeError('KDE Connect version differs from build headers. Rebuild against the installed version.')
    plugin = (build / (PLUGIN + '.so')).read_bytes()
    dependencies = run('ldd', str(build / (PLUGIN + '.so')))
    if 'not found' in dependencies:
        raise RuntimeError(dependencies)
    home = Path.home()
    local = home / '.local'
    root = local / 'lib/qt6/plugins'
    launcher = local / 'libexec/kdeconnect-rich-notifications'
    daemon = shutil.which('kdeconnectd')
    if not daemon:
        raise RuntimeError('kdeconnectd must be on PATH')
    # A launcher scopes plugin discovery to KDE Connect rather than all Qt apps.
    script = ('#!/bin/sh\nexport QT_PLUGIN_PATH=' + shlex.quote(str(root))
              + '${QT_PLUGIN_PATH:+:"$QT_PLUGIN_PATH"}\nexec ' + shlex.quote(daemon) + ' "$@"\n')
    quoted_launcher = '"' + str(launcher).replace('\\', '\\\\').replace('"', '\\"') + '"'
    source = Path(__file__).resolve().parent.parent
    files = {
        root / 'kdeconnect' / (PLUGIN + '.so'): (plugin, 0o755),
        local / 'share/kdeconnect' / (PLUGIN + '_config.qml'): ((source / 'src' / (PLUGIN + '_config.qml')).read_bytes(), 0o644),
        launcher: (script.encode(), 0o755),
        local / 'share/dbus-1/services/org.kde.kdeconnect.service': (
            ('[D-BUS Service]\nName=org.kde.kdeconnect\nExec=' + quoted_launcher + '\n').encode(), 0o644),
        home / '.config/autostart/org.kde.kdeconnect.daemon.desktop': (
            ('[Desktop Entry]\nType=Application\nName=KDE Connect\nExec=' + quoted_launcher
             + '\nTerminal=false\nX-GNOME-Autostart-enabled=true\n').encode(), 0o644),
    }
    state = json.loads(STATE.read_text()) if STATE.exists() else {'files': {}, 'devices': {}}
    for path in files:
        item = state['files'].get(str(path))
        if path.exists() and item and fingerprint(path.read_bytes()) != item['installed']:
            raise RuntimeError(f'Refusing to overwrite your modified file: {path}')
        if path.exists() and not item and not replace:
            raise RuntimeError(f'Existing file: {path}. Use --replace-discovery to back it up and replace it.')
    original = {}
    for path, (data, mode) in files.items():
        previous = path.read_bytes() if path.exists() else None
        previous_mode = path.stat().st_mode & 0o777 if path.exists() else mode
        original[path] = (previous, previous_mode)
        if str(path) not in state['files']:
            state['files'][str(path)] = {'original': base64.b64encode(previous).decode() if previous is not None else None,
                                        'mode': previous_mode}
        state['files'][str(path)]['installed'] = fingerprint(data)
    try:
        for path, (data, mode) in files.items():
            write(path, data, mode)
        state['compatibility'] = compatibility
        write(STATE, json.dumps(state, indent=2).encode(), 0o600)
    except Exception:
        for path, (data, mode) in original.items():
            if data is None:
                path.unlink(missing_ok=True)
            else:
                write(path, data, mode)
        raise
    print('Installed Rich notifications for KDE Connect ' + compatibility['kdeconnect'])
    print('Restart KDE Connect, then use the enable command for the paired device.')


def call(device, method, *args):
    if not re.fullmatch(r'[A-Za-z0-9_-]+', device):
        raise RuntimeError('Invalid device ID')
    return run('busctl', '--user', 'call', 'org.kde.kdeconnect',
               '/modules/kdeconnect/devices/' + device, 'org.kde.kdeconnect.device', method, *args)


def enable(device):
    if not STATE.exists():
        raise RuntimeError('Install the plugin first')
    # loadedPlugins confirms that enabling really loaded a compatible plugin.
    stock = call(device, 'isPluginEnabled', 's', STOCK) == 'b true'
    rich = call(device, 'isPluginEnabled', 's', PLUGIN) == 'b true'
    state = json.loads(STATE.read_text())
    state['devices'].setdefault(device, {'stock': stock, 'rich': rich})
    write(STATE, json.dumps(state, indent=2).encode(), 0o600)
    try:
        call(device, 'setPluginEnabled', 'sb', STOCK, 'false')
        call(device, 'setPluginEnabled', 'sb', PLUGIN, 'true')
        if PLUGIN not in shlex.split(call(device, 'loadedPlugins')):
            raise RuntimeError('Plugin did not load. Check device connectivity, restart KDE Connect, and verify versions.')
    except Exception:
        call(device, 'setPluginEnabled', 'sb', PLUGIN, str(rich).lower())
        call(device, 'setPluginEnabled', 'sb', STOCK, str(stock).lower())
        raise
    print('Rich notifications enabled; stock sender disabled for ' + device)


def restore_device(device, previous):
    call(device, 'setPluginEnabled', 'sb', PLUGIN, str(previous['rich']).lower())
    call(device, 'setPluginEnabled', 'sb', STOCK, str(previous['stock']).lower())


def uninstall():
    state = json.loads(STATE.read_text())
    # Check all managed files before modifying anything.
    for filename, item in state['files'].items():
        path = Path(filename)
        if path.exists() and fingerprint(path.read_bytes()) != item['installed']:
            raise RuntimeError('Refusing to replace a modified file: ' + filename)
    for device, previous in state['devices'].items():
        restore_device(device, previous)
    for filename, item in state['files'].items():
        path = Path(filename)
        if item['original'] is None:
            path.unlink(missing_ok=True)
        else:
            write(path, base64.b64decode(item['original']), item['mode'])
    STATE.unlink()
    print('Uninstalled; previous files and per-device sender settings restored. Restart KDE Connect.')


def restart():
    run('busctl', '--user', 'call', 'org.freedesktop.DBus', '/org/freedesktop/DBus',
        'org.freedesktop.DBus', 'ReloadConfig')
    # Only stop the process currently owning the user's KDE Connect D-Bus name.
    try:
        result = run('busctl', '--user', 'call', 'org.freedesktop.DBus', '/org/freedesktop/DBus',
                     'org.freedesktop.DBus', 'GetConnectionUnixProcessID', 's', 'org.kde.kdeconnect')
        pid = int(result.split()[-1])
        os.kill(pid, signal.SIGTERM)
        for _ in range(50):
            if not Path('/proc/' + str(pid)).exists():
                break
            time.sleep(0.1)
        else:
            raise RuntimeError('KDE Connect did not stop; restart it manually')
    except subprocess.CalledProcessError:
        pass  # It was not running.
    launcher = Path.home() / '.local/libexec/kdeconnect-rich-notifications'
    executable = str(launcher) if launcher.exists() else shutil.which('kdeconnectd')
    if not executable:
        raise RuntimeError('Cannot find kdeconnectd')
    log = STATE.parent / 'daemon.log'
    log.parent.mkdir(parents=True, exist_ok=True)
    with log.open('ab') as output:
        subprocess.Popen([executable], stdin=subprocess.DEVNULL, stdout=output,
                         stderr=output, start_new_session=True)
    print('KDE Connect restarted; allow a few seconds for devices to reconnect.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    setup = commands.add_parser('install')
    setup.add_argument('build', type=Path)
    setup.add_argument('--replace-discovery', action='store_true', help='Back up existing launcher/autostart files')
    commands.add_parser('restart')
    activate = commands.add_parser('enable')
    activate.add_argument('device')
    deactivate = commands.add_parser('disable')
    deactivate.add_argument('device')
    commands.add_parser('uninstall')
    args = parser.parse_args()
    try:
        if args.command == 'install':
            install(args.build.resolve(), args.replace_discovery)
        elif args.command == 'enable':
            enable(args.device)
        elif args.command == 'disable':
            state = json.loads(STATE.read_text())
            previous = state['devices'].pop(args.device)
            restore_device(args.device, previous)
            write(STATE, json.dumps(state, indent=2).encode(), 0o600)
            print('Previous sender settings restored')
        elif args.command == 'restart':
            restart()
        else:
            uninstall()
    except (OSError, RuntimeError, KeyError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, str(error) + '\n')


if __name__ == '__main__':
    main()
