# SPDX-License-Identifier: GPL-3.0-only
"""Filesystem installation is reversible and rejects accidental overwrites."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('manager', Path(__file__).parents[1] / 'scripts/manage.py')
manager = importlib.util.module_from_spec(spec)
spec.loader.exec_module(manager)


class InstallationTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.home = Path(self.temporary.name)
        self.build = self.home / 'build'
        self.build.mkdir()
        (self.build / 'compatibility.json').write_text(json.dumps({'kdeconnect': '26.08.3', 'qt': '6.12.0'}))
        (self.build / (manager.PLUGIN + '.so')).write_bytes(b'fixture')
        self.state = self.home / 'state/install.json'
        self.addCleanup(patch.stopall)
        patch.object(manager, 'STATE', self.state).start()
        patch.object(Path, 'home', return_value=self.home).start()
        patch.object(manager.shutil, 'which', return_value='/usr/bin/kdeconnectd').start()
        patch.object(manager, 'run', side_effect=lambda *args: 'kdeconnect-cli 26.08.3' if args[0] == 'kdeconnect-cli' else 'resolved').start()

    def test_install_upgrade_and_restore_existing_discovery(self):
        discovery = self.home / '.local/share/dbus-1/services/org.kde.kdeconnect.service'
        discovery.parent.mkdir(parents=True)
        discovery.write_bytes(b'original launcher')
        with self.assertRaises(RuntimeError):
            manager.install(self.build)
        manager.install(self.build, replace=True)
        self.assertNotEqual(discovery.read_bytes(), b'original launcher')
        manager.install(self.build)  # Upgrade retains the original backup.
        manager.uninstall()
        self.assertEqual(discovery.read_bytes(), b'original launcher')
        self.assertFalse((self.home / '.local/lib/qt6/plugins/kdeconnect' / (manager.PLUGIN + '.so')).exists())
        self.assertFalse(self.state.exists())

    def test_uninstall_preserves_user_edits(self):
        manager.install(self.build)
        launcher = self.home / '.local/libexec/kdeconnect-rich-notifications'
        launcher.write_text('user change')
        with self.assertRaises(RuntimeError):
            manager.uninstall()
        self.assertEqual(launcher.read_text(), 'user change')
        self.assertTrue(self.state.exists())

    def test_version_mismatch_does_not_install(self):
        (self.build / 'compatibility.json').write_text('{"kdeconnect":"99.0.0"}')
        with self.assertRaises(RuntimeError):
            manager.install(self.build)
        self.assertFalse(self.state.exists())

    def test_failed_activation_restores_sender_settings(self):
        manager.install(self.build)
        calls = []
        def dbus(device, method, *args):
            calls.append((method, args))
            if method == 'isPluginEnabled':
                return 'b true' if args[-1] == manager.STOCK else 'b false'
            if method == 'loadedPlugins':
                return 'as 0'
            return ''
        with patch.object(manager, 'call', side_effect=dbus):
            with self.assertRaises(RuntimeError):
                manager.enable('paired-device')
        self.assertIn(('setPluginEnabled', ('sb', manager.STOCK, 'true')), calls)
        self.assertIn(('setPluginEnabled', ('sb', manager.PLUGIN, 'false')), calls)


if __name__ == '__main__':
    unittest.main()
