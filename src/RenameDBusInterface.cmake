# SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later

# cmake -DIN=<xml> -DOUT=<xml> -DFROM=<interface> -DTO=<interface> -P RenameDBusInterface.cmake
file(READ "${IN}" xml)
string(REPLACE "name=\"${FROM}\"" "name=\"${TO}\"" renamed "${xml}")
if(renamed STREQUAL xml)
    message(FATAL_ERROR "${IN} has no interface ${FROM}")
endif()
file(WRITE "${OUT}" "${renamed}")
