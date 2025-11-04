# Copyright (c) 2024 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

#
# THIS FILE IS AUTO-GENERATED. DO NOT EDIT BY HAND.
#
# Use the following command to update version information:
# % python3 ./tools/bazel/version_updater.py --version=<version> [--channel=<channel>] [--url=<url>]
#
# Specify a fully-qualified CEF version. Optionally override the default
# download URL.
#
# CEF binary distribution file names are expected to take the form
# "cef_binary_<version>_<platform>[_<channel>].tar.bz2". These files must
# exist for each supported platform at the download URL location. Sha256
# hashes must also exist for each file at "<file_name>.sha256".
#

CEF_DOWNLOAD_URL = "https://cef-builds.spotifycdn.com/"

CEF_VERSION = "139.0.28+g55ab8a8+chromium-139.0.7258.139"
CEF_CHANNEL = ""

# Map of supported platform to sha256 for binary distribution download.
CEF_FILE_SHA256 = {
    "windows32": "ec929e977f6fca56e27eb5e5b9849b40c2e9b9f4cc529ec7ecaa464c939f6b00",
    "windows64": "e8acfd42243c253ce36347182b124a742a839e945eef4b3ec0f525753f5a0ffb",
    "windowsarm64": "b9ee959335af1e4dd37e0942eeb18730dd3a54860b39fb1378a0607ddc31224d",
    "macosx64": "4edbf4187f15b239d3359431521ca077f9810a416f8db947c525243734467b22",
    "macosarm64": "990443c394fc501eedc6792c98de93f8fc2df617493ac3def5ef57b4dadf70f1",
    "linux64": "62289e373a093699a3eaaddf37679586426cc8ad638d099c21b2096c29a311d5",
    "linuxarm64": "256e4a1420728a81794e06351ed0e0226c938134eb2fad6e83dc889b7142d379",
    "linuxarm": "b4abb5475e32ab7e77b2d4290d00cb7ac1805f345b0ca90ccb7a46becf17c9f1",
}
