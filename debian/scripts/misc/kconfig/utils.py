# -*- mode: python -*-
# Misc helpers for Kconfig and annotations
# Copyright © 2023 Canonical Ltd.

import sys


def autodetect_annotations():
    return "debian/config/annotations"

def arg_fail(parser, message, show_usage=True):
    print(message)
    if show_usage:
        parser.print_usage()
    sys.exit(1)
