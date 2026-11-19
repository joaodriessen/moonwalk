import configparser
import importlib.util
from pathlib import Path
import sys
import unittest

SCRIPTS = Path(__file__).resolve().parents[2] / "scripts"
sys.path.insert(0, str(SCRIPTS))
SPEC = importlib.util.spec_from_file_location("verify_flatpak_bundle", SCRIPTS / "verify-flatpak-bundle.py")
verifier = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(verifier)


def metadata(context, bus="", extra=""):
    document = configparser.ConfigParser(interpolation=None)
    document.optionxform = str
    document.read_string("[Application]\nruntime=org.kde.Platform/x86_64/6.10\n"
                         f"[Context]\n{context}\n[Session Bus Policy]\n{bus}\n{extra}")
    return document


class FlatpakBundlePermissionsTest(unittest.TestCase):
    def test_old_fallback_x11_and_new_kde_defaults_have_the_same_checked_policy(self):
        older = metadata("sockets=x11;wayland;pulseaudio;fallback-x11;\nshared=network;ipc;")
        newer = metadata("sockets=fallback-x11;pulseaudio;wayland;\nshared=ipc;network;\n"
                         "filesystems=xdg-config/kdeglobals:ro;",
                         "com.canonical.AppMenu.Registrar=talk\norg.kde.kconfig.notify=talk\n"
                         "org.kde.KGlobalSettings=talk")
        self.assertEqual(verifier.sandbox_policy(older), verifier.sandbox_policy(newer))

    def test_additional_grants_and_environment_changes_are_detectable(self):
        expected = verifier.sandbox_policy(metadata("devices=dri;"))
        for changed in (
            metadata("devices=dri;\nfilesystems=home;"),
            metadata("devices=dri;\nfilesystems=xdg-config/kdeglobals;"),
            metadata("devices=dri;", "org.kde.KGlobalSettings=own"),
            metadata("devices=all;"),
            metadata("devices=dri;", extra="[Environment]\nLD_PRELOAD=extra.so"),
            metadata("devices=dri;", extra="[Policy org.example]\ngrants=anything;"),
        ):
            with self.subTest(metadata=dict(changed["Context"])):
                self.assertNotEqual(expected, verifier.sandbox_policy(changed))
