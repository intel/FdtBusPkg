## @file
#  Build the DTB for FdtBusDxe unit tests, and process it into a form
#  that can be consumed by the C compiler.
#
#  Copyright (C) 2023, Intel Corporation. All rights reserved.<BR>
#
#  SPDX -License-Identifier: BSD-2-Clause-Patent
#
##

set -e

basedir=$(dirname -- "$0")

cleanup () {
  rm -f *.dtb *.dtbi
}

trap cleanup EXIT

(
  cd ${basedir}
  dtc -I dts -O dtb -o TestDt.dtb TestDt.dts
  xxd -i TestDt.dtb TestDt.dtbi
  rm TestDt.dtb
  unix2dos TestDt.dts
  unix2dos TestDt.dtbi
)
